/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_light_cwww_client.c
 * @brief Implementation of the CW-WW (Cool White - Warm White) client model for BLE Mesh.
 *
 * This file contains the implementation of the CW-WW client model, including initialization,
 * configuration, and message handling functions. The CW-WW client model is used to control
 * and manage CW-WW lighting devices in a BLE Mesh network.
 *
 * @note This implementation supports configuration server callbacks and generic client callbacks.
 *       It also includes functions to create and add CW-WW client models to the device's element list.
 *
 * @details
 * The CW-WW client model supports the following features:
 * - Initialization and allocation of resources for CW-WW models.
 * - Handling of configuration server events such as model publication and application key binding.
 * - Handling of generic client callback events for CW-WW models.
 * - Sending CW-WW messages to the server.
 *
 * @author Pranjal Chanda
 *
 */
#include "app_common.h"
#include "meshx_control_task.h"
#include "meshx_nvs.h"
#include "meshx_api.h"

#if CONFIG_LIGHT_CWWW_CLIENT_COUNT > 0
#include "meshx_light_cwww_client_element.h"

#if CONFIG_ENABLE_CONFIG_SERVER
#include "meshx_config_server.h"

/**
 * @brief Configuration server callback event mask for cwww server.
 */
#define CONFIG_SERVER_CB_MASK        \
        CONTROL_TASK_MSG_EVT_PUB_ADD \
    |   CONTROL_TASK_MSG_EVT_SUB_ADD \
    |   CONTROL_TASK_MSG_EVT_APP_KEY_BIND
#endif /* CONFIG_ENABLE_CONFIG_SERVER */

/**
 * @defgroup CONTROL_TASK configs
 */
#if defined(__MESHX_CONTROL_TASK__)
#define CONTROL_TASK_MSG_CODE_EVT_MASK          \
        CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF  \
    |   CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL
#endif /* __MESHX_CONTROL_TASK__ */

#define MOD_LCC                             MODULE_ID_ELEMENT_LIGHT_CWWWW_CLIENT
#define CWWW_CLI_MESHX_ONOFF_ENABLE_CB      true
#define CWWW_CLI_EL_STATE_CH_EVT_MASK       CONTROL_TASK_MSG_EVT_EL_STATE_CH_SET_ON_OFF | CONTROL_TASK_MSG_EVT_EL_STATE_CH_SET_CTL

#define IS_EL_IN_RANGE(_element_id)         ((_element_id) >= cwww_client_element_init_ctrl.element_id_start && (_element_id) < cwww_client_element_init_ctrl.element_id_end)
#define GET_RELATIVE_EL_IDX(_element_id)    ((_element_id) - cwww_client_element_init_ctrl.element_id_start)
#define CWWW_CLI_EL(_el_id)                 cwww_client_element_init_ctrl.el_list[_el_id]

static meshx_cwww_client_elements_ctrl_t cwww_client_element_init_ctrl;

#if CWWW_CLI_MESHX_ONOFF_ENABLE_CB

static meshx_err_t meshx_cwww_cli_send_onoff_msg( const dev_struct_t *pdev, uint16_t element_id, uint8_t set_get, uint8_t ack);
static meshx_err_t meshx_cwww_cli_send_ctl_msg( const dev_struct_t *pdev, uint16_t element_id, uint8_t set_get, uint8_t ack);
/**
 * @brief Handler for On/Off state change events in the CW/WW light client.
 *
 * This function processes state change messages received by the CW/WW (Cool White/Warm White)
 * light client element. It is typically called when the On/Off state of the light changes,
 * allowing the client to update its internal state or perform necessary actions.
 *
 * @param pdev Pointer to the device structure representing the client device.
 * @param param Pointer to the message structure containing On/Off state change information.
 *
 * @return meshx_err_t Error code indicating the result of the handler execution.
 */
static meshx_err_t cwww_client_on_off_state_change_handler(
    dev_struct_t const *pdev,
    const meshx_on_off_cli_el_msg_t *param)
{
    uint16_t element_id = param->model.el_id;
    if (!IS_EL_IN_RANGE(element_id))
    {
        return MESHX_SUCCESS;
    }

    size_t rel_el_id = GET_RELATIVE_EL_IDX(element_id);
    meshx_cwww_client_model_ctx_t *el_ctx = CWWW_CLI_EL(rel_el_id).cwww_cli_ctx;
    meshx_api_light_cwww_client_evt_t app_notify;
    meshx_err_t err;
    if(param->err_code == MESHX_SUCCESS)
    {
        CWWW_CLI_EL(rel_el_id).element_model_init |= MESHX_BIT(CWWW_CLI_SIG_ONOFF_MODEL_ID);
        if (el_ctx->prev_state.on_off != param->on_off_state)
        {
            el_ctx->prev_state.on_off = param->on_off_state;
        }
        app_notify.err_code = 0;
        app_notify.state_change.on_off.state = el_ctx->prev_state.on_off;

        err = meshx_send_msg_to_app(element_id,
                                    MESHX_ELEMENT_TYPE_LIGHT_CWWW_CLIENT,
                                    MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_SERVER_ONN_OFF,
                                    sizeof(meshx_api_light_cwww_client_evt_t),
                                    &app_notify);
        if (err != MESHX_SUCCESS)
        {
            MESHX_LOGE(MOD_LCC, "Failed to send CWWW state change message: (%d)", err);
        }

        el_ctx->state.on_off = !param->on_off_state;
        el_ctx->tid++;
        MESHX_LOGD(MOD_LCC, "SET|PUBLISH: %d", param->on_off_state);
        MESHX_LOGD(MOD_LCC, "Next state: %d", el_ctx->state.on_off);
    }
    else
    {
        MESHX_LOGE(MOD_LCC, "CWWW Client Element Message: Error (%d)", param->err_code);
        /* Retry sending the failed packet. Do not notify App */
        /* Please note that the failed packets gets sent endlessly. Hence, a loop condition */
        el_ctx->tid++;
        err = meshx_cwww_cli_send_onoff_msg(pdev,
                                            element_id,
                                            MESHX_GEN_ON_OFF_CLI_MSG_SET,
                                            MESHX_GEN_ON_OFF_CLI_MSG_ACK);
        if (err)
        {
            MESHX_LOGE(MOD_LCC, "CWWW Client Element Message: Retry failed (%d)", err);
        }
    }
    return err;
}

/**
 * @brief Handles state changes for the CW/WW light control client element.
 *
 * This function processes state change events for the CW/WW (Cool White/Warm White) light control client.
 * It is typically called when the on/off state of the light changes, and is responsible for updating
 * the device state or triggering further actions based on the received parameters.
 *
 * @param pdev Pointer to the device structure representing the light client element.
 * @param param Pointer to the message structure containing the on/off state change parameters.
 *
 * @return meshx_err_t Returns an error code indicating the result of the handler execution.
 */

static meshx_err_t cwww_light_ctl_state_change_handler(
    dev_struct_t const *pdev,
    const meshx_ctl_cli_el_msg_t *param)
{
    uint16_t element_id = param->model.el_id;
    if (!IS_EL_IN_RANGE(element_id))
    {
        return MESHX_SUCCESS;
    }

    size_t rel_el_id = GET_RELATIVE_EL_IDX(element_id);
    meshx_cwww_client_model_ctx_t *el_ctx = CWWW_CLI_EL(rel_el_id).cwww_cli_ctx;
    meshx_api_light_cwww_client_evt_t app_notify;
    meshx_err_t err = MESHX_SUCCESS;
    bool state_change = false;

    if(param->err_code == MESHX_SUCCESS)
    {
        CWWW_CLI_EL(rel_el_id).element_model_init |= MESHX_BIT(CWWW_CLI_SIG_L_CTL_MODEL_ID);

        if (param->ctx.opcode == MESHX_MODEL_OP_LIGHT_CTL_STATUS)
        {
            if (el_ctx->prev_ctl_state.lightness != param->ctl_state.lightness
            || el_ctx->prev_ctl_state.temperature != param->ctl_state.temperature
            )
            {
                el_ctx->prev_ctl_state.lightness = param->ctl_state.lightness;
                el_ctx->prev_ctl_state.temperature = param->ctl_state.temperature;
                state_change = true;
            }
        }
        else if (param->ctx.opcode == MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_STATUS)
        {
            if(el_ctx->prev_ctl_state.delta_uv != param->ctl_state.delta_uv
            || el_ctx->prev_ctl_state.temperature != param->ctl_state.temperature)
            {
                el_ctx->prev_ctl_state.delta_uv = param->ctl_state.delta_uv;
                el_ctx->prev_ctl_state.temperature = param->ctl_state.temperature;
                state_change = true;
            }
        }
        else if (param->ctx.opcode == MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_STATUS)
        {
            if (el_ctx->prev_ctl_state.temp_range_max != param->ctl_state.temp_range_max
            ||  el_ctx->prev_ctl_state.temp_range_min != param->ctl_state.temp_range_min)
            {
                el_ctx->prev_ctl_state.temp_range_max = param->ctl_state.temp_range_max;
                el_ctx->prev_ctl_state.temp_range_min = param->ctl_state.temp_range_min;
                state_change = true;
            }
        }
        else if (param->ctx.opcode == MESHX_MODEL_OP_LIGHT_CTL_DEFAULT_STATUS)
        {
            if( el_ctx->prev_ctl_state.temp_def != param->ctl_state.temp_def
            ||  el_ctx->prev_ctl_state.delta_uv_def != param->ctl_state.delta_uv_def
            ||  el_ctx->prev_ctl_state.lightness_def != param->ctl_state.lightness_def)
            {
                el_ctx->prev_ctl_state.temp_def = param->ctl_state.temp_def;
                el_ctx->prev_ctl_state.delta_uv_def = param->ctl_state.delta_uv_def;
                el_ctx->prev_ctl_state.lightness_def = param->ctl_state.lightness_def;
                state_change = true;
            }
        }
        else
        {
            /* Return as No CTL related OPCode were received */
            return false;
        }

        if (state_change)
        {
            MESHX_LOGD(MOD_LCC, "PUBLISH: light|temp : %d|%d",
                     el_ctx->prev_ctl_state.lightness,
                     el_ctx->prev_ctl_state.temperature);

            app_notify.err_code = MESHX_SUCCESS;
            app_notify.state_change.ctl.delta_uv        = el_ctx->prev_ctl_state.delta_uv;
            app_notify.state_change.ctl.lightness       = el_ctx->prev_ctl_state.lightness;
            app_notify.state_change.ctl.temperature     = el_ctx->prev_ctl_state.temperature;
            app_notify.state_change.ctl.temp_range_max  = el_ctx->prev_ctl_state.temp_range_max;
            app_notify.state_change.ctl.temp_range_min  = el_ctx->prev_ctl_state.temp_range_min;

            err = meshx_send_msg_to_app(element_id,
                                        MESHX_ELEMENT_TYPE_LIGHT_CWWW_CLIENT,
                                        MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_CLIENT_CTL,
                                        sizeof(meshx_api_light_cwww_client_evt_t),
                                        &app_notify);
            if (err != MESHX_SUCCESS)
            {
                MESHX_LOGE(MOD_LCC, "Failed to send CWWW state change message: (%d)", err);
            }
        }
        el_ctx->tid++;
    }
    else
    {
        MESHX_LOGE(MOD_LCC, "CWWW Client Element Message: Error (%d)", param->err_code);
        /* Retry sending the failed packet. Do not notify App */
        /* Please note that the failed packets gets sent endlessly. Hence, a loop condition */
        el_ctx->tid++;
        err = meshx_cwww_cli_send_ctl_msg(  pdev,
                                            element_id,
                                            MESHX_LIGHT_CTL_CLI_MSG_GET,
                                            MESHX_LIGHT_CTL_CLI_MSG_ACK);
        if (err)
        {
            MESHX_LOGE(MOD_LCC, "CWWW Client Element Message: Retry failed (%d)", err);
        }
    }
    return err;
}
/**
 * @brief Handles state changes for the CW/WW light client element.
 *
 * This function is called when the state of the CW/WW (Cool White/Warm White) light client element changes.
 * It processes the event and parameters associated with the state change.
 *
 * @param pdev Pointer to the device structure representing the client element.
 * @param evt Event type indicating the nature of the state change.
 * @param params Pointer to additional parameters relevant to the event.
 * @return meshx_err_t Error code indicating success or failure of the handler.
 */

static meshx_err_t meshx_cwww_client_element_state_change_handler(
    const dev_struct_t *pdev,
    control_task_msg_evt_t evt,
    const meshx_ptr_t params
)
{
    if (!pdev || !params)
    {
        return MESHX_INVALID_ARG;
    }
    meshx_err_t err = MESHX_SUCCESS;
    switch (evt)
    {
        case CONTROL_TASK_MSG_EVT_EL_STATE_CH_SET_ON_OFF:
            err = cwww_client_on_off_state_change_handler(
                pdev,
                (const meshx_on_off_cli_el_msg_t *)params);
            break;
        case CONTROL_TASK_MSG_EVT_EL_STATE_CH_SET_CTL:
            err = cwww_light_ctl_state_change_handler(
                pdev,
                (const meshx_ctl_cli_el_msg_t *)params);
            break;
        default:
            err = MESHX_FAIL;
    }

    return err;
}

#if CONFIG_ENABLE_CONFIG_SERVER
/**
 * @brief Callback function for configuration server events.
 *
 * This function handles events from the configuration server, such as model publication
 * and application binding events.
 *
 * @param[in] pdev   Pointer to device struct
 * @param[in] evt    Configuration event type.
 * @param[in] params Pointer to the callback parameter structure.
 */
static meshx_err_t cwww_client_config_srv_cb (
    const dev_struct_t *pdev,
    control_task_msg_evt_t evt,
    const meshx_config_srv_cb_param_t *params)
{
    MESHX_UNUSED(pdev);
    meshx_cwww_client_model_ctx_t *el_ctx = NULL;
    size_t rel_el_id = 0;
    uint16_t element_id = 0;
    uint16_t base_el_id = 0;
    bool nvs_save = false;
    meshx_get_base_element_id(&base_el_id);

    switch (evt)
    {
    case CONTROL_TASK_MSG_EVT_APP_KEY_BIND:
        element_id = params->state_change.mod_app_bind.element_addr - base_el_id;
        if (!IS_EL_IN_RANGE(element_id))
            break;
        rel_el_id = GET_RELATIVE_EL_IDX(element_id);
        el_ctx = CWWW_CLI_EL(rel_el_id).cwww_cli_ctx;
        el_ctx->app_id = params->state_change.appkey_add.app_idx;
        nvs_save = true;
        break;
    case CONTROL_TASK_MSG_EVT_PUB_ADD:
    case CONTROL_TASK_MSG_EVT_PUB_DEL:
        element_id = params->state_change.mod_pub_set.element_addr - base_el_id;
        if (!IS_EL_IN_RANGE(element_id))
            break;
        rel_el_id = GET_RELATIVE_EL_IDX(element_id);
        el_ctx = CWWW_CLI_EL(rel_el_id).cwww_cli_ctx;
        el_ctx->pub_addr = evt == CONTROL_TASK_MSG_EVT_PUB_ADD ? params->state_change.mod_pub_set.pub_addr
                                                           : MESHX_ADDR_UNASSIGNED;
        el_ctx->app_id = params->state_change.mod_pub_set.app_idx;
        nvs_save = true;
        MESHX_LOGI(MOD_LCC, "PUB_ADD: %d, %d, 0x%x, 0x%x", element_id, rel_el_id, el_ctx->pub_addr, el_ctx->app_id);
        break;
    default:
        break;
    }
    if (nvs_save)
    {
        meshx_err_t err = meshx_nvs_element_ctx_set(element_id, el_ctx, sizeof(meshx_cwww_client_model_ctx_t));
        if (err != MESHX_SUCCESS)
        {
            MESHX_LOGE(MOD_LCC, "Failed to set cwww client element context: (%d)", err);
        }
    }
    return MESHX_SUCCESS;
}
#endif /* CONFIG_ENABLE_CONFIG_SERVER */

#if defined(__MESHX_CONTROL_TASK__)

/**
 * @brief CW-WW Client Freshboot Control Task Message Handler
 *
 * This function handles the CW-WW client control task messages.
 *
 * @param[in] pdev   Pointer to the device structure.
 * @param[in] evt    Event type of the control task message.
 * @param[in] params Pointer to the parameters of the control task message.
 * @return meshx_err_t
 */
static meshx_err_t cwww_cli_freshboot_control_task_msg_handle(const dev_struct_t *pdev, control_task_msg_evt_t evt, const void *params)
{
    if(!pdev)
        return MESHX_INVALID_ARG;

    MESHX_UNUSED(params);
    MESHX_UNUSED(evt);
    uint16_t rel_el_id = 0;
    meshx_err_t err = MESHX_SUCCESS;

    for(uint16_t i = cwww_client_element_init_ctrl.element_id_start; i < cwww_client_element_init_ctrl.element_id_end; i++)
    {
        rel_el_id = (uint16_t) GET_RELATIVE_EL_IDX(i);
        for(cwww_cli_sig_id_t model_id = CWWW_CLI_SIG_ONOFF_MODEL_ID; model_id < CWWW_CLI_SIG_ID_MAX; model_id++)
        {
            if(false == (CWWW_CLI_EL(rel_el_id).element_model_init
                        & MESHX_BIT(CWWW_CLI_SIG_ONOFF_MODEL_ID)))
            {
                err = meshx_cwww_el_get_state(i, model_id);
            }
        }
    }
    return err;
}

/**
 * @brief Sends a cwww message over BLE mesh.
 *
 * This function sends a cwww message to a specified element in the BLE mesh network.
 *
 * @param[in] pdev          Pointer to the device structure.
 * @param[in] element_id    The ID of the element to which the message is sent.
 * @param[in] set_get       Indicates whether the message is a set (0) or get (1) operation.
 * @param[in] ack           Indicates whether an acknowledgment is required (1) or not (0).
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_INVALID_ARG: Invalid argument
 *     - MESHX_FAIL: Sending message failed
 */
static meshx_err_t meshx_cwww_cli_send_onoff_msg(
    const dev_struct_t *pdev,
    uint16_t element_id,
    uint8_t set_get,
    uint8_t ack)
{
    if (!pdev || !IS_EL_IN_RANGE(element_id))
        return MESHX_INVALID_ARG;

    meshx_err_t err = MESHX_SUCCESS;
    size_t rel_el_id = GET_RELATIVE_EL_IDX(element_id);
    meshx_onoff_client_model_t *model = CWWW_CLI_EL(rel_el_id).onoff_cli_model;

    meshx_cwww_client_model_ctx_t *el_ctx = CWWW_CLI_EL(rel_el_id).cwww_cli_ctx;
    uint16_t opcode = MESHX_MODEL_OP_GEN_ONOFF_GET;

    if (MESHX_GEN_ON_OFF_CLI_MSG_SET == set_get)
    {
        opcode = ack ? MESHX_MODEL_OP_GEN_ONOFF_SET : MESHX_MODEL_OP_GEN_ONOFF_SET_UNACK;
    }

    MESHX_LOGD(MOD_LCC, "OPCODE: %p", (void *)(uint32_t)opcode);

    /* Send message to the cwww client */
    err = meshx_onoff_client_send_msg(
            model,
            opcode,
            el_ctx->pub_addr,
            pdev->meshx_store.net_key_id,
            el_ctx->app_id,
            el_ctx->state.on_off,
            el_ctx->tid
    );

    if (err)
    {
        MESHX_LOGE(MOD_LCC, "Cwww Client Send Message failed: (%d)", err);
    }
    else
    {
        el_ctx->tid++;
        if(opcode == MESHX_MODEL_OP_GEN_ONOFF_SET_UNACK)
        {
            el_ctx->prev_state.on_off = el_ctx->state.on_off;
            el_ctx->state.on_off = !el_ctx->state.on_off;
        }
    }
    return err;
}

/**
 * @brief Sends a CTL (Color Temperature and White/Warm) control message from the client.
 *
 * This function constructs and sends a CTL message to control the color temperature and white/warm settings
 * of a lighting device element. It can be used for both set and get operations, and supports acknowledgment.
 *
 * @param pdev        Pointer to the device structure containing device-specific information.
 * @param element_id  Identifier for the target element within the device.
 * @param set_get     Operation type: 0 for 'get', 1 for 'set'.
 * @param ack         Acknowledgment flag: 0 for no acknowledgment, 1 to request acknowledgment.
 *
 * @return meshx_err_t Returns an error code indicating the result of the operation.
 */
static meshx_err_t meshx_cwww_cli_send_ctl_msg(
    const dev_struct_t *pdev,
    uint16_t element_id,
    uint8_t set_get,
    uint8_t ack)
{
    if (!pdev || !IS_EL_IN_RANGE(element_id))
        return MESHX_INVALID_ARG;

    meshx_err_t err = MESHX_SUCCESS;
    size_t rel_el_id = GET_RELATIVE_EL_IDX(element_id);
    meshx_cwww_client_model_ctx_t *el_ctx = CWWW_CLI_EL(rel_el_id).cwww_cli_ctx;
    meshx_light_ctl_client_model_t *model = CWWW_CLI_EL(rel_el_id).ctl_cli_model;

    uint16_t opcode = MESHX_MODEL_OP_LIGHT_CTL_GET;

    if( MESHX_LIGHT_CTL_CLI_MSG_SET == set_get)
        opcode = ack ? MESHX_MODEL_OP_LIGHT_CTL_SET : MESHX_MODEL_OP_LIGHT_CTL_SET_UNACK;

    MESHX_LOGD(MOD_LCC, "OPCODE: %p", (void *)(uint32_t)opcode);

    /* Send message to the cwww client */
    err = meshx_light_ctl_client_send_msg(
            model,
            opcode,
            el_ctx->pub_addr,
            pdev->meshx_store.net_key_id,
            el_ctx->app_id,
            el_ctx->ctl_state.lightness,
            el_ctx->ctl_state.temperature,
            el_ctx->ctl_state.delta_uv,
            el_ctx->tid
    );
    if (err)
    {
        MESHX_LOGE(MOD_LCC, "Cwww Client Send Message failed: (%d)", err);
    }
    else
    {
        el_ctx->tid++;
        if(opcode == MESHX_MODEL_OP_LIGHT_CTL_SET_UNACK)
        {
            el_ctx->prev_ctl_state.delta_uv = el_ctx->ctl_state.delta_uv;
            el_ctx->prev_ctl_state.lightness = el_ctx->ctl_state.lightness;
            el_ctx->prev_ctl_state.temperature = el_ctx->ctl_state.temperature;
        }
    }
    return err;
}

/**
 * @brief CWWW Client Control Task Message Handler
 *
 * This function handles the cwww client control task messages.
 *
 * @param[in] pdev   Pointer to the device structure.
 * @param[in] evt    Event type of the control task message.
 * @param[in] params Pointer to the parameters of the control task message.
 * @return meshx_err_t
 */
static meshx_err_t meshx_cwww_client_element_to_ble_handler(
    const dev_struct_t *pdev,
    control_task_msg_evt_t evt,
    const void *params)
{
    if (!pdev || !params)
        return MESHX_INVALID_ARG;
    MESHX_UNUSED(pdev);
    meshx_err_t err = MESHX_SUCCESS;
    const meshx_cwww_client_msg_t *msg = (const meshx_cwww_client_msg_t *)params;
    MESHX_LOGD(MOD_LCC, "EVT: %p, el_id: %d, set_get: %d, ack: %d",
               (void *)evt, msg->element_id, msg->set_get, msg->ack);

    if (!IS_EL_IN_RANGE(msg->element_id))
    {
        return MESHX_INVALID_ARG;
    }
    if (CWWW_CLI_EL(GET_RELATIVE_EL_IDX(msg->element_id)).cwww_cli_ctx->pub_addr == MESHX_ADDR_UNASSIGNED)
    {
        MESHX_LOGW(MOD_LCC, "No publish address set for element: %d", msg->element_id);
        return MESHX_INVALID_STATE;
    }
    if(evt == CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF)
    {
        err = meshx_cwww_cli_send_onoff_msg(
                pdev,
                msg->element_id,
                msg->set_get,
                msg->ack
            );
        if (err != MESHX_SUCCESS)
        {
            MESHX_LOGE(MOD_LCC, "CWWW Client Control Task: Set OnOff failed (%p)", (void *)err);
            return err;
        }
    }
    else if(evt == CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL)
    {
        size_t rel_el_id = msg->element_id - cwww_client_element_init_ctrl.element_id_start;
        meshx_cwww_client_model_ctx_t *el_ctx = CWWW_CLI_EL(rel_el_id).cwww_cli_ctx;
        /* Set new context value as per msg sent */
        el_ctx->ctl_state.delta_uv = (msg->arg_bmap & CWWW_ARG_BMAP_DELTA_UV_SET) ? msg->delta_uv : el_ctx->ctl_state.delta_uv;
        el_ctx->ctl_state.lightness = (msg->arg_bmap & CWWW_ARG_BMAP_LIGHTNESS_SET) ? msg->lightness : el_ctx->ctl_state.lightness;
        el_ctx->ctl_state.temperature = (msg->arg_bmap & CWWW_ARG_BMAP_TEMPERATURE_SET) ? msg->temperature : el_ctx->ctl_state.temperature;
        el_ctx->ctl_state.temp_range_max = (msg->arg_bmap & CWWW_ARG_BMAP_TEMPERATURE_RANGE_SET_MAX) ? msg->temp_range_max : el_ctx->ctl_state.temp_range_max;
        el_ctx->ctl_state.temp_range_min = (msg->arg_bmap & CWWW_ARG_BMAP_TEMPERATURE_RANGE_SET_MIN) ? msg->temp_range_min : el_ctx->ctl_state.temp_range_min;
        err = meshx_cwww_cli_send_ctl_msg(
                pdev,
                msg->element_id,
                msg->set_get,
                msg->ack
            );
        if (err != MESHX_SUCCESS)
        {
            MESHX_LOGE(MOD_LCC, "CWWW Client Control Task: Set CTL failed (%p)", (void *)err);
            return err;
        }
    }
    return err;
}
#endif /* __MESHX_CONTROL_TASK__ */

#if CONFIG_ENABLE_UNIT_TEST

/**
 * @brief CW-WW Client Unit Test Command IDs.
 */
typedef enum cwww_cli_cmd
{
    /* ONOFF UT COMMANDS */
    CWWW_CLI_UT_CMD_ONOFF_GET = 0,
    CWWW_CLI_UT_CMD_ONOFF_SET,
    CWWW_CLI_UT_CMD_ONOFF_SET_UNACK,
    /* CTL UT COMMANDS */
    CWWW_CLI_UT_CMD_CTL_GET,
    CWWW_CLI_UT_CMD_CTL_SET,
    CWWW_CLI_UT_CMD_CTL_SET_UNACK,
    CWWW_CLI_UT_CMD_LIGHTNESS_SET,
    CWWW_CLI_UT_CMD_LIGHTNESS_SET_UNACK,
    CWWW_CLI_UT_CMD_TEMPERATURE_SET,
    CWWW_CLI_UT_CMD_TEMPERATURE_SET_UNACK,
    CWWW_CLI_UT_CMD_DELTA_UV_SET,
    CWWW_CLI_UT_CMD_DELTA_UV_SET_UNACK,
    CWWW_CLI_UT_CMD_TEMPERATURE_RANGE_SET,
    CWWW_CLI_UT_CMD_TEMPERATURE_RANGE_SET_UNACK,
    CWWW_CLI_MAX_CMD
} cwww_cli_cmd_t;

/**
 * @brief Callback handler for the CW-WW client unit test command.
 *
 * This function handles the CW-WW client unit test command by processing the
 * provided command ID and arguments.
 *
 * @param[in] cmd_id The command ID to be processed.
 * @param[in] argc The number of arguments provided.
 * @param[in] argv The array of arguments.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_INVALID_ARG: Invalid arguments
 *     - Other error codes depending on the implementation
 */
static meshx_err_t cwww_cli_unit_test_cb_handler(int cmd_id, int argc, char **argv)
{
    meshx_err_t err = MESHX_SUCCESS;
    meshx_cwww_client_msg_t msg = {0};
    msg.element_id = UT_GET_ARG(0, uint16_t, argv);
    cwww_cli_cmd_t cmd = (cwww_cli_cmd_t)cmd_id;

    MESHX_LOGD(MOD_LCC, "argc|cmd_id: %d|%d", argc, cmd_id);
    if (argc < 1 || cmd_id >= CWWW_CLI_MAX_CMD)
    {
        MESHX_LOGE(MOD_LCC, "CWW Client Unit Test: Invalid number of arguments");
        return MESHX_INVALID_ARG;
    }

    control_task_msg_evt_to_ble_t msg_evt = CONTROL_TASK_MSG_EVT_TO_BLE_MAX;
    switch (cmd)
    {
    case CWWW_CLI_UT_CMD_ONOFF_GET:
        /* ut 1 0 1 <el_id> */
        msg.ack = CWWW_CLI_MSG_ACK;
        msg.set_get = CWWW_CLI_MSG_GET;
        msg_evt = CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF;
        break;
    case CWWW_CLI_UT_CMD_ONOFF_SET:
        /* ut 1 1 1 <el_id> */
    case CWWW_CLI_UT_CMD_ONOFF_SET_UNACK:
        /* ut 1 2 1 <el_id> */
        msg.set_get = CWWW_CLI_MSG_SET;
        msg.arg_bmap = CWWW_ARG_BMAP_ONOFF_SET;
        msg_evt = CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF;
        msg.ack = cmd == CWWW_CLI_UT_CMD_ONOFF_SET ? CWWW_CLI_MSG_ACK : CWWW_CLI_MSG_NO_ACK;
        break;
    case CWWW_CLI_UT_CMD_CTL_GET:
        /* ut 1 3 1 <el_id> */
        msg.ack = CWWW_CLI_MSG_NO_ACK;
        msg.set_get = CWWW_CLI_MSG_GET;
        msg_evt = CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL;
        break;
    case CWWW_CLI_UT_CMD_CTL_SET:
        /* ut 1 4 4 <el_id> <temp> <light> <delta_uv> */
    case CWWW_CLI_UT_CMD_CTL_SET_UNACK:
        /* ut 1 5 4 <el_id> <temp> <light> <delta_uv> */
        if (argc >= 2)
            msg.temperature = UT_GET_ARG(1, uint16_t, argv);
        if (argc >= 3)
            msg.lightness = UT_GET_ARG(2, uint16_t, argv);
        if (argc >= 4)
            msg.delta_uv = UT_GET_ARG(3, uint16_t, argv);
        msg.set_get = CWWW_CLI_MSG_SET;
        msg.arg_bmap = CWWW_ARG_BMAP_CTL_SET;
        msg_evt = CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL;
        msg.ack = cmd == CWWW_CLI_UT_CMD_CTL_SET ? CWWW_CLI_MSG_ACK : CWWW_CLI_MSG_NO_ACK;
        break;
    case CWWW_CLI_UT_CMD_LIGHTNESS_SET:
        /* ut 1 6 2 <el_id> <light> */
    case CWWW_CLI_UT_CMD_LIGHTNESS_SET_UNACK:
        /* ut 1 7 2 <el_id> <light> */
        if (argc >= 2)
            msg.lightness = UT_GET_ARG(1, uint16_t, argv);
        msg.set_get = CWWW_CLI_MSG_SET;
        msg.arg_bmap = CWWW_ARG_BMAP_LIGHTNESS_SET;
        msg_evt = CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL;
        msg.ack = cmd == CWWW_CLI_UT_CMD_LIGHTNESS_SET ? CWWW_CLI_MSG_ACK : CWWW_CLI_MSG_NO_ACK;
        break;
    case CWWW_CLI_UT_CMD_TEMPERATURE_SET:
        /* ut 1 8 2 <el_id> <temp> */
    case CWWW_CLI_UT_CMD_TEMPERATURE_SET_UNACK:
        /* ut 1 9 2 <el_id> <temp> */
        if (argc >= 2)
            msg.temperature = UT_GET_ARG(1, uint16_t, argv);
        msg.set_get = CWWW_CLI_MSG_SET;
        msg.arg_bmap = CWWW_ARG_BMAP_TEMPERATURE_SET;
        msg_evt = CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL;
        msg.ack = cmd == CWWW_CLI_UT_CMD_TEMPERATURE_SET ? CWWW_CLI_MSG_ACK : CWWW_CLI_MSG_NO_ACK;
        break;
    case CWWW_CLI_UT_CMD_DELTA_UV_SET:
        /* ut 1 10 2 <el_id> <delta_uv> */
    case CWWW_CLI_UT_CMD_DELTA_UV_SET_UNACK:
        /* ut 1 11 2 <el_id> <delta_uv> */
        if (argc >= 2)
            msg.delta_uv = UT_GET_ARG(1, uint16_t, argv);
        msg.set_get = CWWW_CLI_MSG_SET;
        msg.arg_bmap = CWWW_ARG_BMAP_DELTA_UV_SET;
        msg_evt = CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL;
        msg.ack = cmd == CWWW_CLI_UT_CMD_DELTA_UV_SET ? CWWW_CLI_MSG_ACK : CWWW_CLI_MSG_NO_ACK;
        break;
    case CWWW_CLI_UT_CMD_TEMPERATURE_RANGE_SET:
        /* ut 1 12 3 <el_id> <min> <max> */
    case CWWW_CLI_UT_CMD_TEMPERATURE_RANGE_SET_UNACK:
        /* ut 1 13 3 <el_id> <min> <max> */
        if (argc >= 2)
            msg.temp_range_min = UT_GET_ARG(1, uint16_t, argv);
        if (argc >= 3)
            msg.temp_range_max = UT_GET_ARG(2, uint16_t, argv);
        msg.set_get = CWWW_CLI_MSG_SET;
        msg.arg_bmap = CWWW_ARG_BMAP_TEMPERATURE_RANGE_SET;
        msg_evt = CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL;
        msg.ack = cmd == CWWW_CLI_UT_CMD_TEMPERATURE_RANGE_SET ? CWWW_CLI_MSG_ACK : CWWW_CLI_MSG_NO_ACK;
        break;
    default:
        MESHX_LOGE(MOD_LCC, "CWWW Client Unit Test: Invalid command");
        return MESHX_INVALID_ARG;
    }

    err = control_task_msg_publish(CONTROL_TASK_MSG_CODE_TO_BLE, msg_evt, &msg, sizeof(msg));
    if (err)
    {
        MESHX_LOGE(MOD_LCC, "CWWW Client Unit Test: Command %d failed", cmd);
    }
    return err;
}

#endif /* CONFIG_ENABLE_UNIT_TEST */

#endif /* CWWW_CLI_MESHX_ONOFF_ENABLE_CB */

/**
 * @brief Initializes the CW-WW client model.
 *
 * This function initializes the CW-WW client model by allocating memory for the
 * CW-WW client context, client list, publish list, and CW-WW client model list.
 *
 * @param[in] n_max Maximum number of elements that can be created in the model space.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_NO_MEM: Memory allocation failure
 *     - MESHX_INVALID_ARG: Invalid arguments
 */
static meshx_err_t meshx_element_struct_init(uint16_t n_max)
{
    if(!n_max)
        return MESHX_INVALID_ARG;

    meshx_err_t err = MESHX_SUCCESS;

    cwww_client_element_init_ctrl.element_cnt = n_max;
    cwww_client_element_init_ctrl.element_id_end = 0;
    cwww_client_element_init_ctrl.element_id_start = 0;

    cwww_client_element_init_ctrl.el_list =
        (meshx_cwww_client_elements_t *)MESHX_CALOC(cwww_client_element_init_ctrl.element_cnt, sizeof(meshx_cwww_client_elements_t));
    if (!cwww_client_element_init_ctrl.el_list)
    {
        MESHX_LOGE(MOD_LCC, "Failed to allocate memory for cwww client elements");
        return MESHX_NO_MEM;
    }

    for (size_t i = 0; i < cwww_client_element_init_ctrl.element_cnt; i++)
    {
        CWWW_CLI_EL(i).cwww_cli_ctx = (meshx_cwww_client_model_ctx_t *)MESHX_CALOC(n_max, sizeof(meshx_cwww_client_model_ctx_t));
        if (!CWWW_CLI_EL(i).cwww_cli_ctx)
        {
            MESHX_LOGE(MOD_LCC, "Failed to allocate memory for cwww client context");
            return MESHX_NO_MEM;
        }
        err = meshx_on_off_client_create(&CWWW_CLI_EL(i).onoff_cli_model,
                                         &CWWW_CLI_EL(i).cwww_cli_sig_model_list[CWWW_CLI_SIG_ONOFF_MODEL_ID]);
        if (err)
        {
            MESHX_LOGE(MOD_LCC, "Meshx On Off Client create failed (Err : 0x%x)", err);
            return err;
        }
        CWWW_CLI_EL(i).onoff_cli_model->meshx_onoff_client_sig_model
            = &CWWW_CLI_EL(i).cwww_cli_sig_model_list[CWWW_CLI_SIG_ONOFF_MODEL_ID];

        err = meshx_light_ctl_client_create(&CWWW_CLI_EL(i).ctl_cli_model,
                                            &CWWW_CLI_EL(i).cwww_cli_sig_model_list[CWWW_CLI_SIG_L_CTL_MODEL_ID]);
        if (err)
        {
            MESHX_LOGE(MOD_LCC, "Meshx CTL Client create failed (Err : 0x%x)", err);
            return err;
        }
        CWWW_CLI_EL(i).ctl_cli_model->meshx_light_ctl_client_sig_model
            = &CWWW_CLI_EL(i).cwww_cli_sig_model_list[CWWW_CLI_SIG_L_CTL_MODEL_ID];
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Deinitializes the CW-WW client model.
 *
 * This function deinitializes the CW-WW client model by freeing the memory allocated
 * for the CW-WW client context, client list, publish list, and CW-WW client model list.
 *
 * @param[in] n_max Maximum number of elements that can be created in the model space.
 */
static meshx_err_t meshx_element_struct_deinit(uint16_t n_max)
{
    if (!cwww_client_element_init_ctrl.element_cnt || !cwww_client_element_init_ctrl.el_list)
    {
        MESHX_LOGE(MOD_LCC, "CWWW element list not initialized");
        return MESHX_INVALID_STATE;
    }

    if (cwww_client_element_init_ctrl.el_list)
    {
        for (size_t i = 0; i < n_max; i++)
        {
            if (CWWW_CLI_EL(i).cwww_cli_ctx)
            {
                MESHX_FREE(CWWW_CLI_EL(i).cwww_cli_ctx);
                CWWW_CLI_EL(i).cwww_cli_ctx = NULL;
            }
        }
        MESHX_FREE(cwww_client_element_init_ctrl.el_list);
        cwww_client_element_init_ctrl.el_list = NULL;
    }
    cwww_client_element_init_ctrl.element_cnt = 0;
    cwww_client_element_init_ctrl.element_id_end = 0;
    cwww_client_element_init_ctrl.element_id_start = 0;

    return MESHX_SUCCESS;
}

/**
 * @brief Creates a CW-WW model space for the given device.
 *
 * This function initializes and allocates resources for a CW-WW (Cool White - Warm White)
 * model space for the specified device. It sets up the necessary structures and configurations
 * to manage the CW-WW model.
 *
 * @param[in] pdev Pointer to the device structure.
 * @param[in] n_max Maximum number of elements that can be created in the model space.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_NO_MEM: Memory allocation failure
 *     - MESHX_INVALID_ARG: Invalid arguments
 */
static meshx_err_t meshx_dev_create_cwww_model_space(dev_struct_t const *pdev, uint16_t n_max)
{
    if (!pdev)
        return MESHX_INVALID_STATE;

    meshx_err_t err = meshx_element_struct_init(n_max);
    if (err)
    {
        MESHX_LOGE(MOD_LCC, "Failed to initialize cwww element structures: (%d)", err);
        meshx_element_struct_deinit(n_max);
        return err;
    }
    return MESHX_SUCCESS;
}

/**
 * @brief
 * This function adds the CW-WW client models to the element list of the specified device.
 * It initializes the necessary structures and configurations for each model.
 *
 * @param[in] pdev      Pointer to the device structure.
 * @param[in] start_idx Pointer to the element index.
 * @param[in] n_max     Maximum number of elements that can be created in the model space.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_NO_MEM: Memory allocation failure
 *     - MESHX_INVALID_ARG: Invalid arguments
 */
static meshx_err_t meshx_add_cwww_cli_model_to_element_list(dev_struct_t *pdev, uint16_t *start_idx, uint16_t n_max)
{
    if (!pdev || !start_idx || !n_max)
        return MESHX_INVALID_ARG;

    if ((n_max + *start_idx) > CONFIG_MAX_ELEMENT_COUNT)
    {
        MESHX_LOGE(MOD_LCC, "No of elements limit reached");
        return MESHX_NO_MEM;
    }
    meshx_err_t err = MESHX_SUCCESS;

    cwww_client_element_init_ctrl.element_id_start = *start_idx;

    for (uint16_t i = *start_idx; i < (n_max + *start_idx) && (i - *start_idx) < CWWW_CLI_MODEL_SIG_CNT; i++)
    {
        if (i == 0)
            continue;
        err = meshx_plat_add_element_to_composition(
            i,
            pdev->elements,
            CWWW_CLI_EL(i - *start_idx).cwww_cli_sig_model_list,
            NULL,
            CWWW_CLI_MODEL_SIG_CNT,
            CWWW_CLI_MODEL_VEN_CNT);
        if (err)
        {
            MESHX_LOGE(MOD_LCC, "Failed to add element to composition: (%d)", err);
            return err;
        }
        err = meshx_nvs_element_ctx_get(i,
                                        CWWW_CLI_EL(i - *start_idx).cwww_cli_ctx,
                                        sizeof(meshx_cwww_client_model_ctx_t));
        if (err != MESHX_SUCCESS)
        {
            MESHX_LOGW(MOD_LCC, "Failed to get cwww cli element context: (0x%x)", err);
        }
    }
    /* Increment the index for further registrations */
    cwww_client_element_init_ctrl.element_id_end = *start_idx += n_max;
    return MESHX_SUCCESS;
}

/**
 * @brief Registers a callback handler for fresh boot events.
 *
 * This function subscribes the provided callback to control task messages
 * related to element state changes. It ensures the callback is valid before subscribing.
 *
 * @param[in] callback  The callback function to handle element state change messages.
 *
 * @return
 *     - MESHX_INVALID_ARG if the callback is NULL.
 *     - Result of control_task_msg_subscribe() otherwise.
 */
static meshx_err_t meshx_cwww_cli_reg_freshboot_cb(control_task_msg_handle_t callback)
{
    if (callback == NULL)
    {
        return MESHX_INVALID_ARG;
    }

    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_SYSTEM,
        CONTROL_TASK_MSG_EVT_SYSTEM_FRESH_BOOT,
        callback
    );
}

/**
 * @brief Registers a callback handler for CW-WW application requests.
 *
 * This function subscribes the provided callback to control task messages
 * related to BLE events. It ensures the callback is valid before subscribing.
 *
 */
static meshx_err_t meshx_cwww_cli_reg_app_req_cb()
{
    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_TO_BLE,
        CONTROL_TASK_MSG_CODE_EVT_MASK,
        (control_task_msg_handle_t)&meshx_cwww_client_element_to_ble_handler
    );
}

static meshx_err_t meshx_cwww_cli_el_state_change_reg_cb()
{
    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_EL_STATE_CH,
        CWWW_CLI_EL_STATE_CH_EVT_MASK,
        (control_task_msg_handle_t)&meshx_cwww_client_element_state_change_handler
    );
}

/**
 * @brief Retrieves the current state of the CW/WW (Cool White/Warm White) light element for the specified element ID.
 *
 * This function queries the state of a light element identified by the given element_id.
 *
 * @param[in] element_id The unique identifier of the light element whose state is to be retrieved.
 * @param[in] model_id The model ID to specify which model's state to retrieve. If set to CWWW_CLI_SIG_ID_MAX, it retrieves the state for all models.
 * @return meshx_err_t Returns MESHX_OK on success, or an appropriate error code on failure.
 */
meshx_err_t meshx_cwww_el_get_state(uint16_t element_id, cwww_cli_sig_id_t model_id)
{
    if (!IS_EL_IN_RANGE(element_id))
    {
        MESHX_LOGE(MOD_LCC, "Invalid element id: %d", element_id);
        return MESHX_INVALID_ARG;
    }
    meshx_cwww_client_msg_t msg = {0};
    msg.ack = CWWW_CLI_MSG_ACK;
    msg.set_get = CWWW_CLI_MSG_GET;
    msg.element_id = element_id;

    if (model_id != CWWW_CLI_SIG_ID_MAX)
    {
        MESHX_LOGD(MOD_LCC, "Sending GET for model: %d", model_id);
        if(model_id == CWWW_CLI_SIG_L_CTL_MODEL_ID)
        {
            control_task_msg_publish(
                CONTROL_TASK_MSG_CODE_TO_BLE,
                CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL,
                &msg,
                sizeof(msg));
        }
        else
        {
            control_task_msg_publish(
                CONTROL_TASK_MSG_CODE_TO_BLE,
                CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF,
                &msg,
                sizeof(msg));
        }
    }
    else
    {
        for(model_id = CWWW_CLI_SIG_ONOFF_MODEL_ID; model_id < CWWW_CLI_SIG_ID_MAX; model_id++)
        {

            MESHX_LOGD(MOD_LCC, "Sending GET for model: %d", model_id);
            if(model_id == CWWW_CLI_SIG_L_CTL_MODEL_ID)
            {
                control_task_msg_publish(
                    CONTROL_TASK_MSG_CODE_TO_BLE,
                    CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL,
                    &msg,
                    sizeof(msg));
            }
            else
            {
                control_task_msg_publish(
                    CONTROL_TASK_MSG_CODE_TO_BLE,
                    CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF,
                    &msg,
                    sizeof(msg));
            }
        }
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Create Dynamic CW-WW Model Elements
 *
 * @param[in] pdev          Pointer to device structure
 * @param[in] element_cnt   Maximum number of CW-WW models
 *
 * @return meshx_err_t
 */
meshx_err_t create_cwww_client_elements(dev_struct_t *pdev, uint16_t element_cnt)
{
    meshx_err_t err;

    err = meshx_dev_create_cwww_model_space(pdev, element_cnt);
    if (err)
    {
        MESHX_LOGE(MOD_LCC, "CWWW Model space create failed: (%d)", err);
        return err;
    }

    err = meshx_add_cwww_cli_model_to_element_list(pdev, (uint16_t *)&pdev->element_idx, element_cnt);
    if (err)
    {
        MESHX_LOGE(MOD_LCC, "CWWW Model add to element create failed: (%d)", err);
        return err;
    }

    err = meshx_on_off_client_init();
    if (err)
    {
        MESHX_LOGE(MOD_LCC, "meshx_onoff_client_init failed: (%d)", err);
        return err;
    }

    err = meshx_light_ctl_client_init();
    if (err)
    {
        MESHX_LOGE(MOD_LCC, "meshx_light_ctl_client_init failed: (%d)", err);
        return err;
    }

#if CONFIG_ENABLE_CONFIG_SERVER
    err = meshx_config_server_cb_reg(
        (config_srv_cb_t) &cwww_client_config_srv_cb,
        CONFIG_SERVER_CB_MASK);
    if (err)
    {
        MESHX_LOGE(MOD_LCC, "Light CWWW config server callback reg failed: (%d)", err);
        return err;
    }
#endif /* CONFIG_ENABLE_CONFIG_SERVER */

#if defined(__MESHX_CONTROL_TASK__)
    /* Register control task callback */
    err = meshx_cwww_cli_reg_app_req_cb();
    if (err)
    {
        MESHX_LOGE(MOD_LCC, "control task callback reg failed: (%d)", err);
        return err;
    }
    /* Register element state change callback */
    err = meshx_cwww_cli_el_state_change_reg_cb();
    if (err)
    {
        MESHX_LOGE(MOD_LCC, "element state change callback reg failed: (%d)", err);
        return err;
    }
    /* Register freshboot control task callback */
    err = meshx_cwww_cli_reg_freshboot_cb(
        (control_task_msg_handle_t)&cwww_cli_freshboot_control_task_msg_handle);
    if (err)
    {
        MESHX_LOGE(MOD_LCC, "control task callback reg failed: (%d)", err);
        return err;
    }

#endif /* __MESHX_CONTROL_TASK__ */
#if CONFIG_ENABLE_UNIT_TEST
    err = register_unit_test(MODULE_ID_ELEMENT_LIGHT_CWWWW_CLIENT, &cwww_cli_unit_test_cb_handler);
    if (err)
    {
        MESHX_LOGE(MOD_LCC, "unit_test reg failed: (%d)", err);
        return err;
    }
#endif /* CONFIG_ENABLE_UNIT_TEST */

    return MESHX_SUCCESS;
}

REG_MESHX_ELEMENT_FN(cwww_cli_el, MESHX_ELEMENT_TYPE_LIGHT_CWWW_CLIENT, create_cwww_client_elements);

#endif /* CONFIG_LIGHT_CWWW_CLIENT_COUNT */
