/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file esp_platform.c
 * @brief Platform-specific initialization and BLE Mesh provisioning for ESP32.
 *        This file contains functions to initialize the platform, Bluetooth,
 *        and BLE Mesh stack for the MeshX framework.
 *
 * @author Pranjal Chanda
 *
 */

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "interface/meshx_platform.h"
#include "interface/ble_mesh/server/meshx_ble_mesh_prov_srv.h"

/**
 * @brief Initializes the MeshX platform for the ESP32.
 *
 * This function sets up the necessary components and configurations
 * required for the MeshX platform to operate on the ESP32. It ensures
 * that the platform is ready for use by other components of the MeshX
 * system.
 *
 * @return
 *     - MESHX_OK on successful initialization.
 *     - Appropriate error code of type meshx_err_t on failure.
 */
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

/**
 * @brief Resets the MeshX platform.
 * This function performs a system reset, restarting the platform.
 */
void meshx_platform_reset(void)
{
    esp_restart();
}
