/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file esp_cfg_srv_model.c
 * @brief Implementation of the BLE Mesh Configuration Server for the MeshX platform.
 *        This file contains the initialization, callback handling, and utility functions
 *        for managing the BLE Mesh Configuration Server model.
 *
 * The Configuration Server is responsible for handling configuration messages
 * such as adding keys, setting publication parameters, and managing subscriptions.
 * It provides an interface for the application to interact with the BLE Mesh stack.
 *
 * @author Pranjal Chanda
 */

#include "interface/ble_mesh/server/meshx_ble_mesh_config_srv.h"

/* Global variable for Configuration Server parameters */
static MESHX_CFG_SRV meshx_config_server_instance = {
    /* 3 transmissions with 20ms interval */
    .net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .relay = ESP_BLE_MESH_RELAY_ENABLED,
    .relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .beacon = ESP_BLE_MESH_BEACON_ENABLED,
#if defined(CONFIG_BLE_MESH_GATT_PROXY_SERVER)
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_ENABLED,
#else
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_NOT_SUPPORTED,
#endif
#if defined(CONFIG_BLE_MESH_FRIEND)
    .friend_state = ESP_BLE_MESH_FRIEND_ENABLED,
#else
    .friend_state = ESP_BLE_MESH_FRIEND_NOT_SUPPORTED,
#endif
    .default_ttl = 7,
};

static MESHX_MODEL meshx_config_server_model = {
    .model_id = MESHX_MODEL_ID_CONFIG_SRV,
    .user_data = &meshx_config_server_instance,
    .keys = ESP_BLE_MESH_MODEL_KEYS_UNUSED,
    .groups = ESP_BLE_MESH_MODEL_GROUPS_UNASSIGNED,
};

/**
 * @brief BLE Mesh Configuration Server callback function.
 *
 * Handles state change events and dispatches them to the appropriate callbacks
 * registered with the configuration server.
 *
 * @param[in] event Configuration server event type.
 * @param[in] param Pointer to the BLE Mesh Configuration Server callback parameters.
 */
static void meshx_ble_mesh_config_server_cb(esp_ble_mesh_cfg_server_cb_event_t event,
                                            const esp_ble_mesh_cfg_server_cb_param_t *param)
{
    if (event != ESP_BLE_MESH_CFG_SERVER_STATE_CHANGE_EVT)
        return;

    meshx_config_srv_cb_param_t pub_param =
        {
            .ctx =
                {
                    .net_idx = param->ctx.net_idx,
                    .app_idx = param->ctx.app_idx,
                    .dst_addr = param->ctx.recv_dst,
                    .src_addr = param->ctx.addr,
                    .opcode = param->ctx.recv_op,
                    .p_ctx = (meshx_ptr_t) &param->ctx
                },
            .model =
                {
                    .model_id = param->model->model_id,
                    .el_id = param->model->element_idx,
                    .p_model = param->model
                }
        };

    config_evt_t pub_evt = CONTROL_TASK_MSG_EVT_ALL;
    /* Copy all the status msg from the BLE layer */
    memcpy(&pub_param.state_change,
           &param->value.state_change,
           sizeof(meshx_cfg_srv_state_change_t));

    if (pub_evt == CONTROL_TASK_MSG_EVT_ALL)
        return;

    meshx_err_t err = control_task_msg_publish(
        CONTROL_TASK_MSG_CODE_CONFIG,
        pub_evt,
        &pub_param,
        sizeof(pub_param));
    if (err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Error publishing control task msg (Err: 0x%x)", err);
    }
}

meshx_err_t meshx_plat_config_srv_init(void)
{
    esp_err_t err = esp_ble_mesh_register_config_server_callback(
        (esp_ble_mesh_cfg_server_cb_t)&meshx_ble_mesh_config_server_cb);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Error plat registering config server (Err: 0x%x)", err);
        return MESHX_ERR_PLAT;
    }
    return MESHX_SUCCESS;
}

meshx_err_t meshx_plat_get_config_srv_instance(meshx_ptr_t* p_conf_srv)
{
    if (p_conf_srv == NULL)
    {
        return MESHX_INVALID_ARG;
    }
    /* Return the pointer to the configuration server instance */
    *p_conf_srv = &meshx_config_server_instance;

    return MESHX_SUCCESS;
}

meshx_err_t meshx_plat_get_config_srv_model(meshx_ptr_t p_model)
{
    if (p_model == NULL)
    {
        return MESHX_INVALID_ARG;
    }
    /* Return the pointer to the configuration server model */
    memcpy(p_model, &meshx_config_server_model, sizeof(meshx_config_server_model));
    return MESHX_SUCCESS;
}
