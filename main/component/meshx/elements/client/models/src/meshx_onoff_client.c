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
#include "meshx_txcm.h"
#include "meshx_onoff_client.h"

#define MESHX_CLIENT_INIT_MAGIC 0x2378

#if CONFIG_ENABLE_GEN_ONOFF_CLIENT

static uint16_t meshx_client_init_flag = 0;

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
static meshx_err_t meshx_state_change_notify(const meshx_gen_cli_cb_param_t *param, uint8_t status)
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
    const meshx_gen_cli_cb_param_t *param
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

    meshx_err_t err = param->evt == MESHX_GEN_CLI_TIMEOUT ?
        meshx_state_change_notify(param, MESHX_TIMEOUT) :
        meshx_state_change_notify(param, MESHX_SUCCESS);

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
        &((*p_model)->meshx_pub),
        &((*p_model)->meshx_gen));
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
        &((*p_model)->meshx_pub),
        &((*p_model)->meshx_gen)
    );

    MESHX_FREE(*p_model);
    *p_model = NULL;

    return MESHX_SUCCESS;
}

/**
 * @brief Send a Generic OnOff client message.
 *
 * This function sends a Generic OnOff client message with the specified parameters.
 *
 * @param[in] params   Pointer to the structure containing the message parameters to set.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_INVALID_ARG: Invalid argument
 *     - MESHX_NO_MEM: Out of memory
 *     - MESHX_FAIL: Sending message failed
 */
meshx_err_t meshx_onoff_client_send_msg(meshx_gen_onoff_send_params_t *params)
{
    meshx_err_t err;
    meshx_gen_cli_set_t set = {0};
    if (!params || !params->model || !params->model->meshx_sig)
    {
        return MESHX_INVALID_ARG;
    }

    meshx_gen_client_send_params_t send_params = {
        .state   = &set,
        .addr    = params->addr,
        .opcode  = params->opcode,
        .app_idx = params->app_idx,
        .net_idx = params->net_idx,
        .model   = params->model->meshx_sig,
    };

    if(params->opcode == MESHX_MODEL_OP_GEN_ONOFF_GET)
    {
        err = meshx_gen_cli_send_msg(&send_params);
    }

    else if (params->opcode == MESHX_MODEL_OP_GEN_ONOFF_SET ||
        params->opcode == MESHX_MODEL_OP_GEN_ONOFF_SET_UNACK)
    {
        set.onoff_set.tid   = params->tid;
        set.onoff_set.onoff = params->state;
        set.onoff_set.op_en = false;

        err = meshx_gen_cli_send_msg(
            &send_params
        );
    }
    else{
        err = MESHX_INVALID_ARG; // Invalid opcode
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Invalid opcode for Generic OnOff Client: %04x", params->opcode);
    }
    return err;
}

/**
 * @brief Handle state changes for the Generic OnOff Client.
 *
 * This function processes state change events for the Generic OnOff Client,
 * updating the previous and next states based on the received message parameters.
 *
 * @param[in]       param           Pointer to the message structure containing the state change parameters.
 * @param[in,out]   p_prev_state    Pointer to the previous state structure.
 * @param[in,out]   p_next_state    Pointer to the next state structure.
 *
 * @return meshx_err_t Returns an error code indicating the result of the handler execution.
 *                     - MESHX_SUCCESS if a state change occurred
 *                     - MESHX_INVALID_STATE if no state change occurred
 *                     - Appropriate error code otherwise
 */
meshx_err_t meshx_gen_on_off_state_change_handle(
    const meshx_on_off_cli_el_msg_t *param,
    meshx_on_off_cli_state_t *p_prev_state,
    meshx_on_off_cli_state_t *p_next_state
)
{
    if (!p_prev_state || !param || !p_next_state)
        return MESHX_INVALID_ARG;
    bool state_change = false;

    if(param->err_code == MESHX_SUCCESS)
    {
        if (p_prev_state->on_off != param->on_off_state)
        {
            p_prev_state->on_off = param->on_off_state;
            state_change = true;
        }
        else
        {
            MESHX_LOGD(MODULE_ID_MODEL_CLIENT, "No change in state: %d", param->on_off_state);
        }
        p_next_state->on_off = !param->on_off_state;
    }
    else
    {
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "OnOff state change failed: %d", param->err_code);
        state_change = true;
        /* state_change = true if want to notify app about timeout */
    }

    return state_change ? MESHX_SUCCESS : MESHX_INVALID_STATE;
}
#endif /* CONFIG_ENABLE_GEN_ONOFF_CLIENT*/
