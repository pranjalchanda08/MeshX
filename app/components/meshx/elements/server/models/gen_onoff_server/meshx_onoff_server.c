/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_onoff_server.c
 * @brief Header file for the On/Off Server model in the BLE Mesh Node application.
 *
 * This file contains the function defination for the
 * On/Off Server model used in the BLE Mesh Node application.
 *
 *
 */
#include "meshx_onoff_server.h"

/**
 * @brief Perform hardware change based on the BLE Mesh generic server callback parameter.
 *
 * This function is responsible for executing the necessary hardware changes
 * when a BLE Mesh generic server event occurs.
 *
 * @param param Pointer to the BLE Mesh generic server callback parameter structure.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - ESP_FAIL: Failure
 */
static meshx_err_t meshx_perform_hw_change(esp_ble_mesh_generic_server_cb_param_t *param)
{
    esp_ble_mesh_gen_onoff_srv_t const *srv = (esp_ble_mesh_gen_onoff_srv_t *)param->model->user_data;

    if (ESP_BLE_MESH_ADDR_IS_UNICAST(param->ctx.recv_dst)
    || (ESP_BLE_MESH_ADDR_BROADCAST(param->ctx.recv_dst))
    || (ESP_BLE_MESH_ADDR_IS_GROUP(param->ctx.recv_dst) && (esp_ble_mesh_is_model_subscribed_to_group(param->model, param->ctx.recv_dst))))
    {
        meshx_err_t err = control_task_msg_publish(
            CONTROL_TASK_MSG_CODE_EL_STATE_CH,
            CONTROL_TASK_MSG_EVT_EL_STATE_CH_SET_ON_OFF,
            srv,
            sizeof(esp_ble_mesh_gen_onoff_srv_t));
        return err ? err : MESHX_SUCCESS;
    }
    return MESHX_NOT_SUPPORTED;
}

#if !CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE
/**
 * @brief Handle Generic OnOff messages for the server model.
 *
 * This function processes the received Generic OnOff messages and performs
 * the necessary actions based on the message type and content.
 *
 * @param param Pointer to the callback parameter structure containing the
 *              details of the received message.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_INVALID_ARG: Invalid argument
 *    - ESP_FAIL: Other failures
 */
static meshx_err_t meshx_handle_gen_onoff_msg(esp_ble_mesh_generic_server_cb_param_t *param)
{
    esp_ble_mesh_gen_onoff_srv_t *srv = (esp_ble_mesh_gen_onoff_srv_t *)param->model->user_data;
    bool send_reply = (param->ctx.recv_op != ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK);
    switch (param->ctx.recv_op)
    {
    case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET:
        break;
    case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET:
    case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK:
        srv->state.onoff = param->value.state_change.onoff_set.onoff;
        ESP_ERROR_CHECK(meshx_perform_hw_change(param));
        break;
    default:
        break;
    }
    if (send_reply
    /* This is meant to notify the respective publish client */
    || param->ctx.addr != param->model->pub->publish_addr)
    {
        /* Here the message was received from unregistered source and mention the state to the respective client */
        ESP_LOGD(TAG, "PUB: src|pub %x|%x", param->ctx.addr, param->model->pub->publish_addr);
        param->ctx.addr = param->model->pub->publish_addr;
        esp_ble_mesh_server_model_send_msg(param->model,
                                           &param->ctx,
                                           ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS,
                                           sizeof(srv->state.onoff),
                                           &srv->state.onoff);
    }
    return MESHX_SUCCESS;
}

#else
/**
 * @brief Handle Generic OnOff messages for the server model.
 *
 * This function processes the received Generic OnOff messages and performs
 * the necessary actions based on the message type and content.
 *
 * @param param Pointer to the callback parameter structure containing the
 *              details of the received message.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_INVALID_ARG: Invalid argument
 *    - ESP_FAIL: Other failures
 */
static meshx_err_t meshx_handle_gen_onoff_msg(const dev_struct_t *pdev, control_task_msg_evt_t model_id, esp_ble_mesh_generic_server_cb_param_t *param)
{
    ESP_UNUSED(pdev);
    ESP_LOGD(TAG, "op|src|dst:%04" PRIx32 "|%04x|%04x",
             param->ctx.recv_op, param->ctx.addr, param->ctx.recv_dst);
    if(model_id != ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV)
    {
        return MESHX_INVALID_ARG;
    }
    esp_ble_mesh_gen_onoff_srv_t *srv = (esp_ble_mesh_gen_onoff_srv_t *)param->model->user_data;
    bool send_reply = (param->ctx.recv_op != ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK);
    switch (param->ctx.recv_op)
    {
    case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET:
        break;
    case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET:
    case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK:
        srv->state.onoff = param->value.state_change.onoff_set.onoff;
        ESP_ERROR_CHECK(meshx_perform_hw_change(param));
        break;
    default:
        break;
    }
    if (send_reply
    /* This is meant to notify the respective publish client */
    || param->ctx.addr != param->model->pub->publish_addr)
    {
        /* Here the message was received from unregistered source and mention the state to the respective client */
        ESP_LOGD(TAG, "PUB: src|pub %x|%x", param->ctx.addr, param->model->pub->publish_addr);
        param->ctx.addr = param->model->pub->publish_addr;
        esp_ble_mesh_server_model_send_msg(param->model,
                                           &param->ctx,
                                           ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS,
                                           sizeof(srv->state.onoff),
                                           &srv->state.onoff);
    }
    return MESHX_SUCCESS;
}
#endif /* !CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE */
/**
 * @brief Initialize the On/Off server model.
 *
 * This function initializes the On/Off server model for the BLE mesh node.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - ESP_FAIL: Failure
 */
meshx_err_t meshx_on_off_server_init()
{
    meshx_err_t err = MESHX_SUCCESS;
#if CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE
    /* Protect only one registration*/
    static uint8_t init_cnt = 0;
    if (init_cnt)
        return MESHX_SUCCESS;
    init_cnt++;
#endif
#if CONFIG_ENABLE_SERVER_COMMON
    err = meshx_gen_srv_init();
    if (err){
        ESP_LOGE(TAG, "Failed to initialize meshx server");
    }
#endif /* CONFIG_ENABLE_SERVER_COMMON */
    err = meshx_gen_srv_reg_cb(ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV, (meshx_server_cb) &meshx_handle_gen_onoff_msg);
    if (err){
        ESP_LOGE(TAG, "Failed to initialize meshx_gen_srv_reg_cb (Err: %d)", err);
    }

    return err;
}
