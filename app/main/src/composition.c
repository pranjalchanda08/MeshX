#include <app_common.h>

#if CONFIG_RELAY_SERVER_COUNT
#include <relay_server_model.h>
#endif /* CONFIG_RELAY_SERVER_COUNT */


#if CONFIG_RELAY_CLIENT_COUNT
#include <relay_client_model.h>
#endif /* CONFIG_RELAY_CLIENT_COUNT */

#define TAG "COMP"

#define ESP_ERR_PRINT_RET(_e_str, _err)            \
    if (_err != ESP_OK)                            \
    {                                              \
        ESP_LOGE(TAG, _e_str " (err 0x%x)", _err); \
        return _err;                               \
    }
    
esp_err_t create_ble_mesh_element_composition(dev_struct_t *p_dev)
{
#if CONFIG_MAX_ELEMENT_COUNT > 0
    esp_err_t err;
#if CONFIG_RELAY_SERVER_COUNT
    err = create_relay_elements(p_dev);
    ESP_ERR_PRINT_RET("Failed to initialize BLE Relay Elements", err);
#endif /* CONFIG_RELAY_SERVER_COUNT */

#if CONFIG_RELAY_CLIENT_COUNT
    err = create_relay_client_elements(p_dev);
    ESP_ERR_PRINT_RET("Failed to initialize BLE Relay Elements", err);
#endif /* CONFIG_RELAY_CLIENT_COUNT */
    ARG_UNUSED(err);
    ARG_UNUSED(p_dev);
#endif
    return ESP_OK;
}
