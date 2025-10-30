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

/**
 * @brief Send a status message from the Generic Server model.
 *
 * This function sends a status message to the specified context with the provided data.
 *
 * @param[in] p_model   Pointer to the model instance.
 * @param[in] p_ctx     Pointer to the context structure containing destination address and other parameters.
 * @param[in] p_data    Pointer to the data to be sent.
 * @param[in] data_len  Length of the data to be sent.
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

/**
 * @brief Set the state of a generic server model.
 *
 * This function updates the on/off state of a specified generic server model.
 *
 * @param[in] p_model       Pointer to the model whose state is to be set.
 * @param[in] state         The desired on/off state to set for the model.
 * @param[in] state_len     The length of the state data.
 *
 * @return MESHX_SUCCESS on success, or an appropriate error code on failure.
 */
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
 * @brief Initialize the generic server model platform.
 *
 * This function initializes the generic server model platform and registers the
 * necessary callbacks for handling generic server model operations.
 *
 * @return MESHX_SUCCESS on success, or an appropriate error code on failure.
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

/**
 * @brief Creates a Generic OnOff Server model and its publication context.
 *
 * This function initializes the Generic OnOff Server model, its publication
 * context, and allocates memory for the server instance. It checks for
 * invalid arguments and handles memory allocation failures.
 *
 * @param[out] p_model Pointer to the model structure to be created.
 * @param[out] p_pub Pointer to the publication context to be created.
 * @param[out] p_onoff_srv Pointer to the OnOff server instance to be allocated.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully created the model and publication context.
 *     - MESHX_INVALID_ARG: One or more arguments are invalid.
 *     - MESHX_NO_MEM: Memory allocation failed.
 */
meshx_err_t meshx_plat_on_off_gen_srv_create(void* p_model, void** p_pub, void** p_onoff_srv)
{
    if(!p_model || !p_pub || !p_onoff_srv)
        return MESHX_INVALID_ARG;

    /* SIG ON OFF initialisation */
    uint16_t model_id = ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV;
    memcpy((meshx_ptr_t) &(((MESHX_MODEL*)p_model)->model_id), &model_id, sizeof(model_id));

    return meshx_plat_gen_srv_create(p_model, p_pub, p_onoff_srv);
}

/**
 * @brief Creates a Generic Level Server model and its publication context.
 *
 * This function initializes the Generic Level Server model, its publication
 * context, and allocates memory for the server instance. It checks for
 * invalid arguments and handles memory allocation failures.
 *
 * @param[out] p_model Pointer to the model structure to be created.
 * @param[out] p_pub Pointer to the publication context to be created.
 * @param[out] p_level_srv Pointer to the Level server instance to be allocated.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully created the model and publication context.
 *     - MESHX_INVALID_ARG: One or more arguments are invalid.
 *     - MESHX_NO_MEM: Memory allocation failed.
 */
meshx_err_t meshx_plat_level_gen_srv_create(void* p_model, void** p_pub, void** p_level_srv)
{
    if(!p_model || !p_pub || !p_level_srv)
        return MESHX_INVALID_ARG;

    /* SIG Level Server initialisation */
    uint16_t model_id = ESP_BLE_MESH_MODEL_ID_GEN_LEVEL_SRV;
    memcpy((meshx_ptr_t) &(((MESHX_MODEL*)p_model)->model_id), &model_id, sizeof(model_id));

    return meshx_plat_gen_srv_create(p_model, p_pub, p_level_srv);
}

/**
 * @brief Creates a Generic Battery Server model and its publication context.
 *
 * This function initializes the Generic Battery Server model, its publication
 * context, and allocates memory for the server instance. It checks for
 * invalid arguments and handles memory allocation failures.
 *
 * @param[out] p_model Pointer to the model structure to be created.
 * @param[out] p_pub Pointer to the publication context to be created.
 * @param[out] p_battery_srv Pointer to the Battery server instance to be allocated.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully created the model and publication context.
 *     - MESHX_INVALID_ARG: One or more arguments are invalid.
 *     - MESHX_NO_MEM: Memory allocation failed.
 */
meshx_err_t meshx_plat_battery_gen_srv_create(void* p_model, void** p_pub, void** p_battery_srv)
{
    if(!p_model || !p_pub || !p_battery_srv)
        return MESHX_INVALID_ARG;

    /* SIG Battery Server initialisation */
    uint16_t model_id = ESP_BLE_MESH_MODEL_ID_GEN_BATTERY_SRV;
    memcpy((meshx_ptr_t) &(((MESHX_MODEL*)p_model)->model_id), &model_id, sizeof(model_id));

    return meshx_plat_gen_srv_create(p_model, p_pub, p_battery_srv);
}

/**
 * @brief Creates a Generic Location Server model and its publication context.
 *
 * This function initializes the Generic Location Server model, its publication
 * context, and allocates memory for the server instance. It checks for
 * invalid arguments and handles memory allocation failures.
 *
 * @param[out] p_model Pointer to the model structure to be created.
 * @param[out] p_pub Pointer to the publication context to be created.
 * @param[out] p_location_srv Pointer to the Location server instance to be allocated.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully created the model and publication context.
 *     - MESHX_INVALID_ARG: One or more arguments are invalid.
 *     - MESHX_NO_MEM: Memory allocation failed.
 */
meshx_err_t meshx_plat_location_gen_srv_create(void* p_model, void** p_pub, void** p_location_srv)
{
    if(!p_model || !p_pub || !p_location_srv)
        return MESHX_INVALID_ARG;

    /* SIG Location Server initialisation */
    uint16_t model_id = ESP_BLE_MESH_MODEL_ID_GEN_LOCATION_SRV;
    memcpy((meshx_ptr_t) &(((MESHX_MODEL*)p_model)->model_id), &model_id, sizeof(model_id));

    return meshx_plat_gen_srv_create(p_model, p_pub, p_location_srv);
}

/**
 * @brief Creates a Generic Power Level Server model and its publication context.
 *
 * This function initializes the Generic Power Level Server model, its publication
 * context, and allocates memory for the server instance. It checks for
 * invalid arguments and handles memory allocation failures.
 *
 * @param[out] p_model Pointer to the model structure to be created.
 * @param[out] p_pub Pointer to the publication context to be created.
 * @param[out] p_power_level_srv Pointer to the Power Level server instance to be allocated.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully created the model and publication context.
 *     - MESHX_INVALID_ARG: One or more arguments are invalid.
 *     - MESHX_NO_MEM: Memory allocation failed.
 */
meshx_err_t meshx_plat_power_level_gen_srv_create(void* p_model, void** p_pub, void** p_power_level_srv)
{
    if(!p_model || !p_pub || !p_power_level_srv)
        return MESHX_INVALID_ARG;

    /* SIG Power Level Server initialisation */
    uint16_t model_id = ESP_BLE_MESH_MODEL_ID_GEN_POWER_LEVEL_SRV;
    memcpy((meshx_ptr_t) &(((MESHX_MODEL*)p_model)->model_id), &model_id, sizeof(model_id));

    return meshx_plat_gen_srv_create(p_model, p_pub, p_power_level_srv);
}

/**
 * @brief Creates a Generic Default Transition Time Server model and its publication context.
 *
 * This function initializes the Generic Default Transition Time Server model, its publication
 * context, and allocates memory for the server instance. It checks for
 * invalid arguments and handles memory allocation failures.
 *
 * @param[out] p_model Pointer to the model structure to be created.
 * @param[out] p_pub Pointer to the publication context to be created.
 * @param[out] p_trans_time_srv Pointer to the Default Transition Time server instance to be allocated.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully created the model and publication context.
 *     - MESHX_INVALID_ARG: One or more arguments are invalid.
 *     - MESHX_NO_MEM: Memory allocation failed.
 */
meshx_err_t meshx_plat_def_trans_time_gen_srv_create(void* p_model, void** p_pub, void** p_trans_time_srv)
{
    if(!p_model || !p_pub || !p_trans_time_srv)
        return MESHX_INVALID_ARG;

    /* SIG Default Transition Time Server initialisation */
    uint16_t model_id = ESP_BLE_MESH_MODEL_ID_GEN_DEF_TRANS_TIME_SRV;
    memcpy((meshx_ptr_t) &(((MESHX_MODEL*)p_model)->model_id), &model_id, sizeof(model_id));

    return meshx_plat_gen_srv_create(p_model, p_pub, p_trans_time_srv);
}

/**
 * @brief Deletes the Generic OnOff Server model and its associated resources.
 *
 * This function frees the memory allocated for the Generic OnOff Server
 * and sets the pointer to NULL. It also deletes the model publication
 * resources associated with the server.
 *
 * @param[in,out] p_pub Pointer to the publication structure to be deleted.
 * @param[in,out] p_srv Pointer to the Generic Server structure to be freed.
 *
 * @return
 *     - MESHX_SUCCESS: Model and publication deleted successfully.
 *     - MESHX_FAIL: Failed to delete the model or publication.
 */
meshx_err_t meshx_plat_gen_srv_delete(void** p_pub, void** p_srv)
{
    if(p_srv)
    {
        MESHX_FREE(*p_srv);
        *p_srv = NULL;
    }

    return meshx_plat_del_model_pub(p_pub);
}

/**
 * @brief Restores the state of the Generic OnOff Server model.
 *
 * This function sets the user data of the specified model to the given state.
 * It checks if the model pointer is valid before proceeding with the operation.
 *
 * @param[in] p_model Pointer to the model structure.
 * @param[in] state The state to be restored in the model.
 *
 * @return
 *     - MESHX_SUCCESS: State restored successfully.
 *     - MESHX_INVALID_ARG: Invalid model pointer.
 */
meshx_err_t meshx_plat_gen_on_off_srv_restore(void* p_model, uint8_t state)
{
    meshx_gen_server_state_t state_change;

    state_change.onoff.onoff = state;
    state_change.onoff.target_onoff = state;

    return meshx_plat_set_gen_srv_state (p_model, &state_change, sizeof(state));
}
