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

#include "interface/ble_mesh/meshx_ble_mesh_config_srv.h"

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
                    .p_ctx = (void*) &param->ctx
                },
            .model =
                {
                    .model_id = param->model->model_id,
                    .el_id = param->model->element_idx,
                    .p_model = param->model
                }
        };

    config_evt_t pub_evt = CONTROL_TASK_MSG_EVT_ALL;
    switch (param->model->model_id)
    {
    case ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD:
        memcpy(&pub_param.state_change.appkey_add,
               &param->value.state_change.appkey_add,
               sizeof(pub_param.state_change.appkey_add));
        pub_evt = CONTROL_TASK_MSG_EVT_APP_KEY_ADD;
        break;
    case ESP_BLE_MESH_MODEL_OP_NET_KEY_ADD:
        memcpy(&pub_param.state_change.netkey_add,
               &param->value.state_change.netkey_add,
               sizeof(pub_param.state_change.netkey_add));
        pub_evt = CONTROL_TASK_MSG_EVT_NET_KEY_ADD;
        break;
    case ESP_BLE_MESH_MODEL_OP_MODEL_SUB_ADD:
        memcpy(&pub_param.state_change.mod_sub_add,
               &param->value.state_change.mod_sub_add,
               sizeof(pub_param.state_change.mod_sub_add));
        pub_evt = CONTROL_TASK_MSG_EVT_SUB_ADD;
        break;
    case ESP_BLE_MESH_MODEL_OP_MODEL_PUB_SET:
        memcpy(&pub_param.state_change.mod_pub_set,
               &param->value.state_change.mod_pub_set,
               sizeof(pub_param.state_change.mod_pub_set));
        pub_evt = CONTROL_TASK_MSG_EVT_PUB_ADD;
        break;
    case ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND:
        memcpy(&pub_param.state_change.mod_app_bind,
               &param->value.state_change.mod_app_bind,
               sizeof(pub_param.state_change.mod_app_bind));
        pub_evt = CONTROL_TASK_MSG_EVT_APP_KEY_BIND;
        break;
    case ESP_BLE_MESH_MODEL_OP_NET_KEY_DELETE:
        memcpy(&pub_param.state_change.netkey_delete,
               &param->value.state_change.netkey_delete,
               sizeof(pub_param.state_change.netkey_delete));
        pub_evt = CONTROL_TASK_MSG_EVT_NET_KEY_DEL;
        break;
    case ESP_BLE_MESH_MODEL_OP_APP_KEY_DELETE:
        memcpy(&pub_param.state_change.appkey_delete,
               &param->value.state_change.appkey_delete,
               sizeof(pub_param.state_change.appkey_delete));
        pub_evt = CONTROL_TASK_MSG_EVT_APP_KEY_DEL;
        break;
    case ESP_BLE_MESH_MODEL_OP_MODEL_SUB_DELETE:
        memcpy(&pub_param.state_change.mod_sub_delete,
               &param->value.state_change.mod_sub_delete,
               sizeof(pub_param.state_change.mod_sub_delete));
        pub_evt = CONTROL_TASK_MSG_EVT_SUB_DEL;
        break;
    case ESP_BLE_MESH_MODEL_OP_MODEL_APP_UNBIND:
        memcpy(&pub_param.state_change.mod_app_unbind,
               &param->value.state_change.mod_app_unbind,
               sizeof(pub_param.state_change.mod_app_unbind));
        pub_evt = CONTROL_TASK_MSG_EVT_APP_KEY_UNBIND;
        break;
    default:
        break;
    }
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

meshx_err_t meshx_plat_get_config_srv_instance(void** p_conf_srv)
{
    if (p_conf_srv == NULL)
    {
        return MESHX_INVALID_ARG;
    }
    /* Return the pointer to the configuration server instance */
    *p_conf_srv = &meshx_config_server_instance;

    return MESHX_SUCCESS;
}

meshx_err_t meshx_plat_get_config_srv_model(void* p_model)
{
    if (p_model == NULL)
    {
        return MESHX_INVALID_ARG;
    }
    /* Return the pointer to the configuration server model */
    memcpy(p_model, &meshx_config_server_model, sizeof(meshx_config_server_model));
    return MESHX_SUCCESS;
}
