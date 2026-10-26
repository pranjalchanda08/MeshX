/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file esp_platform_bt.c
 * @brief This file contains the initialization code for BLE Mesh functionality
 *        on the ESP32 platform. It supports both Bluedroid and NimBLE BLE stacks.
 *        The file includes functions to initialize the Bluetooth controller,
 *        configure the BLE stack, and retrieve the device UUID for provisioning.
 *
 * @author Pranjal Chanda
 *
 */

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
#include "interface/meshx_platform.h"
#include "interface/logging/meshx_log.h"
#if defined(CONFIG_BT_BLE_50_FEATURES_SUPPORTED) && !defined(BLE_50_FEATURE_SUPPORT)
#define BLE_50_FEATURE_SUPPORT TRUE
#endif
#include "esp_gap_ble_api.h"

/* Extended Advertising parameters for BLE beacon broadcasting */
#define MESHX_EXT_ADV_INTERVAL_MIN          0x00A0   /**< Extended advertising interval min (~100ms) */
#define MESHX_EXT_ADV_INTERVAL_MAX          0x00A0   /**< Extended advertising interval max (~100ms) */
#define MESHX_EXT_ADV_CHANNEL_MAP           0x07     /**< Advertising channels 37, 38, 39 */
#define MESHX_EXT_ADV_TX_POWER              0x7F     /**< Extended advertising TX power */
#define MESHX_EXT_ADV_DURATION_10MS         100U     /**< Extended advertising duration in 10ms units (1000ms) */

#define TAG "EXAMPLE_INIT"

#ifdef CONFIG_BT_BLUEDROID_ENABLED
void ble_mesh_get_dev_uuid(uint8_t *dev_uuid)
{
    if (dev_uuid == NULL) {
        ESP_LOGE(TAG, "%s, Invalid device uuid", __func__);
        return;
    }

    /* Copy device address to the device uuid with offset equals to 2 here. */
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

    /* Copy device address to the device uuid with offset equals to 2 here. */
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

#if !defined(CONFIG_BT_BLE_50_FEATURES_SUPPORTED) || !defined(CONFIG_BT_BLE_50_EXTEND_ADV_EN)
#error "BLE 5.0 Extended Advertising (CONFIG_BT_BLE_50_EXTEND_ADV_EN) must be enabled for meshx_plat_ble_adv_mfg_data to broadcast full-length BLE advertising packets without truncation"
#endif

meshx_err_t meshx_plat_ble_adv_mfg_data(const uint8_t *mfg_data, uint16_t mfg_len)
{
    if (!mfg_data || mfg_len == 0)
    {
        return MESHX_INVALID_ARG;
    }

    static uint8_t s_reheal_adv_instance = 0;
    static bool s_ext_adv_configured = false;

    if (!s_ext_adv_configured)
    {
        esp_ble_gap_ext_adv_params_t ext_adv_params = {
            .type = ESP_BLE_GAP_SET_EXT_ADV_PROP_NONCONN_NONSCANNABLE_UNDIRECTED,
            .interval_min = MESHX_EXT_ADV_INTERVAL_MIN,
            .interval_max = MESHX_EXT_ADV_INTERVAL_MAX,
            .channel_map = MESHX_EXT_ADV_CHANNEL_MAP,
            .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
            .peer_addr_type = BLE_ADDR_TYPE_PUBLIC,
            .filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
            .tx_power = MESHX_EXT_ADV_TX_POWER,
            .primary_phy = ESP_BLE_GAP_PHY_1M,
            .max_skip = 0,
            .secondary_phy = ESP_BLE_GAP_PHY_1M,
            .sid = 1,
            .scan_req_notif = false,
        };
        esp_err_t err = esp_ble_gap_ext_adv_set_params(s_reheal_adv_instance, &ext_adv_params);
        if (err != ESP_OK)
        {
            MESHX_LOGE(MODULE_ID_COMMON, "esp_ble_gap_ext_adv_set_params failed: %d", err);
            return MESHX_FAIL;
        }
        s_ext_adv_configured = true;
    }

    esp_err_t err = esp_ble_gap_config_ext_adv_data_raw(s_reheal_adv_instance, mfg_len, mfg_data);
    if (err != ESP_OK)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "esp_ble_gap_config_ext_adv_data_raw failed: %d", err);
        return MESHX_FAIL;
    }

    esp_ble_gap_ext_adv_t ext_adv = {
        .instance = s_reheal_adv_instance,
        .duration = MESHX_EXT_ADV_DURATION_10MS,
        .max_events = 0,
    };
    err = esp_ble_gap_ext_adv_start(1, &ext_adv);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "esp_ble_gap_ext_adv_start failed: %d", err);
        return MESHX_FAIL;
    }

    MESHX_LOGI(MODULE_ID_COMMON, "plat_ble_adv_mfg_data: Advertised via Ext Adv (len=%u)",
               (unsigned int)mfg_len);
    return MESHX_SUCCESS;
}
