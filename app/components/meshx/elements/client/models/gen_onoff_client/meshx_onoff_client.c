/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_onoff_client.c
 * @brief Implementation of the Generic OnOff Client Model for BLE Mesh.
 *
 * This file provides the implementation of the Generic OnOff Client Model
 * used in BLE Mesh networks. It includes functions to initialize the client,
 * register callbacks, and handle BLE Mesh events related to the OnOff Client.
 */

#include "meshx_err.h"
#include "meshx_onoff_client.h"

#define MESHX_CLIENT_INIT_MAGIC 0x2378

static uint16_t meshx_client_init_flag = 0;

static struct{
    uint16_t addr;
    uint16_t opcode;
    uint16_t net_idx;
    uint16_t app_idx;
    meshx_ptr_t p_model;
    meshx_gen_cli_set_t state;
}onoff_client_last_msg_ctx;

/**
 * @brief Perform hardware change based on the BLE Mesh generic server callback parameter.
 *
 * This function is responsible for executing the necessary hardware changes
 * when a BLE Mesh generic server event occurs.
 *
 * @param[in] param Pointer to the BLE Mesh generic server callback parameter structure.
 * @param[in] status Status of the operation (success or timeout).
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_FAIL: Failure
 */
static meshx_err_t meshx_state_change_notify(meshx_gen_cli_cb_param_t *param, uint8_t status)
{
    if (!param)
        return MESHX_INVALID_ARG;

    meshx_on_off_cli_el_msg_t cli_onoff_param = {
        .err_code = status,
        .ctx = param->ctx,
        .model = param->model,
        .on_off_state = param->status.onoff_status.present_onoff
    };

    return control_task_msg_publish(
            CONTROL_TASK_MSG_CODE_EL_STATE_CH,
            CONTROL_TASK_MSG_EVT_EL_STATE_CH_SET_ON_OFF,
            &cli_onoff_param,
            sizeof(meshx_on_off_cli_el_msg_t));

}

/**
 * @brief Handle timeout for relaying the last OnOff message.
 *
 * This function is called when a timeout occurs while waiting for an acknowledgment
 * for the last sent OnOff message. It attempts to resend the message using the
 * stored context information.
 *
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
static meshx_err_t meshx_relay_client_timeout_handler(void)
{
    if(!onoff_client_last_msg_ctx.p_model)
        return MESHX_INVALID_STATE;

    MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Timeout");

    return meshx_onoff_client_send_msg(
        onoff_client_last_msg_ctx.p_model,
        onoff_client_last_msg_ctx.opcode,
        onoff_client_last_msg_ctx.addr,
        onoff_client_last_msg_ctx.net_idx,
        onoff_client_last_msg_ctx.app_idx,
        onoff_client_last_msg_ctx.state.onoff_set.onoff,
        onoff_client_last_msg_ctx.state.onoff_set.tid
    );
}

/**
 * @brief Handle the Generic OnOff Client messages.
 *
 * This function processes the incoming messages for the Generic OnOff Client
 * and performs the necessary actions based on the message event and parameters.
 *
 * @param[in] pdev      Pointer to the device structure containing device-specific information.
 * @param[in] model_id  The model ID of the received message.
 * @param[in] param     Pointer to the Generic Client callback parameter structure.
 *
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
static meshx_err_t meshx_handle_gen_onoff_msg(
    const dev_struct_t *pdev,
    control_task_msg_evt_t model_id,
    meshx_gen_cli_cb_param_t *param
)
{
    if(!param || !pdev)
    {
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Invalid parameters");
        return MESHX_INVALID_ARG;
    }
    if(model_id != MESHX_MODEL_ID_GEN_ONOFF_CLI)
    {
        return MESHX_SUCCESS;
    }
    MESHX_LOGD(MODULE_ID_MODEL_CLIENT, "op|src|dst:%04" PRIx32 "|%04x|%04x",
               param->ctx.opcode, param->ctx.src_addr, param->ctx.dst_addr);

    meshx_err_t err = MESHX_SUCCESS;

    switch (param->evt)
    {
    case MESHX_GEN_CLI_EVT_GET:
    case MESHX_GEN_CLI_EVT_SET:
    case MESHX_GEN_CLI_PUBLISH:
        err = meshx_state_change_notify(param, MESHX_SUCCESS);
        break;
    case MESHX_GEN_CLI_TIMEOUT:
        err = meshx_relay_client_timeout_handler();
        if(err != MESHX_SUCCESS)
            MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Resend failed: %d", err);
        else
            err = meshx_state_change_notify(param, MESHX_TIMEOUT);
        break;
    default:
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Unhandled event: %d", param->evt);
        break;
    }
    return err;
}

/**
 * @brief Initialize the Generic OnOff Client.
 *
 * This function initializes the OnOff Client by registering the BLE Mesh
 * generic client callback.
 *
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
meshx_err_t meshx_on_off_client_init(void)
{
    meshx_err_t err = MESHX_SUCCESS;

    if (meshx_client_init_flag == MESHX_CLIENT_INIT_MAGIC)
    {
        return MESHX_SUCCESS;
    }
    meshx_client_init_flag = MESHX_CLIENT_INIT_MAGIC;

    err = meshx_gen_client_init();
    if (err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Failed to initialize meshx client");
    }

    err = meshx_gen_client_from_ble_reg_cb(
            MESHX_MODEL_ID_GEN_ONOFF_CLI,
            (meshx_gen_client_cb_t)&meshx_handle_gen_onoff_msg
        );
    if (err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to initialize meshx_gen_srv_reg_cb (Err: %d)", err);
    }

    return err;
}

/**
 * @brief Creates and initializes a Generic OnOff Client model instance.
 *
 * This function allocates and sets up a Generic OnOff Client model, associating it with the provided
 * SIG model context.
 *
 * @param[out] p_model      Pointer to a pointer where the created model instance will be stored.
 * @param[in]  p_sig_model  Pointer to the SIG model context to associate with the client model.
 *
 * @return meshx_err_t      Returns an error code indicating the result of the operation.
 *                         - MESHX_OK on success
 *                         - Appropriate error code otherwise
 */
meshx_err_t meshx_on_off_client_create(meshx_onoff_client_model_t **p_model, void *p_sig_model)
{
    if (!p_model || !p_sig_model)
    {
        return MESHX_INVALID_ARG;
    }

    *p_model = (meshx_onoff_client_model_t *)MESHX_CALOC(1, sizeof(meshx_onoff_client_model_t));
    if (!*p_model)
    {
        return MESHX_NO_MEM;
    }

    return meshx_plat_on_off_gen_cli_create(
        p_sig_model,
        &((*p_model)->meshx_onoff_client_pub),
        &((*p_model)->meshx_onoff_client_gen_cli));
}

/**
 * @brief Delete the On/Off client model instance.
 *
 * This function deletes an instance of the On/Off client model, freeing
 * associated resources and setting the model pointer to NULL.
 *
 * @param[in,out] p_model Double pointer to the On/Off client model instance to be deleted.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully deleted the model.
 *     - MESHX_INVALID_ARG: Invalid argument provided.
 */
meshx_err_t meshx_on_off_client_delete(meshx_onoff_client_model_t **p_model)
{
    if (p_model == NULL || *p_model == NULL)
    {
        return MESHX_INVALID_ARG;
    }

    meshx_plat_gen_cli_delete(
        &((*p_model)->meshx_onoff_client_pub),
        &((*p_model)->meshx_onoff_client_gen_cli)
    );

    MESHX_FREE(*p_model);
    *p_model = NULL;

    return MESHX_SUCCESS;
}

/**
 * @brief Send a generic on/off client message.
 *
 * This function sends a generic on/off client message with the specified parameters.
 *
 * @param[in] model   Pointer to the BLE Mesh model structure.
 * @param[in] opcode  The operation code of the message.
 * @param[in] addr    The destination address to which the message is sent.
 * @param[in] net_idx The network index to be used for sending the message.
 * @param[in] app_idx The application index to be used for sending the message.
 * @param[in] state   The state value to be sent in the message.
 * @param[in] tid     The transaction ID to be used for the message.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_INVALID_ARG: Invalid argument
 *    - MESHX_NO_MEM: Out of memory
 *    - MESHX_FAIL: Sending message failed
 */
meshx_err_t meshx_onoff_client_send_msg(
        meshx_onoff_client_model_t *model,
        uint16_t opcode,  uint16_t addr,
        uint16_t net_idx, uint16_t app_idx,
        uint8_t  state,   uint8_t tid
)
{
    meshx_err_t err;
    meshx_gen_cli_set_t set = {0};
    if (!model || !model->meshx_onoff_client_sig_model)
    {
        return MESHX_INVALID_ARG;
    }

    onoff_client_last_msg_ctx.addr = addr;
    onoff_client_last_msg_ctx.opcode = opcode;
    onoff_client_last_msg_ctx.p_model = model;
    onoff_client_last_msg_ctx.net_idx = net_idx;
    onoff_client_last_msg_ctx.app_idx = app_idx;

    if(opcode == MESHX_MODEL_OP_GEN_ONOFF_GET)
    {
        err = meshx_gen_cli_send_msg(
            model->meshx_onoff_client_sig_model,
            &set, opcode,
            addr, net_idx,
            app_idx
        );
    }

    else if (opcode == MESHX_MODEL_OP_GEN_ONOFF_SET ||
        opcode == MESHX_MODEL_OP_GEN_ONOFF_SET_UNACK)
    {
        set.onoff_set.tid   = tid;
        set.onoff_set.onoff = state;
        set.onoff_set.op_en = false;

        memcpy(&onoff_client_last_msg_ctx.state, &set, sizeof(meshx_gen_cli_set_t));

        err = meshx_gen_cli_send_msg(
            model->meshx_onoff_client_sig_model,
            &set, opcode,
            addr, net_idx,
            app_idx
        );
    }
    else{
        err = MESHX_INVALID_ARG; // Invalid opcode
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Invalid opcode for Generic OnOff Client: %04x", opcode);
    }
    return err;
}
