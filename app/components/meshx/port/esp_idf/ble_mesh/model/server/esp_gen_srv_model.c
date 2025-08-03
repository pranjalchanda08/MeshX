/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file esp_gen_srv_model.c
 * @brief Implementation of the Generic OnOff Server model for BLE Mesh.
 *        This file contains the initialization, state management, and
 *        message handling logic for the Generic OnOff Server model in
 *        the MeshX platform.
 *
 *        The Generic OnOff Server model is responsible for managing the
 *        on/off state of a device in a BLE Mesh network. It handles
 *        incoming messages, updates the state, and publishes the state
 *        changes to the network.
 *
 * @author Pranjal Chanda
 *
 */
#include "meshx_control_task.h"
#include "interface/ble_mesh/server/meshx_ble_mesh_gen_srv.h"

#define CONTROL_TASK_MSG_EVT_TO_BLE_GEN_SRV_MASK \
    CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF_SRV

/**
 * @brief Creates and initializes the Generic Server model platform resources.
 *
 * This function sets up the necessary resources for a Generic Server model,
 * including publication and OnOff server instances.
 *
 * @param[in]  p_model  Pointer to the model instance to be initialized.
 * @param[out] p_pub    Pointer to the location where the publication context will be stored.
 * @param[out] p_srv    Pointer to the location where the OnOff server instance will be stored.
 *
 * @return meshx_err_t Returns an error code indicating success or failure of the operation.
 */
static meshx_err_t meshx_plat_gen_srv_create(void* p_model, void** p_pub, void** p_srv)
{
    if(!p_model || !p_pub || !p_srv)
        return MESHX_INVALID_ARG;

    meshx_err_t err = MESHX_SUCCESS;

    err = meshx_plat_create_model_pub(p_pub, 1);
    if (err)
        return meshx_plat_del_model_pub(p_pub);

    *p_srv = (MESHX_GEN_ONOFF_SRV *) MESHX_CALOC(1, sizeof(MESHX_GEN_ONOFF_SRV));
    if(!*p_srv)
        return MESHX_NO_MEM;

    ((MESHX_GEN_ONOFF_SRV*)*p_srv)->rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;
    ((MESHX_GEN_ONOFF_SRV*)*p_srv)->rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;
    ((MESHX_MODEL*)p_model)->user_data = *p_srv;

    void **temp = (void**) &((MESHX_MODEL*)p_model)->pub;

    *temp = *p_pub;

    return err;
}

/**
 * @brief String representation of the server state change events.
 */
static const char *server_state_str[] = {
    [ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT] = "SRV_STATE_CH",
    [ESP_BLE_MESH_GENERIC_SERVER_RECV_GET_MSG_EVT] = "SRV_RECV_GET",
    [ESP_BLE_MESH_GENERIC_SERVER_RECV_SET_MSG_EVT] = "SRV_RECV_SET"
};

#if 0
/**
 * @brief Handles the BLE message sending for the Generic OnOff Server model.
 *
 * This function processes the event to send a BLE Mesh message for the
 * Generic Server model.
 *
 * @param[in] pdev Pointer to the device structure.
 * @param[in] evt Event type for the control task message to BLE.
 * @param[in] params Parameters for the BLE Mesh Generic Server model.
 *
 * @return
 *     - MESHX_SUCCESS: Message sent successfully or event type not matched.
 *     - MESHX_FAIL: Failed to send the message.
 */
static meshx_err_t esp_ble_mesh_gen_srv_msg_send(
                            const dev_struct_t *pdev,
                            control_task_msg_evt_to_ble_t evt,
                            meshx_gen_srv_cb_param_t *params)
{
    if((evt & CONTROL_TASK_MSG_EVT_TO_BLE_GEN_SRV_MASK) == 0)
        return MESHX_SUCCESS;

    esp_ble_mesh_msg_ctx_t *ctx = (esp_ble_mesh_msg_ctx_t *)params->ctx.p_ctx;
    bool malloc_flag = false;

    if(ctx == NULL)
    {
        ctx = (esp_ble_mesh_msg_ctx_t *) MESHX_MALLOC(sizeof(esp_ble_mesh_msg_ctx_t));
        if(ctx == NULL)
            return MESHX_NO_MEM;

        malloc_flag = true;
    }

    ctx->net_idx    =   params->ctx.net_idx;
    ctx->app_idx    =   params->ctx.app_idx;
    ctx->addr       =   params->ctx.dst_addr;
    ctx->send_ttl   =   ESP_BLE_MESH_TTL_DEFAULT;
    ctx->send_cred  =   0;
    ctx->send_tag   =   BIT1;

    esp_err_t err = esp_ble_mesh_server_model_send_msg(params->model.p_model,
        ctx,
        params->ctx.opcode,
        sizeof(params->state_change.onoff_set.onoff),
        &params->state_change.onoff_set.onoff);
    if(err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Mesh Model msg send failed (err: 0x%x)", err);
        return MESHX_ERR_PLAT;
    }

    if(malloc_flag)
        MESHX_FREE(ctx);

    ESP_UNUSED(pdev);
    return MESHX_SUCCESS;
}
/**
 * @brief Register Callback function to handle messages sent to the BLE layer From Apps layer.
 *
 * @param cb Callback function to handle messages.
 */
static meshx_err_t meshx_plat_reg_gen_srv_msg_handler(control_task_msg_handle_t cb)
{
    if(!cb)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid callback function");
        return MESHX_INVALID_ARG;
    }
    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_TO_BLE,
        CONTROL_TASK_MSG_EVT_TO_BLE_GEN_SRV_MASK,
        cb
    );
}
#endif

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
    ESP_UNUSED(server_state_str);
    MESHX_LOGD(MODULE_ID_MODEL_SERVER, "%s, op|src|dst:%04" PRIx32 "|%04x|%04x",
            server_state_str[event], param->ctx.recv_op, param->ctx.addr, param->ctx.recv_dst);

    if (event != ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT)
        return;

    meshx_gen_srv_cb_param_t pub_param = {
        .ctx = {
            .net_idx    = param->ctx.net_idx,
            .app_idx    = param->ctx.app_idx,
            .dst_addr   = param->ctx.recv_dst,
            .src_addr   = param->ctx.addr,
            .opcode     = param->ctx.recv_op,
            .p_ctx      = &param->ctx
        },
        .model = {
            .pub_addr   = param->model->pub->publish_addr,
            .model_id   = param->model->model_id,
            .el_id      = param->model->element_idx,
            .p_model    = param->model
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
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to publish to control task");
    }
}

/**
 * @brief Send a status message from the Generic Server model.
 *
 * This function sends a status message to the specified context with the provided data.
 *
 * @param[in] p_model Pointer to the model instance.
 * @param[in] p_ctx Pointer to the context structure containing destination address and other parameters.
 * @param[in] p_data Pointer to the data to be sent.
 * @param[in] data_len Length of the data to be sent.
 *
 * @return
 *     - MESHX_SUCCESS: Message sent successfully.
 *     - MESHX_NO_MEM: Memory allocation failed.
 *     - MESHX_ERR_PLAT: Platform error occurred while sending the message.
 */
meshx_err_t meshx_plat_gen_srv_send_status(
        meshx_model_t *p_model,
        meshx_ctx_t *p_ctx,
        meshx_ptr_t p_data,
        uint32_t data_len
    )
{
    esp_ble_mesh_msg_ctx_t *ctx = (esp_ble_mesh_msg_ctx_t *)p_ctx->p_ctx;
    bool malloc_flag = false;

    if(ctx == NULL)
    {
        ctx = (esp_ble_mesh_msg_ctx_t *) MESHX_MALLOC(sizeof(esp_ble_mesh_msg_ctx_t));
        if(ctx == NULL)
            return MESHX_NO_MEM;

        malloc_flag = true;
    }

    ctx->net_idx    =   p_ctx->net_idx;
    ctx->app_idx    =   p_ctx->app_idx;
    ctx->addr       =   p_ctx->dst_addr;
    ctx->send_ttl   =   ESP_BLE_MESH_TTL_DEFAULT;
    ctx->send_cred  =   0;
    ctx->send_tag   =   BIT1;

    esp_err_t err = esp_ble_mesh_server_model_send_msg(p_model->p_model,
        ctx,
        p_ctx->opcode,
        (uint16_t)data_len,
        (uint8_t*)p_data);
    if(err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Mesh Model msg send failed (err: 0x%x)", err);
        return MESHX_ERR_PLAT;
    }

    if(malloc_flag)
        MESHX_FREE(ctx);

    return MESHX_SUCCESS;
}

/**
 * @brief Set the state of a generic server model.
 *
 * This function updates the on/off state of a specified generic server model.
 *
 * @param[in] p_model       Pointer to the model whose state is to be set.
 * @param[in] on_off_state  The desired on/off state to set for the model.
 *
 * @return MESHX_SUCCESS on success, or an appropriate error code on failure.
 */
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
    meshx_err_t err = MESHX_SUCCESS;
#if 0
    /* Register callback for enabling Sending of Model Msg From MeshX to BLE Layer */
    err = meshx_plat_reg_gen_srv_msg_handler(
        (control_task_msg_handle_t)&esp_ble_mesh_gen_srv_msg_send
    );
    if (err)
        return err;
#endif
    /* Register the ESP Generic Server callback */
    esp_err_t esp_err = esp_ble_mesh_register_generic_server_callback(
            (MESHX_GEN_SRV_CB)&esp_ble_mesh_generic_server_cb
        );
    if(esp_err != ESP_OK)
        err = MESHX_ERR_PLAT;

    return err;
}

meshx_err_t meshx_plat_on_off_gen_srv_create(void* p_model, void** p_pub, void** p_onoff_srv)
{
    if(!p_model || !p_pub || !p_onoff_srv)
        return MESHX_INVALID_ARG;

    /* SIG ON OFF initialisation */
    uint16_t model_id = ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV;
    memcpy((meshx_ptr_t) &(((MESHX_MODEL*)p_model)->model_id), &model_id, sizeof(model_id));

    return meshx_plat_gen_srv_create(p_model, p_pub, p_onoff_srv);
}

meshx_err_t meshx_plat_gen_srv_delete(void** p_pub, void** p_srv)
{
    if(p_srv)
    {
        MESHX_FREE(*p_srv);
        *p_srv = NULL;
    }

    return meshx_plat_del_model_pub(p_pub);
}

meshx_err_t meshx_plat_gen_on_off_srv_restore(void* p_model, uint8_t state)
{
    return meshx_plat_set_gen_srv_state (p_model, state);
}
