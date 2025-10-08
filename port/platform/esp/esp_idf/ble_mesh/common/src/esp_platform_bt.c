/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file ble_mesh_plat_init.c
 * @brief This file contains the initialization code for BLE Mesh functionality
 *        on the ESP32 platform. It supports both Bluedroid and NimBLE BLE stacks.
 *        The file includes functions to initialize the Bluetooth controller,
 *        configure the BLE stack, and retrieve the device UUID for provisioning.
 *
 * @author Pranjal Chanda
 *
 */

#include <stdio.h>
#include <string.h>
#include <sdkconfig.h>

#ifdef CONFIG_BT_BLUEDROID_ENABLED
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#endif

#ifdef CONFIG_BT_NIMBLE_ENABLED
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#endif

#include "esp_ble_mesh_defs.h"

#define TAG "EXAMPLE_INIT"

#ifdef CONFIG_BT_BLUEDROID_ENABLED
void ble_mesh_get_dev_uuid(uint8_t *dev_uuid)
{
    if (dev_uuid == NULL) {
        ESP_LOGE(TAG, "%s, Invalid device uuid", __func__);
        return;
    }

    /* Copy device address to the device uuid with offset equals to 2 here.
     * The first two bytes is used for matching device uuid by Provisioner.
     * And using device address here is to avoid using the same device uuid
     * by different unprovisioned devices.
     */
    memcpy(dev_uuid + 2, esp_bt_dev_get_address(), BD_ADDR_LEN);
}

/**
 * @brief Initializes the Bluetooth controller for the ESP32 platform.
 *
 * This function initializes the Bluetooth controller and enables BLE mode.
 * It is used to set up the Bluetooth stack for BLE Mesh functionality.
 *
 * @return
 *     - ESP_OK on success.
 *     - Error code on failure.
 */
esp_err_t bluetooth_init(void)
{
    esp_err_t ret;

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "%s initialize controller failed", __func__);
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "%s enable controller failed", __func__);
        return ret;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "%s init bluetooth failed", __func__);
        return ret;
    }
    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "%s enable bluetooth failed", __func__);
        return ret;
    }

    return ret;
}
#endif /* CONFIG_BT_BLUEDROID_ENABLED */

#ifdef CONFIG_BT_NIMBLE_ENABLED
static SemaphoreHandle_t mesh_sem;
static uint8_t own_addr_type;
void ble_store_config_init(void);
static uint8_t addr_val[6] = {0};

/**
 * @brief Retrieves the device UUID for BLE Mesh provisioning.
 *
 * This function copies the device address into the provided UUID buffer,
 * which is used for matching by the Provisioner during the provisioning process.
 *
 * @param dev_uuid Pointer to the buffer where the device UUID will be stored.
 */
void ble_mesh_get_dev_uuid(uint8_t *dev_uuid)
{
    if (dev_uuid == NULL) {
        ESP_LOGE(TAG, "%s, Invalid device uuid", __func__);
        return;
    }

    /* Copy device address to the device uuid with offset equals to 2 here.
     * The first two bytes is used for matching device uuid by Provisioner.
     * And using device address here is to avoid using the same device uuid
     * by different unprovisioned devices.
     */
    memcpy(dev_uuid + 2, addr_val, BD_ADDR_LEN);
}

/**
 * @brief Callback function for BLE host reset event.
 *
 * This function is called when the BLE host is reset. It can be used to handle
 * any necessary cleanup or state reset operations.
 *
 * @param reason The reason for the reset.
 */
static void mesh_on_reset(int reason)
{
    ESP_LOGI(TAG, "Resetting state; reason=%d", reason);
}

/**
 * @brief Callback function for BLE host synchronization event.
 *
 * This function is called when the BLE host is synchronized. It ensures that
 * the BLE address is set and gives a semaphore to indicate that the host is ready.
 */
static void mesh_on_sync(void)
{
    int rc;

    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    /* Figure out address to use while advertising (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGI(TAG, "error determining address type; rc=%d", rc);
        return;
    }

    rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);

    xSemaphoreGive(mesh_sem);
}

/**
 * @brief Task to run the NimBLE host.
 *
 * This function runs the NimBLE host in a FreeRTOS task. It will block until
 * nimble_port_stop() is called, at which point it will exit.
 *
 * @param param Pointer to task parameters (not used).
 */
void mesh_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}

/**
 * @brief Initializes the NimBLE Bluetooth stack.
 *
 * This function initializes the NimBLE Bluetooth stack and starts the host task.
 * It also creates a semaphore to synchronize the initialization process.
 *
 * @return
 *     - MESHX_SUCCESS on success.
 *     - Error code on failure.
 */
esp_err_t bluetooth_init(void)
{
    esp_err_t ret;

    mesh_sem = xSemaphoreCreateBinary();
    if (mesh_sem == NULL) {
        ESP_LOGE(TAG, "Failed to create mesh semaphore");
        return MESHX_FAIL;
    }

    ret = nimble_port_init();
    if (ret != MESHX_SUCCESS) {
        ESP_LOGE(TAG, "Failed to init nimble %d ", ret);
        return ret;
    }

    /* Initialize the NimBLE host configuration. */
    ble_hs_cfg.reset_cb = mesh_on_reset;
    ble_hs_cfg.sync_cb = mesh_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* XXX Need to have template for store */
    ble_store_config_init();

    nimble_port_freertos_init(mesh_host_task);

    xSemaphoreTake(mesh_sem, portMAX_DELAY);

    return MESHX_SUCCESS;
}
#endif /* CONFIG_BT_NIMBLE_ENABLED */
