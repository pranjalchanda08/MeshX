/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_light_ctl_client.c
 * @brief Implementation of the Light CTL Client model for ESP32 BLE Mesh.
 *
 * This file contains the implementation of the Light CTL Client model, including
 * initialization, callback registration, and event handling.
 *
 * @author Pranjal Chanda
 */

#include "meshx_err.h"
#include "meshx_light_ctl_client.h"

#define LIGHT_CTL_CLIENT_INIT_MAGIC 0x8932

#if CONFIG_LIGHT_CTL_CLIENT_COUNT > 0

static uint16_t light_ctl_client_init_flag = 0;

/**
 * @brief Notifies about a change in the CTL (Color Temperature Lightness) state.
 *
 * This function is called to notify the application or upper layers when the CTL state
 * of a light device has changed. It provides the relevant parameters describing the new state.
 *
 * @param[in] param Pointer to a structure containing the CTL state change parameters.
 * @param[in] status Status code indicating the result of the notification operation.
 *
 * @return meshx_err_t Returns an error code indicating the result of the notification operation.
 */
static meshx_err_t meshx_ctl_state_change_notify(const meshx_gen_light_cli_cb_param_t *param, uint8_t status)
{
    if (!param)
        return MESHX_INVALID_ARG;

    meshx_ctl_cli_el_msg_t el_light_ctl_param = {
        .err_code = status,
        .ctx = param->ctx,
        .model = param->model
    };
    switch (param->ctx.opcode)
    {
    case MESHX_MODEL_OP_LIGHT_CTL_STATUS:
        el_light_ctl_param.ctl_state.lightness = param->status.ctl_status.present_ctl_lightness;
        el_light_ctl_param.ctl_state.temperature = param->status.ctl_status.present_ctl_temperature;
        break;
    case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_STATUS:
        el_light_ctl_param.ctl_state.delta_uv = param->status.ctl_temperature_status.present_ctl_delta_uv;
        el_light_ctl_param.ctl_state.temperature = param->status.ctl_temperature_status.present_ctl_temperature;
        break;
    case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_STATUS:
        el_light_ctl_param.ctl_state.delta_uv = param->status.ctl_temperature_range_status.range_max;
        el_light_ctl_param.ctl_state.temperature = param->status.ctl_temperature_range_status.range_min;
        break;
    case MESHX_MODEL_OP_LIGHT_CTL_DEFAULT_STATUS:
        el_light_ctl_param.ctl_state.delta_uv = param->status.ctl_default_status.delta_uv;
        el_light_ctl_param.ctl_state.lightness = param->status.ctl_default_status.lightness;
        el_light_ctl_param.ctl_state.temperature = param->status.ctl_default_status.temperature;
        break;

    default:
        break;
    }
    if(param->evt == MESHX_GEN_LIGHT_CLI_TIMEOUT)
    {
        el_light_ctl_param.err_code = MESHX_TIMEOUT;
    }

    return control_task_msg_publish(
        CONTROL_TASK_MSG_CODE_EL_STATE_CH,
        CONTROL_TASK_MSG_EVT_EL_STATE_CH_SET_CTL,
        &el_light_ctl_param,
        sizeof(meshx_ctl_cli_el_msg_t));

}

/**
 * @brief Handles generic light model messages for the Light CTL client.
 *
 * This function processes incoming messages related to the generic light model
 * and performs the necessary actions based on the message event and parameters.
 *
 * @param[in] pdev      Pointer to the device structure containing device-specific information.
 * @param[in] model_id  Identifier for the control task message event associated with the model.
 * @param[in,out] param Pointer to the callback parameter structure for the generic light client.
 *
 * @return meshx_err_t  Error code indicating the result of the operation.
 */
static meshx_err_t meshx_handle_gen_light_msg(
    const dev_struct_t *pdev,
    control_task_msg_evt_t model_id,
    const meshx_gen_light_cli_cb_param_t *param
)
{
    if (model_id != MESHX_MODEL_ID_LIGHT_CTL_CLI || !pdev || !param)
        return MESHX_INVALID_ARG;

    MESHX_LOGD(MODULE_ID_MODEL_SERVER, "op|src|dst:%04" PRIx32 "|%04x|%04x",
               param->ctx.opcode, param->ctx.src_addr, param->ctx.dst_addr);
    meshx_err_t err = param->evt == MESHX_GEN_LIGHT_CLI_TIMEOUT ?
        meshx_ctl_state_change_notify(param, MESHX_TIMEOUT):
        meshx_ctl_state_change_notify(param, MESHX_SUCCESS);

    return err;
}

/**
 * @brief Initialize the Light CTL Client model.
 *
 * This function initializes the Light CTL Client model by registering the
 * Light Client callback with the BLE Mesh stack.
 *
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
meshx_err_t meshx_light_ctl_client_init()
{
    meshx_err_t err = MESHX_SUCCESS;

    if (light_ctl_client_init_flag == LIGHT_CTL_CLIENT_INIT_MAGIC)
        return MESHX_SUCCESS;

    light_ctl_client_init_flag = LIGHT_CTL_CLIENT_INIT_MAGIC;

    err = meshx_gen_light_cli_init();
    if (err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Failed to initialize meshx client");
    }

    err = meshx_gen_light_client_from_ble_reg_cb(
        MESHX_MODEL_ID_LIGHT_CTL_CLI,
        (meshx_gen_light_client_cb_t)&meshx_handle_gen_light_msg
    );
    if (err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Failed to register Light CTL Client callback: %d", err);
    }

    return err;
}

/**
 * @brief Creates and initializes a Generic Light Client model instance.
 *
 * This function allocates and sets up a Generic Light Client model, associating it with the provided
 * SIG model context.
 *
 * @param[out] p_model      Pointer to a pointer where the created model instance will be stored.
 * @param[in]  p_sig_model  Pointer to the SIG model context to associate with the client model.
 *
 * @return meshx_err_t      Returns an error code indicating the result of the operation.
 *                         - MESHX_OK on success
 *                         - Appropriate error code otherwise
 */
meshx_err_t meshx_light_ctl_client_create(meshx_light_ctl_client_model_t **p_model, void *p_sig_model)
{
    if (!p_model || !p_sig_model)
    {
        return MESHX_INVALID_ARG;
    }

    *p_model = (meshx_light_ctl_client_model_t *)MESHX_CALOC(1, sizeof(meshx_light_ctl_client_model_t));
    if (!*p_model)
    {
        return MESHX_NO_MEM;
    }

    return meshx_plat_light_ctl_client_create(
        p_sig_model,
        &((*p_model)->meshx_pub),
        &((*p_model)->meshx_gen));
}

/**
 * @brief Delete the Light client model instance.
 *
 * This function deletes an instance of the Light client model, freeing
 * associated resources and setting the model pointer to NULL.
 *
 * @param[in,out] p_model Double pointer to the Light client model instance to be deleted.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully deleted the model.
 *     - MESHX_INVALID_ARG: Invalid argument provided.
 */
meshx_err_t meshx_light_ctl_client_delete(meshx_light_ctl_client_model_t **p_model)
{
    if (p_model == NULL || *p_model == NULL)
    {
        return MESHX_INVALID_ARG;
    }

    meshx_plat_light_client_delete(
        &((*p_model)->meshx_pub),
        &((*p_model)->meshx_gen)
    );

    MESHX_FREE(*p_model);
    *p_model = NULL;

    return MESHX_SUCCESS;
}

/**
 * @brief Sends a Light CTL (Color Temperature Lightness) message from the Light CTL Client model.
 *
 * This function constructs and sends a Light CTL message to a specified destination address
 * using the provided network and application indices. The message contains the desired lightness,
 * temperature, and a transaction identifier (TID).
 *
 * @param[in] params Pointer to a structure containing the message parameters.
 *
 * @return meshx_err_t     Returns the result of the message send operation.
 */
meshx_err_t meshx_light_ctl_client_send_msg(meshx_gen_ctl_send_params_t *params)
{
    meshx_light_client_set_state_t set = {0};
    if (!params || !params->model || !params->model->meshx_sig)
    {
        return MESHX_INVALID_ARG; // Invalid model pointer
    }
    meshx_gen_light_client_send_params_t send_params = {
        .state      = &set,
        .addr       = params->addr,
        .opcode     = params->opcode,
        .app_idx    = params->app_idx,
        .net_idx    = params->net_idx,
        .model      = params->model->meshx_sig,
    };

    if (params->opcode == MESHX_MODEL_OP_LIGHT_CTL_GET)
    {
        MESHX_DO_NOTHING;
    }
    else if (params->opcode == MESHX_MODEL_OP_LIGHT_CTL_SET ||
             params->opcode == MESHX_MODEL_OP_LIGHT_CTL_SET_UNACK)
    {
        set.ctl_set.op_en           = false;
        set.ctl_set.tid             = params->tid;
        set.ctl_set.ctl_delta_uv    = params->delta_uv;
        set.ctl_set.ctl_lightness   = params->lightness;
        set.ctl_set.ctl_temperature = params->temperature;
    }
    else{
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Invalid opcode for Light CTL Client: %04x", params->opcode);
        return MESHX_INVALID_ARG;
    }
    return meshx_gen_light_send_msg(&send_params);
}

/**
 * @brief Sends a Light CTL Temperature message from the client model.
 *
 * This function constructs and sends a Light CTL Temperature message to a specified address
 * using the provided network and application indices. It allows the client to control the
 * color temperature and delta UV of a lighting element in a mesh network.
 *
 * @param[in] params Pointer to a structure containing the message parameters.
 *
 * @return meshx_err_t     Result of the message send operation.
 */
meshx_err_t meshx_light_ctl_temperature_client_send_msg(meshx_gen_ctl_send_params_t *params)
{
    meshx_light_client_set_state_t set = {0};
    if (!params || !params->model || !params->model->meshx_sig)
    {
        return MESHX_INVALID_ARG; // Invalid model pointer
    }
    meshx_gen_light_client_send_params_t send_params = {
        .state      = &set,
        .addr       = params->addr,
        .opcode     = params->opcode,
        .app_idx    = params->app_idx,
        .net_idx    = params->net_idx,
        .model      = params->model->meshx_sig,
    };

    if (params->opcode == MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_GET)
    {
        MESHX_DO_NOTHING;
    }
    else if (params->opcode == MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_SET ||
             params->opcode == MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_SET_UNACK)
    {
        set.ctl_temperature_set.op_en           = false;
        set.ctl_temperature_set.tid             = params->tid;
        set.ctl_temperature_set.ctl_delta_uv    = params->delta_uv;
        set.ctl_temperature_set.ctl_temperature = params->temperature;

    }
    else{
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Invalid opcode for Light CTL Client: %04x", params->opcode);
        return MESHX_INVALID_ARG;
    }

    return meshx_gen_light_send_msg(&send_params);
}

/**
 * @brief Sends a Light CTL Temperature Range message from the client model.
 *
 * This function constructs and sends a Light CTL Temperature Range message to a specified address
 * using the provided network and application indices. It allows the client to set or get the
 * temperature range of a lighting element in a mesh network.
 *
 * @param[in] params Pointer to a structure containing the message parameters.
 *
 * @return meshx_err_t     Result of the message send operation.
 */
meshx_err_t meshx_light_ctl_temp_range_client_send_msg(meshx_gen_ctl_send_params_t *params)
{
    meshx_light_client_set_state_t set = {0};
    if (!params || !params->model || !params->model->meshx_sig)
    {
        return MESHX_INVALID_ARG; // Invalid model pointer
    }
    meshx_gen_light_client_send_params_t send_params = {
        .state      = &set,
        .addr       = params->addr,
        .opcode     = params->opcode,
        .app_idx    = params->app_idx,
        .net_idx    = params->net_idx,
        .model      = params->model->meshx_sig,
    };

    if(params->opcode == MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_GET)
    {
        MESHX_DO_NOTHING;
    }
    if (params->opcode == MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_SET ||
        params->opcode == MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_SET_UNACK)
    {
        set.ctl_temperature_range_set.range_min = params->temp_range_min;
        set.ctl_temperature_range_set.range_max = params->temp_range_max;
    }
    else{
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Invalid opcode for Light CTL Client: %04x", params->opcode);
        return MESHX_INVALID_ARG;
    }

    return meshx_gen_light_send_msg(&send_params);
}

/**
 * @brief Handles state changes for the Light CTL client element.
 *
 * This function processes state change events for the Light CTL client element.
 *
 * @param[in]       param Pointer to the message structure containing the state change parameters.
 * @param[in,out]   p_ctl_prev_state Pointer to the previous state structure.
 * @param[in,out]   p_ctl_next_state Pointer to the next state structure (currently unused).
 *
 * @return meshx_err_t Returns an error code indicating the result of the handler execution.
 */
meshx_err_t meshx_light_ctl_state_change_handle(
    const meshx_ctl_cli_el_msg_t *param,
    meshx_ctl_el_state_t *p_ctl_prev_state,
    const meshx_ctl_el_state_t *p_ctl_next_state
)
{
    if (!p_ctl_prev_state || !param || !p_ctl_next_state)
        return MESHX_INVALID_ARG;

    /* Kept for future use */
    MESHX_UNUSED(p_ctl_next_state);
    bool state_change = false;
    if(param->err_code == MESHX_SUCCESS)
    {
        if (param->ctx.opcode == MESHX_MODEL_OP_LIGHT_CTL_STATUS)
        {
            if (p_ctl_prev_state->lightness != param->ctl_state.lightness
            || p_ctl_prev_state->temperature != param->ctl_state.temperature
            )
            {
                p_ctl_prev_state->lightness = param->ctl_state.lightness;
                p_ctl_prev_state->temperature = param->ctl_state.temperature;
                state_change = true;
            }
        }
        else if (param->ctx.opcode == MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_STATUS)
        {
            if(p_ctl_prev_state->delta_uv != param->ctl_state.delta_uv
            || p_ctl_prev_state->temperature != param->ctl_state.temperature)
            {
                p_ctl_prev_state->delta_uv = param->ctl_state.delta_uv;
                p_ctl_prev_state->temperature = param->ctl_state.temperature;
                state_change = true;
            }
        }
        else if (param->ctx.opcode == MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_STATUS)
        {
            if (p_ctl_prev_state->temp_range_max != param->ctl_state.temp_range_max
            ||  p_ctl_prev_state->temp_range_min != param->ctl_state.temp_range_min)
            {
                p_ctl_prev_state->temp_range_max = param->ctl_state.temp_range_max;
                p_ctl_prev_state->temp_range_min = param->ctl_state.temp_range_min;
                state_change = true;
            }
        }
        else if (param->ctx.opcode == MESHX_MODEL_OP_LIGHT_CTL_DEFAULT_STATUS)
        {
            MESHX_DO_NOTHING;
        }
        else
        {
            /* Return as No CTL related OPCode were received */
            MESHX_DO_NOTHING;
        }
    }
    else
    {
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "CWWW Client Element Message: Error (%d)", param->err_code);
        /* Retry sending the failed packet done by MeshX Light CTL Layer. Do not notify App */
        MESHX_DO_NOTHING;
    }
    return state_change ? MESHX_SUCCESS : MESHX_INVALID_STATE;
}

#endif /* CONFIG_LIGHT_CTL_CLIENT_COUNT > 0 */
