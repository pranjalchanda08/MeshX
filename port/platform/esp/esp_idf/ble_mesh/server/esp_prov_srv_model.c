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
