/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file esp_prov_srv_model.c
 * @brief Implementation of BLE Mesh provisioning server model for ESP32.
 *        This file contains the provisioning callback handling, control task
 *        message mapping, and provisioning parameter initialization.
 *        It facilitates the provisioning process and event handling for BLE Mesh.
 *
 * @author Pranjal Chanda
 *
 */

#include "interface/ble_mesh/server/meshx_ble_mesh_prov_srv.h"

/**
 * @brief Global provisioning structure.
 *
 * This structure holds the global provisioning configuration.
 */
static MESHX_PROV g_meshx_prov;

/**
 * @brief Callback function for BLE Mesh provisioning events.
 *
 * This function is called whenever a BLE Mesh provisioning event occurs.
 *
 * @param event The provisioning event type.
 * @param param Pointer to the provisioning event parameters.
 */
static void meshx_provisioning_cb(MESHX_PROV_CB_EVT event,
                                  const MESHX_PROV_CB_PARAM *param)
{
    MESHX_LOGD(MODULE_ID_MODEL_SERVER, "Event 0x%02x", event);

    meshx_prov_srv_param_t prov_srv_param;

    memcpy(&prov_srv_param.param, param, sizeof(meshx_prov_cb_param_t));
    prov_srv_param.prov_evt = event;

    if(MESHX_SUCCESS != meshx_prov_srv_notify_plat_event(&prov_srv_param))
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to notify model event");
    }
}

/**
 * @brief Initialize provisioning parameters.
 *
 * This function initializes the provisioning parameters by copying the UUID from the provided
 * server configuration and registering the provisioning callback.
 *
 * @param uuid Pointer to the UUID of the device.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_FAIL: Failed to register provisioning callback
 */
meshx_err_t meshx_plat_init_prov(const uint8_t *uuid)
{
    if (!uuid)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid server configuration");
        return MESHX_INVALID_ARG;
    }
    g_meshx_prov.uuid = uuid;

    return esp_ble_mesh_register_prov_callback((esp_ble_mesh_prov_cb_t)meshx_provisioning_cb);
}

/**
 * @brief Get the provisioning parameters.
 *
 * This function returns a pointer to the global provisioning parameters.
 *
 * @return Pointer to the global provisioning parameters.
 */
meshx_ptr_t meshx_plat_get_prov(void)
{
    return (meshx_ptr_t)&g_meshx_prov;
}

/**
 * @brief Enable provisioning bearers on the platform.
 *
 * Configures and enables the requested BLE Mesh provisioning bearers
 * (PB-ADV and/or PB-GATT) using ESP-IDF.
 *
 * @param[in] bearers Bitmask of provisioning bearers (MESHX_PROV_ADV, MESHX_PROV_GATT).
 *
 * @return
 *    - MESHX_SUCCESS: Provisioning bearers enabled successfully
 *    - MESHX_FAIL: Failed to enable provisioning bearers
 */
meshx_err_t meshx_plat_prov_enable(meshx_prov_bearer_t bearers)
{
    esp_ble_mesh_prov_bearer_t esp_bearers = (esp_ble_mesh_prov_bearer_t)0;
    if (bearers & MESHX_PROV_ADV)
    {
        esp_bearers = (esp_ble_mesh_prov_bearer_t)(esp_bearers | ESP_BLE_MESH_PROV_ADV);
    }
    if (bearers & MESHX_PROV_GATT)
    {
        esp_bearers = (esp_ble_mesh_prov_bearer_t)(esp_bearers | ESP_BLE_MESH_PROV_GATT);
    }
    esp_err_t err = esp_ble_mesh_node_prov_enable(esp_bearers);
    return (err == ESP_OK) ? MESHX_SUCCESS : MESHX_FAIL;
}

/**
 * @brief Disable provisioning bearers on the platform.
 *
 * Disables the active BLE Mesh provisioning bearers (PB-ADV and/or PB-GATT)
 * using ESP-IDF.
 *
 * @param[in] bearers Bitmask of provisioning bearers (MESHX_PROV_ADV, MESHX_PROV_GATT).
 *
 * @return
 *    - MESHX_SUCCESS: Provisioning bearers disabled successfully
 *    - MESHX_FAIL: Failed to disable provisioning bearers
 */
meshx_err_t meshx_plat_prov_disable(meshx_prov_bearer_t bearers)
{
    esp_ble_mesh_prov_bearer_t esp_bearers = (esp_ble_mesh_prov_bearer_t)0;
    if (bearers & MESHX_PROV_ADV)
    {
        esp_bearers = (esp_ble_mesh_prov_bearer_t)(esp_bearers | ESP_BLE_MESH_PROV_ADV);
    }
    if (bearers & MESHX_PROV_GATT)
    {
        esp_bearers = (esp_ble_mesh_prov_bearer_t)(esp_bearers | ESP_BLE_MESH_PROV_GATT);
    }
    esp_err_t err = esp_ble_mesh_node_prov_disable(esp_bearers);
    return (err == ESP_OK) ? MESHX_SUCCESS : MESHX_FAIL;
}

/**
 * @brief Set the unprovisioned device name on the platform.
 *
 * Configures the advertised unprovisioned device name using ESP-IDF.
 *
 * @param[in] name Null-terminated device name string.
 *
 * @return
 *    - MESHX_SUCCESS: Device name configured successfully
 *    - MESHX_INVALID_ARG: Name pointer is NULL
 *    - MESHX_FAIL: Platform failed to configure device name
 */
meshx_err_t meshx_plat_prov_set_device_name(const char *name)
{
    if (name == NULL)
    {
        return MESHX_INVALID_ARG;
    }
    esp_err_t err = esp_ble_mesh_set_unprovisioned_device_name(name);
    return (err == ESP_OK) ? MESHX_SUCCESS : MESHX_FAIL;
}

/**
 * @brief Provide numeric authentication input to the provisioning stack.
 *
 * Submits a numeric authentication value entered by the user to the
 * ESP-IDF provisioning stack.
 *
 * @param[in] val Numeric authentication value.
 *
 * @return
 *    - MESHX_SUCCESS: Input accepted by the provisioning stack
 *    - MESHX_FAIL: Failed to submit input
 */
meshx_err_t meshx_plat_prov_input_number(uint32_t val)
{
    esp_err_t err = esp_ble_mesh_node_input_number(val);
    return (err == ESP_OK) ? MESHX_SUCCESS : MESHX_FAIL;
}

/**
 * @brief Provide alphanumeric string authentication input to the provisioning stack.
 *
 * Submits an alphanumeric authentication string entered by the user to the
 * ESP-IDF provisioning stack.
 *
 * @param[in] str Null-terminated alphanumeric string.
 *
 * @return
 *    - MESHX_SUCCESS: Input accepted by the provisioning stack
 *    - MESHX_INVALID_ARG: String pointer is NULL
 *    - MESHX_FAIL: Failed to submit input
 */
meshx_err_t meshx_plat_prov_input_string(const char *str)
{
    if (str == NULL)
    {
        return MESHX_INVALID_ARG;
    }
    esp_err_t err = esp_ble_mesh_node_input_string((char *)str);
    return (err == ESP_OK) ? MESHX_SUCCESS : MESHX_FAIL;
}
