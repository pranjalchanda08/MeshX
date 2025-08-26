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

static struct{
    uint16_t addr;
    uint16_t opcode;
    uint16_t net_idx;
    uint16_t app_idx;
    meshx_ptr_t p_model;
    meshx_light_client_set_state_t state;
} light_ctl_client_last_msg_ctx;

/**
 * @brief Notifies about a change in the CTL (Color Temperature Lightness) state.
 *
 * This function is called to notify the application or upper layers when the CTL state
 * of a light device has changed. It provides the relevant parameters describing the new state.
 *
 * @param[in] param Pointer to a structure containing the CTL state change parameters.
 *
 * @return meshx_err_t Returns an error code indicating the result of the notification operation.
 */
static meshx_err_t meshx_ctl_state_change_notify(meshx_gen_light_cli_cb_param_t *param)
{
    if (!param)
        return MESHX_INVALID_ARG;

    meshx_ctl_cli_el_msg_t el_light_ctl_param = {
        .err_code = MESHX_SUCCESS,
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
 * @brief Handle timeout for relaying the last Light CTL message.
 *
 * This function is called when a timeout occurs while waiting for an acknowledgment
 * for the last sent Light CTL message. It attempts to resend the message using the
 * stored context information.
 *
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
static meshx_err_t meshx_light_ctl_client_timeout_handler(void)
{
    if(!light_ctl_client_last_msg_ctx.p_model)
        return MESHX_INVALID_STATE;

    MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Timeout");

    switch (light_ctl_client_last_msg_ctx.opcode)
    {
        case MESHX_MODEL_OP_LIGHT_CTL_GET:
        case MESHX_MODEL_OP_LIGHT_CTL_SET:
        case MESHX_MODEL_OP_LIGHT_CTL_SET_UNACK:
            return meshx_light_ctl_client_send_msg(
                light_ctl_client_last_msg_ctx.p_model,
                light_ctl_client_last_msg_ctx.opcode,
                light_ctl_client_last_msg_ctx.addr,
                light_ctl_client_last_msg_ctx.net_idx,
                light_ctl_client_last_msg_ctx.app_idx,
                light_ctl_client_last_msg_ctx.state.ctl_set.ctl_lightness,
                light_ctl_client_last_msg_ctx.state.ctl_set.ctl_temperature,
                light_ctl_client_last_msg_ctx.state.ctl_set.ctl_delta_uv,
                light_ctl_client_last_msg_ctx.state.ctl_set.tid
            );
        case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_GET:
        case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_SET:
        case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_SET_UNACK:
            return meshx_light_ctl_temperature_client_send_msg(
                light_ctl_client_last_msg_ctx.p_model,
                light_ctl_client_last_msg_ctx.opcode,
                light_ctl_client_last_msg_ctx.addr,
                light_ctl_client_last_msg_ctx.net_idx,
                light_ctl_client_last_msg_ctx.app_idx,
                light_ctl_client_last_msg_ctx.state.ctl_set.ctl_temperature,
                light_ctl_client_last_msg_ctx.state.ctl_set.ctl_delta_uv,
                light_ctl_client_last_msg_ctx.state.ctl_set.tid
            );
        case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_GET:
        case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_SET:
        case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_SET_UNACK:
            return meshx_light_ctl_temp_range_client_send_msg(
                light_ctl_client_last_msg_ctx.p_model,
                light_ctl_client_last_msg_ctx.opcode,
                light_ctl_client_last_msg_ctx.addr,
                light_ctl_client_last_msg_ctx.net_idx,
                light_ctl_client_last_msg_ctx.app_idx,
                light_ctl_client_last_msg_ctx.state.ctl_temperature_range_set.range_min,
                light_ctl_client_last_msg_ctx.state.ctl_temperature_range_set.range_max
            );
        default:
            return MESHX_INVALID_STATE;
    }
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
    meshx_gen_light_cli_cb_param_t *param
)
{
    if (model_id != MESHX_MODEL_ID_LIGHT_CTL_CLI || !pdev || !param)
        return MESHX_INVALID_ARG;

    MESHX_LOGD(MODULE_ID_MODEL_SERVER, "op|src|dst:%04" PRIx32 "|%04x|%04x",
               param->ctx.opcode, param->ctx.src_addr, param->ctx.dst_addr);
    meshx_err_t err = MESHX_SUCCESS;

    switch (param->evt)
    {
    case MESHX_GEN_LIGHT_CLI_EVT_GET:
    case MESHX_GEN_LIGHT_CLI_EVT_SET:
    case MESHX_GEN_LIGHT_CLI_PUBLISH:
        err = meshx_ctl_state_change_notify(param);
        if (err)
        {
            MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Failed to notify state change: %d", err);
        }
        break;
    case MESHX_GEN_LIGHT_CLI_TIMEOUT:
        err = meshx_light_ctl_client_timeout_handler();
        if(err != MESHX_SUCCESS)
            MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Resend failed: %d", err);

        else
            err = meshx_ctl_state_change_notify(param);
        break;
    default:
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Unknown event: %d", param->evt);
        err = MESHX_NOT_SUPPORTED;
        break;
    }

    return err;
}

/**
 * @brief Registers a callback function for the Light CTL (Color Temperature Lightness) client model.
 *
 * This function associates a user-defined callback with a specific Light CTL client model,
 * allowing the application to handle events or responses related to the model.
 *
 * @param[in] model_id The unique identifier of the Light CTL client model instance.
 * @param[in] cb       The callback function to be registered. This function will be called
 *                     when relevant events occur for the specified model.
 *
 * @return meshx_err_t Returns MESHX_OK on success, or an appropriate error code on failure.
 */
static meshx_err_t meshx_light_ctl_cli_reg_cb(uint32_t model_id, meshx_gen_light_client_cb_t cb)
{
    if (cb == NULL || model_id != MESHX_MODEL_ID_LIGHT_CTL_CLI)
        return MESHX_INVALID_ARG; // Invalid arguments

    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_FRM_BLE,
        model_id,
        cb);
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

    err = meshx_light_ctl_cli_reg_cb(
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
        &((*p_model)->meshx_light_ctl_client_pub),
        &((*p_model)->meshx_light_ctl_client_gen_cli));
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
        &((*p_model)->meshx_light_ctl_client_pub),
        &((*p_model)->meshx_light_ctl_client_gen_cli)
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
 * @param[in] model        Pointer to the Light CTL Client model instance.
 * @param[in] opcode       Opcode of the Light CTL message to be sent.
 * @param[in] addr         Destination address for the message.
 * @param[in] net_idx      Network index to be used for sending the message.
 * @param[in] app_idx      Application index to be used for sending the message.
 * @param[in] lightness    Desired lightness value to be set.
 * @param[in] temperature  Desired color temperature value to be set.
 * @param[in] delta_uv     Desired delta UV value to be set.
 * @param[in] tid          Transaction Identifier for the message.
 *
 * @return meshx_err_t     Returns the result of the message send operation.
 */
meshx_err_t meshx_light_ctl_client_send_msg(
        meshx_light_ctl_client_model_t *model,
        uint16_t opcode,  uint16_t addr,
        uint16_t net_idx, uint16_t app_idx,
        uint16_t lightness, uint16_t temperature,
        uint16_t delta_uv, uint8_t tid
)
{
    meshx_err_t err;
    meshx_light_client_set_state_t set = {0};
    if (!model || !model->meshx_light_ctl_client_sig_model)
    {
        return MESHX_INVALID_ARG; // Invalid model pointer
    }

    light_ctl_client_last_msg_ctx.addr = addr;
    light_ctl_client_last_msg_ctx.opcode = opcode;
    light_ctl_client_last_msg_ctx.net_idx = net_idx;
    light_ctl_client_last_msg_ctx.app_idx = app_idx;
    light_ctl_client_last_msg_ctx.p_model = model;

    if (opcode == MESHX_MODEL_OP_LIGHT_CTL_GET)
    {
        err = meshx_gen_light_send_msg(
            model->meshx_light_ctl_client_sig_model,
            &set, opcode,
            addr, net_idx,
            app_idx
        );
    }
    else if (opcode == MESHX_MODEL_OP_LIGHT_CTL_SET ||
        opcode == MESHX_MODEL_OP_LIGHT_CTL_SET_UNACK)
    {
        set.ctl_set.tid   = tid;
        set.ctl_set.op_en = false;
        set.ctl_set.ctl_delta_uv    = delta_uv;
        set.ctl_set.ctl_lightness   = lightness;
        set.ctl_set.ctl_temperature = temperature;

        memcpy(&light_ctl_client_last_msg_ctx.state, &set, sizeof(meshx_light_client_set_state_t));

        err = meshx_gen_light_send_msg(
            model->meshx_light_ctl_client_sig_model,
            &set, opcode,
            addr, net_idx,
            app_idx
        );
    }
    else{
        err = MESHX_INVALID_ARG; // Invalid opcode
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Invalid opcode for Light CTL Client: %04x", opcode);
    }
    return err;
}

/**
 * @brief Sends a Light CTL Temperature message from the client model.
 *
 * This function constructs and sends a Light CTL Temperature message to a specified address
 * using the provided network and application indices. It allows the client to control the
 * color temperature and delta UV of a lighting element in a mesh network.
 *
 * @param[in] model        Pointer to the Light CTL client model instance.
 * @param[in] opcode       Opcode of the message to be sent.
 * @param[in] addr         Destination address of the message.
 * @param[in] net_idx      Network index to be used for sending the message.
 * @param[in] app_idx      Application index to be used for sending the message.
 * @param[in] temperature  Desired color temperature value to be set.
 * @param[in] delta_uv     Delta UV value to be set.
 * @param[in] tid          Transaction Identifier for the message.
 *
 * @return meshx_err_t     Result of the message send operation.
 */
meshx_err_t meshx_light_ctl_temperature_client_send_msg(
        meshx_light_ctl_client_model_t *model,
        uint16_t opcode,  uint16_t addr,
        uint16_t net_idx, uint16_t app_idx,
        uint16_t temperature, uint16_t delta_uv, uint8_t tid
)
{
    meshx_err_t err;
    meshx_light_client_set_state_t set = {0};

    if (!model || !model->meshx_light_ctl_client_sig_model)
    {
        return MESHX_INVALID_ARG; // Invalid model pointer
    }

    light_ctl_client_last_msg_ctx.addr = addr;
    light_ctl_client_last_msg_ctx.opcode = opcode;
    light_ctl_client_last_msg_ctx.net_idx = net_idx;
    light_ctl_client_last_msg_ctx.app_idx = app_idx;
    light_ctl_client_last_msg_ctx.p_model = model;

    if (opcode == MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_GET)
    {
        MESHX_DO_NOTHING;
    }
    else if (opcode == MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_SET ||
             opcode == MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_SET_UNACK)
    {
        set.ctl_set.tid   = tid;
        set.ctl_set.op_en = false;
        set.ctl_set.ctl_delta_uv    = delta_uv;
        set.ctl_set.ctl_temperature = temperature;

        memcpy(&light_ctl_client_last_msg_ctx.state, &set, sizeof(meshx_light_client_set_state_t));
    }
    else{
        err = MESHX_INVALID_ARG; // Invalid opcode
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Invalid opcode for Light CTL Client: %04x", opcode);
        return err;
    }

    err = meshx_gen_light_send_msg(
        model->meshx_light_ctl_client_sig_model,
        &set, opcode,
        addr, net_idx,
        app_idx
    );
    return err;
}

/**
 * @brief Sends a Light CTL Temperature Range message from the client model.
 *
 * This function constructs and sends a Light CTL Temperature Range message to a specified address
 * using the provided network and application indices. It allows the client to set or get the
 * temperature range of a lighting element in a mesh network.
 *
 * @param[in] model        Pointer to the Light CTL client model instance.
 * @param[in] opcode       Opcode of the message to be sent.
 * @param[in] addr         Destination address of the message.
 * @param[in] net_idx      Network index to be used for sending the message.
 * @param[in] app_idx      Application index to be used for sending the message.
 * @param[in] temp_min     Minimum temperature value of the range to be set.
 * @param[in] temp_max     Maximum temperature value of the range to be set.
 *
 * @return meshx_err_t     Result of the message send operation.
 */
meshx_err_t meshx_light_ctl_temp_range_client_send_msg(
        meshx_light_ctl_client_model_t *model,
        uint16_t opcode,  uint16_t addr,
        uint16_t net_idx, uint16_t app_idx,
        uint16_t temp_min, uint16_t temp_max
)
{
    meshx_err_t err;
    meshx_light_client_set_state_t set = {0};
    if (!model || !model->meshx_light_ctl_client_sig_model)
    {
        return MESHX_INVALID_ARG; // Invalid model pointer
    }
    light_ctl_client_last_msg_ctx.addr = addr;
    light_ctl_client_last_msg_ctx.opcode = opcode;
    light_ctl_client_last_msg_ctx.net_idx = net_idx;
    light_ctl_client_last_msg_ctx.app_idx = app_idx;
    light_ctl_client_last_msg_ctx.p_model = model;

    if(opcode == MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_GET)
    {
        MESHX_DO_NOTHING;
    }
    if (opcode == MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_SET ||
        opcode == MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_SET_UNACK)
    {
        set.ctl_temperature_range_set.range_min = temp_min;
        set.ctl_temperature_range_set.range_max = temp_max;

        memcpy(&light_ctl_client_last_msg_ctx.state, &set, sizeof(meshx_light_client_set_state_t));
    }
    else{
        err = MESHX_INVALID_ARG; // Invalid opcode
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Invalid opcode for Light CTL Client: %04x", opcode);
        return err;
    }

    err = meshx_gen_light_send_msg(
        model->meshx_light_ctl_client_sig_model,
        &set, opcode,
        addr, net_idx,
        app_idx
    );
    return err;
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
    meshx_ctl_cli_el_msg_t *param,
    meshx_ctl_el_state_t *p_ctl_prev_state,
    meshx_ctl_el_state_t *p_ctl_next_state
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
