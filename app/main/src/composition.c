/**
 * @file composition.c
 * @brief Contains the logic to initialize and manage BLE Mesh element compositions.
 *
 * @author [Pranjal Chanda]
 */

#include <app_common.h>

#if CONFIG_RELAY_SERVER_COUNT
#include "relay_server_model.h"
#endif /* CONFIG_RELAY_SERVER_COUNT */

#if CONFIG_RELAY_CLIENT_COUNT
#include "relay_client_model.h"
#endif /* CONFIG_RELAY_CLIENT_COUNT */

#if CONFIG_LIGHT_CWWW_SRV_COUNT
#include "cwww_server_model.h"
#endif /* CONFIG_LIGHT_CWWW_SRV_COUNT */

#if CONFIG_LIGHT_CWWW_CLIENT_COUNT
#include "light_cwww_client.h"
#endif /* CONFIG_LIGHT_CWWW_CLIENT_COUNT */

/**
 * @brief Prints error message and returns if an error occurs.
 */
#define ESP_ERR_PRINT_RET(_e_str, _err)            \
    if (_err != ESP_OK)                            \
    {                                              \
        ESP_LOGE(TAG, _e_str " (err 0x%x)", _err); \
        return _err;                               \
    }

/**
 * @brief Creates the BLE Mesh element composition.
 *
 * Initializes the BLE Mesh elements for different configurations like relay servers,
 * relay clients, and CWWW (Cool White and Warm White) servers.
 *
 * @param[in] p_dev Pointer to the device structure containing element information.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
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
    ESP_ERR_PRINT_RET("Failed to initialize BLE Relay Client Elements", err);
#endif /* CONFIG_RELAY_CLIENT_COUNT */

#if CONFIG_LIGHT_CWWW_SRV_COUNT
    err = create_cwww_elements(p_dev);
    ESP_ERR_PRINT_RET("Failed to initialize CWWW Elements", err);
#endif /* CONFIG_LIGHT_CWWW_SRV_COUNT */

#if CONFIG_LIGHT_CWWW_CLIENT_COUNT
    err = create_cwww_client_elements(p_dev);
    ESP_ERR_PRINT_RET("Failed to initialize CWWW Client Elements", err);
#endif /* CONFIG_LIGHT_CWWW_CLIENT_COUNT */

    ARG_UNUSED(err);
    ARG_UNUSED(p_dev);

#endif /* CONFIG_MAX_ELEMENT_COUNT */
    return ESP_OK;
}
