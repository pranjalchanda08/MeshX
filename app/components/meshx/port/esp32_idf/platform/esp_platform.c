/**
 * @file esp32_platform.c
 * @brief Platform-specific initialization and BLE Mesh setup for ESP32.
 *
 * This file contains functions to initialize the platform, Bluetooth, and BLE Mesh
 * for the ESP32 platform. It includes NVS flash initialization, Bluetooth stack
 * setup, and BLE Mesh provisioning and configuration.
 *
 * @author Pranjal Chanda
 */

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "interface/meshx_platform.h"
#include "interface/ble_mesh/meshx_ble_mesh_prov_srv.h"
#include "ble_mesh_example_init.h"

meshx_err_t meshx_platform_init(void)
{
    esp_err_t err = ESP_OK;
    /* Initialize NVS flash */
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if(err)
        return MESHX_ERR_PLAT;

    /* Set log level for BLE Mesh */
    esp_log_level_set("BLE_MESH", ESP_LOG_ERROR);

    return MESHX_SUCCESS;
}

meshx_err_t meshx_platform_bt_init(void)
{
    esp_err_t err = ESP_OK;
    /* Initialize Bluetooth */
    err = bluetooth_init();
    if(err)
    {
        return MESHX_ERR_PLAT;
    }

    return MESHX_SUCCESS;
}

meshx_err_t meshx_plat_ble_mesh_init(const meshx_prov_params_t *prov_cfg, void* comp)
{
    if(comp == NULL || prov_cfg == NULL)
        return MESHX_INVALID_ARG;

    meshx_err_t err = MESHX_SUCCESS;

    /* Initialize BLE Mesh Provisioner */
    MESHX_PROV *p_prov = NULL;
    err = meshx_plat_init_prov(prov_cfg->uuid);
    if(err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to initialize provisioning");
        return err;
    }
    p_prov = meshx_plat_get_prov();
    if(p_prov == NULL)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to get provisioning instance");
        return MESHX_ERR_PLAT;
    }
    err = esp_ble_mesh_init(p_prov, comp);
    if(err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to initialize mesh stack");
        return err;
    }
    err = esp_ble_mesh_set_unprovisioned_device_name((char*)prov_cfg->node_name);
    if(err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to set device name");
        return err;
    }
    err = esp_ble_mesh_node_prov_enable((esp_ble_mesh_prov_bearer_t)(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT));
    if(err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to enable mesh node");
        return err;
    }
    MESHX_LOGI(MODULE_ID_MODEL_SERVER, "BLE Mesh Node initialized");
    return MESHX_SUCCESS;
}
