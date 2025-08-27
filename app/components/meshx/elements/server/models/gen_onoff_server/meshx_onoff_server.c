/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_onoff_server.c
 * @brief Header file for the On/Off Server model in the BLE Mesh Node application.
 *
 * This file contains the function defination for the
 * On/Off Server model used in the BLE Mesh Node application.
 *
 * @author Pranjal Chanda
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
 *     - MESHX_FAIL: Failure
 */
static meshx_err_t meshx_state_change_notify(meshx_gen_srv_cb_param_t *param)
{
    if (!param)
        return MESHX_INVALID_ARG;

    meshx_on_off_srv_el_msg_t srv_onoff_param = {
        .model = param->model,
        .on_off_state = param->state_change.onoff_set.onoff};

    if (MESHX_ADDR_IS_UNICAST(param->ctx.dst_addr) || (MESHX_ADDR_BROADCAST(param->ctx.dst_addr)) || (MESHX_ADDR_IS_GROUP(param->ctx.dst_addr) && (MESHX_SUCCESS == meshx_is_group_subscribed(param->model.p_model, param->ctx.dst_addr))))
    {
        meshx_err_t err = control_task_msg_publish(
            CONTROL_TASK_MSG_CODE_EL_STATE_CH,
            CONTROL_TASK_MSG_EVT_EL_STATE_CH_SET_ON_OFF,
            &srv_onoff_param,
            sizeof(meshx_on_off_srv_el_msg_t));
        return err ? err : MESHX_SUCCESS;
    }
    return MESHX_NOT_SUPPORTED;
}

/**
 * @brief Handle Generic OnOff messages for the server model.
 *
 * This function processes the received Generic OnOff messages and performs
 * the necessary actions based on the message type and content.
 *
 * @param pdev Pointer to the device structure.
 * @param model_id The model ID of the received message.
 * @param param Pointer to the callback parameter structure containing the
 *              details of the received message.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_INVALID_ARG: Invalid argument
 *    - MESHX_FAIL: Other failures
 */
static meshx_err_t meshx_handle_gen_onoff_msg(const dev_struct_t *pdev, control_task_msg_evt_t model_id, meshx_gen_srv_cb_param_t *param)
{
    MESHX_UNUSED(pdev);
    MESHX_LOGD(MODULE_ID_MODEL_SERVER, "op|src|dst:%04" PRIx32 "|%04x|%04x",
               param->ctx.opcode, param->ctx.src_addr, param->ctx.dst_addr);
    if (model_id != MESHX_MODEL_ID_GEN_ONOFF_SRV)
        return MESHX_INVALID_ARG;

    bool send_reply = (param->ctx.opcode != MESHX_MODEL_OP_GEN_ONOFF_SET_UNACK);
    switch (param->ctx.opcode)
    {
    case MESHX_MODEL_OP_GEN_ONOFF_GET:
        break;
    case MESHX_MODEL_OP_GEN_ONOFF_SET:
    case MESHX_MODEL_OP_GEN_ONOFF_SET_UNACK:
        meshx_state_change_notify(param);
        break;
    default:
        break;
    }
    if (send_reply
        /* This is meant to notify the respective publish client */
        || param->ctx.src_addr != param->model.pub_addr)
    {
        /* Here the message was received from unregistered source and mention the state to the respective client */
        MESHX_LOGD(MODULE_ID_MODEL_SERVER, "PUB: src|pub %x|%x", param->ctx.src_addr, param->model.pub_addr);
        param->ctx.opcode = MESHX_MODEL_OP_GEN_ONOFF_STATUS;
        param->ctx.dst_addr = param->model.pub_addr;

        return meshx_gen_srv_send_msg_to_ble(
            CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF_SRV,
            param);
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Send the On/Off status message to the client.
 *
 * This function sends the On/Off status message to the client in response to a
 * Generic OnOff Set or Get request. It uses the provided model and context to
 * construct and send the message.
 *
 * @param[in] model         The model instance that is sending the status.
 * @param[in] ctx           The context containing information about the message.
 * @param[in] on_off_state  The current On/Off state to be sent in the status message.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully sent the status message.
 *     - MESHX_INVALID_ARG: Invalid argument provided.
 *     - MESHX_ERR_PLAT: Platform-specific error occurred.
 */
meshx_err_t meshx_gen_on_off_srv_status_send(
    meshx_model_t *model,
    meshx_ctx_t *ctx,
    uint8_t on_off_state
)
{
    if (!model || !ctx)
    {
        return MESHX_INVALID_ARG;
    }
    ctx->opcode = MESHX_MODEL_OP_GEN_ONOFF_STATUS;
    meshx_gen_srv_state_change_t state_change = {
        .onoff_set = {
            .onoff = on_off_state
        }
    };
    return meshx_gen_srv_status_send(
            model,
            ctx,
            state_change,
            sizeof(meshx_state_change_gen_onoff_set_t)
    );
}
/**
 * @brief Initialize the On/Off server model.
 *
 * This function initializes the On/Off server model for the BLE mesh node.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_FAIL: Failure
 */
meshx_err_t meshx_on_off_server_init(void)
{
    meshx_err_t err = MESHX_SUCCESS;
#if CONFIG_ENABLE_SERVER_COMMON
    err = meshx_gen_srv_init();
    if (err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to initialize meshx server");
    }
#endif /* CONFIG_ENABLE_SERVER_COMMON */
    err = meshx_gen_srv_reg_cb(
            MESHX_MODEL_ID_GEN_ONOFF_SRV,
            (meshx_server_cb)&meshx_handle_gen_onoff_msg
        );
    if (err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to initialize meshx_gen_srv_reg_cb (Err: %d)", err);
    }

    return err;
}

/**
 * @brief Create and initialize a new On/Off server model instance.
 *
 * This function allocates memory for a new On/Off server model and initializes
 * it using the platform-specific creation function. It ensures that the model
 * is properly set up for handling Generic OnOff messages in a BLE Mesh network.
 *
 * @param[in,out] p_model Pointer to a pointer where the newly created On/Off server model
 *                instance will be stored.
 * @param[in,out] p_sig_model Pointer to a pointer where the offset of the model will be stored.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully created and initialized the model.
 *     - MESHX_INVALID_ARG: The provided pointer is NULL.
 *     - MESHX_NO_MEM: Memory allocation failed.
 */
meshx_err_t meshx_on_off_server_create(meshx_onoff_server_model_t **p_model, void *p_sig_model)
{
    if (!p_model || !p_sig_model)
    {
        return MESHX_INVALID_ARG;
    }

    *p_model = (meshx_onoff_server_model_t *)MESHX_CALOC(1, sizeof(meshx_onoff_server_model_t));
    if (!*p_model)
    {
        return MESHX_NO_MEM;
    }

    return meshx_plat_on_off_gen_srv_create(
        p_sig_model,
        &((*p_model)->meshx_server_pub),
        &((*p_model)->meshx_server_onoff_gen_srv));
}

/**
 * @brief Delete the On/Off server model instance.
 *
 * This function deletes an instance of the On/Off server model, freeing
 * associated resources and setting the model pointer to NULL.
 *
 * @param[in,out] p_model Double pointer to the On/Off server model instance to be deleted.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully deleted the model.
 *     - MESHX_INVALID_ARG: Invalid argument provided.
 */
meshx_err_t meshx_on_off_server_delete(meshx_onoff_server_model_t **p_model)
{
    if (p_model == NULL || *p_model == NULL)
    {
        return MESHX_INVALID_ARG;
    }

    meshx_plat_gen_srv_delete(
        &((*p_model)->meshx_server_pub),
        &((*p_model)->meshx_server_onoff_gen_srv)
    );

    MESHX_FREE(*p_model);
    *p_model = NULL;

    return MESHX_SUCCESS;
}

/**
 * @brief Restore the On/Off state for the generic server model.
 *
 * This function restores the On/Off state of the specified server model
 * using the provided state value. It checks for a valid model pointer
 * before proceeding with the restoration.
 *
 * @param[in] p_model       Pointer to the On/Off server model structure.
 * @param[in] onoff_state   The On/Off state to be restored.
 *
 * @return
 *     - MESHX_INVALID_STATE: If the model pointer is NULL.
 *     - Result of the platform-specific restoration function.
 */
meshx_err_t meshx_gen_on_off_srv_state_restore(meshx_ptr_t p_model, meshx_on_off_srv_el_state_t onoff_state)
{
    if(!p_model)
        return MESHX_INVALID_STATE;

    return meshx_plat_gen_on_off_srv_restore(p_model, onoff_state.on_off);
}

/**
 * @brief Create a message packet for sending On/Off status.
 *
 * This function prepares a message packet containing the On/Off status
 * information to be sent to a client. It populates the provided
 * `meshx_gen_srv_cb_param_t` structure with the necessary details.
 *
 * @param[in] p_model       Pointer to the model instance sending the status.
 * @param[in] element_id    The element ID associated with the model.
 * @param[in] key_id        The network key index to be used for sending the message.
 * @param[in] app_id        The application key index to be used for sending the message.
 * @param[in] addr          The destination address to which the message is sent.
 * @param[in] state         The On/Off state value to be included in the message.
 * @param[out] p_send_pack  Pointer to the structure where the message packet will be created.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully created the message packet.
 *     - MESHX_INVALID_ARG: Invalid argument provided (NULL pointers).
 */
meshx_err_t meshx_gen_on_off_srv_send_pack_create(
    meshx_ptr_t p_model,
    uint16_t element_id,
    uint8_t key_id,
    uint8_t app_id,
    uint16_t addr,
    uint8_t state,
    meshx_gen_srv_cb_param_t *p_send_pack
)
{
    if (!p_model || !p_send_pack)
    {
        return MESHX_INVALID_ARG;
    }

    memset(p_send_pack, 0, sizeof(meshx_gen_srv_cb_param_t));

    p_send_pack->ctx.net_idx    = key_id;
    p_send_pack->ctx.app_idx    = app_id;
    p_send_pack->ctx.dst_addr   = addr;
    p_send_pack->ctx.opcode     = MESHX_MODEL_OP_GEN_ONOFF_STATUS;
    p_send_pack->model.el_id    = element_id;
    p_send_pack->model.p_model  = p_model;

    p_send_pack->state_change.onoff_set.onoff = state;

    return MESHX_SUCCESS;
}
