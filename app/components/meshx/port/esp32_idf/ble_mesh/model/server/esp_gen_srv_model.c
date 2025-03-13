/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_gen_server.c
 * @brief Implementation of the BLE Mesh Generic Server for the meshxuct.
 *
 * This file contains the implementation of the BLE Mesh Generic Server
 * for handling various server events and registering callbacks.
 *
 *
 */

#include "esp_log.h"
#include "meshx_gen_server.h"
#include "meshx_control_task.h"

#ifdef TAG
#undef TAG
#define TAG "ESP_GEN_SRV"
#endif

#define MESHX_SERVER_INIT_MAGIC_NO 0x1121

/**
 * @brief String representation of the server state change events.
 */
static const char *server_state_str[] = {
    [ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT] = "SRV_STATE_CH",
    [ESP_BLE_MESH_GENERIC_SERVER_RECV_GET_MSG_EVT] = "SRV_RECV_GET",
    [ESP_BLE_MESH_GENERIC_SERVER_RECV_SET_MSG_EVT] = "SRV_RECV_SET"
};

static uint16_t meshx_server_init = 0;

static meshx_err_t ble_send_msg_handle_t(
                            const dev_struct_t *pdev,
                            control_task_msg_evt_to_ble_t evt,
                            meshx_gen_srv_model_param_t *params)
{
    if(evt != CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF_SRV)
        return MESHX_SUCCESS;

    esp_ble_mesh_msg_ctx_t *ctx = params->ctx.p_ctx;

    ctx->addr = params->ctx.dst_addr;

    esp_err_t err = esp_ble_mesh_server_model_send_msg(params->model.p_model,
        ctx,
        ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS,
        sizeof(params->state_change.onoff_set.onoff),
        &params->state_change.onoff_set.onoff);
    if(err)
    {
        ESP_LOGE(TAG, "Mesh Model msg send failed (err: 0x%x)", err);
        return MESHX_FAIL;
    }

    ESP_UNUSED(pdev);
    return MESHX_SUCCESS;
}

/**
 * @brief Callback function for BLE Mesh Generic Server events.
 *
 * This function is called whenever a BLE Mesh Generic Server event occurs.
 *
 * @param event The event type for the BLE Mesh Generic Server.
 * @param param Parameters associated with the event.
 */
static void esp_ble_mesh_generic_server_cb(MESHX_GEN_SRV_CB_EVT event,
                                           MESHX_GEN_SRV_CB_PARAM *param)
{
    ESP_LOGD(TAG, "%s, op|src|dst:%04" PRIx32 "|%04x|%04x",
             server_state_str[event], param->ctx.recv_op, param->ctx.addr, param->ctx.recv_dst);

    if (event != ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT)
        return;

    meshx_gen_srv_model_param_t pub_param = {
        .ctx = {
            .dst_addr = param->ctx.recv_dst,
            .src_addr = param->ctx.addr,
            .opcode = param->ctx.recv_op,
            .p_ctx = &param->ctx
        },
        .model = {
            .pub_addr = param->model->pub->publish_addr,
            .model_id = param->model->model_id,
            .el_id = param->model->element_idx,
            .p_model = param->model
        },
        .state_change = {
            .onoff_set.onoff = param->value.state_change.onoff_set.onoff
        }
    };

    if(pub_param.model.model_id == ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV)
    {
        MESHX_GEN_ONOFF_SRV *srv = (MESHX_GEN_ONOFF_SRV *)param->model->user_data;
        srv->state.onoff = pub_param.state_change.onoff_set.onoff;
    }

    meshx_err_t err = control_task_msg_publish(
        CONTROL_TASK_MSG_CODE_FRM_BLE,
        pub_param.model.model_id,
        &pub_param,
        sizeof(meshx_gen_srv_model_param_t));
    if (err)
    {
        ESP_LOGE(TAG, "Failed to publish to control task");
    }
}

/**
 * @brief Register a callback function for the meshxuction server model.
 *
 * This function registers a callback function that will be called when
 * specific events related to the meshxuction server model occur.
 *
 * @param[in] model_id  The ID of the model for which the callback is being registered.
 * @param[in] cb        The callback function to be registered.
 *
 * @return
 *     - MESHX_SUCCESS: Callback registered successfully.
 *     - MESHX_INVALID_ARG: Invalid arguments.
 *     - MESHX_FAIL: Failed to register the callback.
 */
meshx_err_t meshx_gen_srv_reg_cb(uint32_t model_id, meshx_server_cb cb)
{
    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_FRM_BLE,
        model_id,
        (control_task_msg_handle_t)cb);
}

/**
 * @brief Callback function to deregister a generic server model.
 *
 * This function is called to deregister a generic server model identified by the given model ID.
 *
 * @param[in] model_id  The ID of the model to be deregistered.
 * @param[in] cb        The callback function to be deregistered.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_INVALID_ARG: Invalid argument
 *     - MESHX_FAIL: Other failures
 */
meshx_err_t meshx_gen_srv_dereg_cb(uint32_t model_id, meshx_server_cb cb)
{
    return control_task_msg_unsubscribe(
        CONTROL_TASK_MSG_CODE_FRM_BLE,
        model_id,
        (control_task_msg_handle_t)cb);
}

/**
 * @brief Initialize the meshxuction generic server.
 *
 * This function sets up the necessary configurations and initializes the
 * meshxuction generic server for the BLE mesh node.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_FAIL: Failed to initialize the server
 */
meshx_err_t meshx_gen_srv_init(void)
{
    if (meshx_server_init == MESHX_SERVER_INIT_MAGIC_NO)
        return MESHX_SUCCESS;
    meshx_server_init = MESHX_SERVER_INIT_MAGIC_NO;

    meshx_err_t err = control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_TO_BLE,
        CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF_SRV,
        (control_task_msg_handle_t)&ble_send_msg_handle_t
    );
    if (err)
        return err;

    esp_err_t esp_err = esp_ble_mesh_register_generic_server_callback(
            (MESHX_GEN_SRV_CB)&esp_ble_mesh_generic_server_cb);
    if(esp_err != ESP_OK)
        err = MESHX_FAIL;

    return err;
}
