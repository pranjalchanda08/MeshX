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

/**
 * @brief Callback function for BLE Mesh Generic Server events.
 *
 * This function is invoked to handle events related to the Generic Server model
 * in the BLE Mesh stack. It processes various server events and their associated
 * parameters.
 *
 * @param event The event type received by the Generic Server.
 * @param param Pointer to the structure containing event-specific parameters.
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

meshx_err_t meshx_plat_gen_srv_send_status(
        meshx_model_t *p_model,
        meshx_ctx_t *p_ctx,
        meshx_ptr_t p_data,
        uint32_t data_len
    )
{
    static esp_ble_mesh_msg_ctx_t ctx;
    const esp_ble_mesh_msg_ctx_t *pctx = (esp_ble_mesh_msg_ctx_t *)p_ctx->p_ctx;
    if(pctx != NULL)
    {
        memcpy(&ctx, pctx, sizeof(esp_ble_mesh_msg_ctx_t));
    }

    ctx.net_idx    =   p_ctx->net_idx;
    ctx.app_idx    =   p_ctx->app_idx;
    ctx.addr       =   p_ctx->dst_addr;
    ctx.send_ttl   =   ESP_BLE_MESH_TTL_DEFAULT;
    ctx.send_cred  =   0;
    ctx.send_tag   =   BIT1;

    esp_err_t err = esp_ble_mesh_server_model_send_msg(p_model->p_model,
        &ctx,
        p_ctx->opcode,
        (uint16_t)data_len,
        (uint8_t*)p_data);
    if(err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Mesh Model msg send failed (err: 0x%x)", err);
        return MESHX_ERR_PLAT;
    }
    MESHX_LOGD(MODULE_ID_MODEL_SERVER, "Mesh Model msg sent (opcode: 0x%04x)", p_ctx->opcode);
    return MESHX_SUCCESS;
}

meshx_err_t meshx_plat_set_gen_srv_state(void * p_model, const meshx_gen_server_state_t *state, uint16_t state_len)
{
    if(!p_model)
        return MESHX_INVALID_ARG;

    MESHX_MODEL * model = (MESHX_MODEL *)p_model;
    meshx_ptr_t srv = model->user_data;

    if(!srv)
        return MESHX_INVALID_STATE;

    /**
     * Here we are getting the state pointer from the generic onoff server structure
     * But in all the other generic servers, the state pointer are at the same position
     * that of the generic onoff server structure.
     */
    meshx_ptr_t state_ptr = (meshx_ptr_t)&((esp_ble_mesh_gen_onoff_srv_t*) srv)->state;

    memcpy(state_ptr, state, state_len);

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
    meshx_gen_server_state_t state_change;

    state_change.onoff.onoff = state;
    state_change.onoff.target_onoff = state;

    return meshx_plat_set_gen_srv_state (p_model, &state_change, sizeof(state));
}
