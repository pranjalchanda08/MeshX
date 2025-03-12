#include "meshx_platform.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
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
    {
        return MESHX_ERR_PLAT;
    }

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
