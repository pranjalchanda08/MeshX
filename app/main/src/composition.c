/**
 * @file composition.c
 * @brief BLE Mesh Composition Initialization and Element Creation
 *
 * This file contains functions and definitions for initializing BLE Mesh composition data
 * and creating BLE Mesh elements for various configurations such as relay servers, relay clients,
 * and CWWW (Cool White and Warm White) servers.
 *
 * The file includes necessary headers and conditionally includes headers based on configuration
 * macros. It defines macros for error handling and contains static variables for provisioning
 * parameters and Light CTL state.
 *
 * Functions:
 * - get_root_models: Returns the root models for BLE Mesh elements.
 * - get_root_models_size: Returns the size of the root models.
 * - ble_mesh_composition_init: Initializes BLE Mesh composition data.
 * - create_ble_mesh_element_composition: Creates the BLE Mesh element composition.
 *
 * Configuration Macros:
 * - CONFIG_RELAY_SERVER_COUNT: Enables inclusion of relay server model.
 * - CONFIG_RELAY_CLIENT_COUNT: Enables inclusion of relay client model.
 * - CONFIG_LIGHT_CWWW_SRV_COUNT: Enables inclusion of CWWW server model.
 * - CONFIG_LIGHT_CWWW_CLIENT_COUNT: Enables inclusion of CWWW client model.
 * - CONFIG_ENABLE_PROVISIONING: Enables provisioning parameters for BLE Mesh.
 * - CONFIG_ENABLE_LIGHT_CTL_SERVER: Enables Light CTL server model.
 * - CONFIG_ENABLE_CONFIG_SERVER: Enables configuration server model.
 * - CONFIG_MAX_ELEMENT_COUNT: Defines the maximum number of elements.
 *
 * Error Handling:
 * The ESP_ERR_PRINT_RET macro is used for printing error messages and returning error codes.
 *
 * Provisioning:
 * The prod_prov_cfg structure holds provisioning parameters for BLE Mesh.
 *
 * Light CTL Server:
 * The ctl_state variable holds the Light CTL state, and the ctl_setup_server structure defines
 * the Light CTL setup server model.
 *
 * Root Models:
 * The app_root_model array holds the root models for BLE Mesh elements.
 *
 * @note This file is part of the MeshX application.
 *
 * @author Pranjal Chanda
 *
 */

#include "app_common.h"
#include "main.h"

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

#if CONFIG_ENABLE_PROVISIONING
/** Provisioning parameters for BLE Mesh. */
static prov_params_t prod_prov_cfg = {
    .cb_reg = NULL};
#endif

#if CONFIG_ENABLE_LIGHT_CTL_SERVER
/** Light CTL state. */
esp_ble_mesh_light_ctl_state_t ctl_state;
ESP_BLE_MESH_MODEL_PUB_DEFINE(ctl_setup_pub, 16, ROLE_NODE);

/** Light CTL setup server model. */
static esp_ble_mesh_light_ctl_setup_srv_t ctl_setup_server = {
    .rsp_ctrl = {
        .get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
        .set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    },
    .state = &ctl_state,
};
#endif

/** Root models for BLE Mesh elements. */
static esp_ble_mesh_model_t app_root_model[] = {
#if CONFIG_ENABLE_CONFIG_SERVER
    ESP_BLE_MESH_MODEL_CFG_SRV(&PROD_CONFIG_SERVER_INSTANCE),
#endif /* CONFIG_ENABLE_CONFIG_SERVER */
#if CONFIG_ENABLE_LIGHT_CTL_SERVER
    ESP_BLE_MESH_MODEL_LIGHT_CTL_SETUP_SRV(&ctl_setup_pub, &ctl_setup_server),
#endif
};

#if CONFIG_ENABLE_PROVISIONING
void app_prod_prov_cb(const esp_ble_mesh_prov_cb_param_t *param, prod_prov_evt_t evt)
{
    ESP_UNUSED(param);
    ESP_UNUSED(evt);
    return;
}
#endif

/**
 * @brief Returns the root models for BLE Mesh elements.
 *
 * @return Pointer to the root models.
 */
esp_ble_mesh_model_t * get_root_models(void)
{
    return app_root_model;
}

/**
 * @brief Returns the size of the root models.
 *
 * @return Size of the root models.
 */
size_t get_root_models_size(void)
{
    return ARRAY_SIZE(app_root_model);
}
/**
 * @brief Initializes BLE Mesh composition data.
 *
 * @param[in] p_dev Pointer to the device structure.
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t ble_mesh_composition_init(dev_struct_t *p_dev)
{
    if (!p_dev)
        return ESP_ERR_INVALID_STATE;

    p_dev->composition.cid = CID_ESP;
    p_dev->composition.pid = CONFIG_PID_ID;
    p_dev->composition.element_count = p_dev->element_idx;
    p_dev->composition.elements = p_dev->elements;

    return ESP_OK;
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

    ble_mesh_get_dev_uuid(prod_prov_cfg.uuid);

    err = prod_init_prov(&prod_prov_cfg);
    ESP_ERR_PRINT_RET("Failed to initialize Prov server", err);

    err = prod_init_config_server();
    ESP_ERR_PRINT_RET("Failed to initialize config server", err);

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
