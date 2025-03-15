/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_gen_server.c
 * @brief Implementation of the BLE Mesh Generic Server for the meshxuct.
 *
 * This file contains the implementation of the BLE Mesh Generic Server
 * for handling various server events and registering callbacks.
 *
 * @author Pranjal Chanda
 */

#include "esp_log.h"
#include "meshx_gen_server.h"
#include "meshx_control_task.h"

#ifdef TAG
#undef TAG
#define TAG "ESP_GEN_SRV"
#endif

/**
 * @brief String representation of the server state change events.
 */
static const char *server_state_str[] = {
    [ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT] = "SRV_STATE_CH",
    [ESP_BLE_MESH_GENERIC_SERVER_RECV_GET_MSG_EVT] = "SRV_RECV_GET",
    [ESP_BLE_MESH_GENERIC_SERVER_RECV_SET_MSG_EVT] = "SRV_RECV_SET"
};

/**
 * @brief Handles the BLE message sending for the Generic OnOff Server model.
 *
 * This function processes the event to send a BLE Mesh message for the
 * Generic OnOff Server model. It checks if the event type is
 * CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF_SRV and sends the OnOff status
 * message using the provided parameters.
 *
 * @param[in] pdev Pointer to the device structure.
 * @param[in] evt Event type for the control task message to BLE.
 * @param[in] params Parameters for the BLE Mesh Generic Server model.
 *
 * @return
 *     - MESHX_SUCCESS: Message sent successfully or event type not matched.
 *     - MESHX_FAIL: Failed to send the message.
 */
static meshx_err_t ble_send_msg_handle_t(
                            const dev_struct_t *pdev,
                            control_task_msg_evt_to_ble_t evt,
                            meshx_gen_srv_cb_param_t *params)
{
    if(evt != CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF_SRV)
        return MESHX_SUCCESS;

    if(params->model.model_id != ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS)
        return MESHX_INVALID_ARG;

    esp_ble_mesh_msg_ctx_t *ctx = params->ctx.p_ctx;
    bool malloc_flag = false;

    if(ctx == NULL)
    {
        ctx = (esp_ble_mesh_msg_ctx_t *) malloc(sizeof(esp_ble_mesh_msg_ctx_t));
        if(ctx == NULL)
            return MESHX_NO_MEM;

        malloc_flag = true;

        ctx->net_idx    =   params->ctx.net_idx;
        ctx->app_idx    =   params->ctx.app_idx;
        ctx->addr       =   params->ctx.dst_addr;
        ctx->send_ttl   =   ESP_BLE_MESH_TTL_DEFAULT;
        ctx->send_cred  =   0;
        ctx->send_tag   =   BIT1;
    }
    else
    {
        ctx->addr = params->ctx.dst_addr;
    }

    esp_err_t err = esp_ble_mesh_server_model_send_msg(params->model.p_model,
        ctx,
        params->model.model_id,
        sizeof(params->state_change.onoff_set.onoff),
        &params->state_change.onoff_set.onoff);
    if(err)
    {
        ESP_LOGE(TAG, "Mesh Model msg send failed (err: 0x%x)", err);
        return MESHX_FAIL;
    }

    if(malloc_flag)
    {
        free(ctx);
    }

    ESP_UNUSED(pdev);
    return MESHX_SUCCESS;
}

/**
 * @brief Callback function for BLE Mesh Generic Server events.
 *
 * This function is called whenever a BLE Mesh Generic Server event occurs.
 *
 * @param[in] event The event type for the BLE Mesh Generic Server.
 * @param[in] param Parameters associated with the event.
 */
static void esp_ble_mesh_generic_server_cb(MESHX_GEN_SRV_CB_EVT event,
                                           MESHX_GEN_SRV_CB_PARAM *param)
{
    ESP_LOGD(TAG, "%s, op|src|dst:%04" PRIx32 "|%04x|%04x",
             server_state_str[event], param->ctx.recv_op, param->ctx.addr, param->ctx.recv_dst);

    if (event != ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT)
        return;

    meshx_gen_srv_cb_param_t pub_param = {
        .ctx = {
            .net_idx = param->ctx.net_idx,
            .app_idx = param->ctx.app_idx,
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
        sizeof(meshx_gen_srv_cb_param_t));
    if (err)
    {
        ESP_LOGE(TAG, "Failed to publish to control task");
    }
}

meshx_err_t meshx_plat_get_gen_srv_model_id(void* p_model, uint16_t *model_id)
{
    if(!p_model)
        return MESHX_INVALID_ARG;

    MESHX_MODEL * model = (MESHX_MODEL *)p_model;
    *model_id = model->model_id;

    return MESHX_SUCCESS;
}

meshx_err_t meshx_plat_set_gen_srv_state(void * p_model, uint8_t on_off_state)
{
    if(!p_model)
        return MESHX_INVALID_ARG;

    MESHX_MODEL * model = (MESHX_MODEL *)p_model;
    MESHX_GEN_ONOFF_SRV *srv = (MESHX_GEN_ONOFF_SRV *)model->user_data;

    srv->state.onoff = on_off_state;

    return MESHX_SUCCESS;
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
meshx_err_t meshx_plat_gen_srv_init(void)
{
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
