/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_cwww_server.c
 * @brief Implementation of the CW-WW server model for BLE Mesh.
 *
 * This file contains the implementation of the CW-WW server model for BLE Mesh,
 * including initialization, configuration, and event handling.
 *
 * @author Pranjal Chanda
 */

#include <meshx_cwww_server_element.h>
#include <meshx_nvs.h>
#include <meshx_api.h>

#if CONFIG_LIGHT_CWWW_SRV_COUNT > 0

#if CONFIG_ENABLE_CONFIG_SERVER
#include "meshx_config_server.h"

/**
 * @brief Configuration server callback event mask for relay server.
 */
#define CONFIG_SERVER_CB_MASK    \
    CONTROL_TASK_MSG_EVT_PUB_ADD \
    | CONTROL_TASK_MSG_EVT_SUB_ADD | CONTROL_TASK_MSG_EVT_APP_KEY_BIND
#endif /* CONFIG_ENABLE_CONFIG_SERVER */

#define CONTROL_TASK_EVT_MASK                   \
    CONTROL_TASK_MSG_EVT_EL_STATE_CH_SET_ON_OFF \
    | CONTROL_TASK_MSG_EVT_EL_STATE_CH_SET_CTL

#define CONTROL_TASK_MSG_EVT_TO_BLE_GEN_SRV_MASK \
    CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF_SRV   \
    | CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL_SRV

#define GET_RELATIVE_EL_IDX(_element_id) _element_id - cwww_element_init_ctrl.element_id_start
#define IS_EL_IN_RANGE(_element_id) (_element_id >= cwww_element_init_ctrl.element_id_start && _element_id < cwww_element_init_ctrl.element_id_end)

#define CWWW_TEMP_MIN 2700
#define CWWW_TEMP_MAX 6500

static meshx_cwww_elements_ctrl_t cwww_element_init_ctrl;

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
static meshx_err_t cwww_server_config_srv_cb(
    const dev_struct_t *pdev,
    control_task_msg_evt_t evt,
    const meshx_config_srv_cb_param_t *params)
{
    MESHX_UNUSED(pdev);
    meshx_cwww_server_ctx_t *el_ctx = NULL;
    size_t rel_el_id = 0;
    uint16_t element_id = 0;
    bool nvs_save = false;

    MESHX_LOGD(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "EVT: %p", (void *)evt);
    switch (evt)
    {
    case CONTROL_TASK_MSG_EVT_APP_KEY_BIND:
        element_id = params->model.el_id;
        if (!IS_EL_IN_RANGE(element_id))
            break;
        rel_el_id = GET_RELATIVE_EL_IDX(element_id);
        el_ctx = cwww_element_init_ctrl.el_list[rel_el_id].srv_ctx;
        el_ctx->app_id = params->state_change.appkey_add.app_idx;
        nvs_save = true;

        break;
    case CONTROL_TASK_MSG_EVT_PUB_ADD:
    case CONTROL_TASK_MSG_EVT_PUB_DEL:
        element_id = params->model.el_id;
        if (!IS_EL_IN_RANGE(element_id))
            break;
        rel_el_id = GET_RELATIVE_EL_IDX(element_id);
        el_ctx = cwww_element_init_ctrl.el_list[rel_el_id].srv_ctx;
        el_ctx->pub_addr = evt == CONTROL_TASK_MSG_EVT_PUB_ADD ? params->state_change.mod_pub_set.pub_addr
                                                               : MESHX_ADDR_UNASSIGNED;
        el_ctx->app_id = params->state_change.mod_pub_set.app_idx;
        nvs_save = true;
        MESHX_LOGI(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "PUB_ADD: %d, %d, 0x%x, 0x%x", element_id, rel_el_id, el_ctx->pub_addr, el_ctx->app_id);
        break;
    default:
        break;
    }
    if (nvs_save)
    {
        meshx_err_t err = meshx_nvs_element_ctx_set(element_id, el_ctx, sizeof(meshx_cwww_server_ctx_t));
        if (err != MESHX_SUCCESS)
        {
            MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Failed to set cwww server element context: (%d)", err);
        }
    }
    return MESHX_SUCCESS;
}
#endif /* CONFIG_ENABLE_CONFIG_SERVER */

/**
 * @brief Initializes the mesh element structure by freeing allocated memory.
 *
 * This function allocates memory for various components of the mesh element
 * structure, including server context, server signature model list, server
 * publication list, server on/off generic list, server light control list, and
 * light control state. It ensures that all pointers are set to NULL after
 * freeing the memory to avoid dangling pointers.
 *
 * @param[in] n_max The maximum number of elements in the server signature model list
 *              and server publication list.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully deinitialized the mesh element structure.
 */
static meshx_err_t meshx_element_struct_init(uint16_t n_max)
{
    if (!n_max)
        return MESHX_INVALID_ARG;

    if (cwww_element_init_ctrl.el_list)
    {
        MESHX_LOGW(MODULE_ID_MODEL_SERVER, "CWWW element list already initialized");
        return MESHX_INVALID_STATE;
    }
    meshx_err_t err = MESHX_SUCCESS;

    cwww_element_init_ctrl.element_cnt = n_max;
    cwww_element_init_ctrl.element_id_end = 0;
    cwww_element_init_ctrl.element_id_start = 0;

    cwww_element_init_ctrl.el_list =
        (meshx_cwww_element_t *)MESHX_CALOC(cwww_element_init_ctrl.element_cnt, sizeof(meshx_cwww_element_t));

    if (!cwww_element_init_ctrl.el_list)
        return MESHX_NO_MEM;

    for (size_t i = 0; i < cwww_element_init_ctrl.element_cnt; i++)
    {
        cwww_element_init_ctrl.el_list[i].srv_ctx =
            (meshx_cwww_server_ctx_t *)MESHX_CALOC(1, sizeof(meshx_cwww_server_ctx_t));

        if (!cwww_element_init_ctrl.el_list[i].srv_ctx)
            return MESHX_NO_MEM;

        err = meshx_on_off_server_create(&cwww_element_init_ctrl.el_list[i].onoff_srv_model,
                                         &cwww_element_init_ctrl.el_list[i].cwww_srv_model_list[CWWW_SIG_ONOFF_MODEL_ID]);
        if (err)
        {
            MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Meshx On Off Server create failed (Err : 0x%x)", err);
            return err;
        }
        err = meshx_light_ctl_server_create(&cwww_element_init_ctrl.el_list[i].ctl_srv_model,
                                            &cwww_element_init_ctrl.el_list[i].cwww_srv_model_list[CWWW_SIG_L_CTL_MODEL_ID]);
        if (err)
        {
            MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Meshx CTL Server create failed (Err : 0x%x)", err);
            return err;
        }
        cwww_element_init_ctrl.el_list[i].onoff_srv_model->meshx_server_sig_model = &cwww_element_init_ctrl.el_list[i].cwww_srv_model_list[CWWW_SIG_ONOFF_MODEL_ID];
        cwww_element_init_ctrl.el_list[i].ctl_srv_model->meshx_server_sig_model = &cwww_element_init_ctrl.el_list[i].cwww_srv_model_list[CWWW_SIG_L_CTL_MODEL_ID];
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Deinitializes the mesh element structure by freeing allocated memory.
 *
 * This function deallocates memory for various components of the mesh element
 * structure, including server context, server signature model list, server
 * publication list, server on/off generic list, server light control list, and
 * light control state. It ensures that all pointers are set to NULL after
 * freeing the memory to avoid dangling pointers.
 *
 * @param[in] n_max The maximum number of elements in the server signature model list
 *              and server publication list.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully deinitialized the mesh element structure.
 */

static meshx_err_t meshx_element_struct_deinit()
{
    if (!cwww_element_init_ctrl.element_cnt || !cwww_element_init_ctrl.el_list)
        return MESHX_INVALID_STATE;

    meshx_err_t err = MESHX_SUCCESS;

    for (size_t i = 0; i < cwww_element_init_ctrl.element_cnt; i++)
    {
        if (cwww_element_init_ctrl.el_list[i].srv_ctx)
        {
            MESHX_FREE(cwww_element_init_ctrl.el_list[i].srv_ctx);
            cwww_element_init_ctrl.el_list[i].srv_ctx = NULL;
        }

        err = meshx_on_off_server_delete(&cwww_element_init_ctrl.el_list[i].onoff_srv_model);
        if (err)
        {
            MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Meshx On Off Server create failed (Err : 0x%x)", err);
            return err;
        }

        err = meshx_light_ctl_server_delete(&cwww_element_init_ctrl.el_list[i].ctl_srv_model);
        if (err)
        {
            MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Meshx CTL Server create failed (Err : 0x%x)", err);
            return err;
        }
    }

    MESHX_FREE(cwww_element_init_ctrl.el_list);
    cwww_element_init_ctrl.el_list = NULL;

    return MESHX_SUCCESS;
}
/**
 * @brief Create space for CW-WW models.
 *
 * This function allocates and initializes the space required for CW-WW models.
 *
 * @param n_max Maximum number of models.
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
static meshx_err_t meshx_dev_create_cwww_model_space(uint16_t n_max)
{
    meshx_err_t err = meshx_element_struct_init(n_max);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Failed to initialize cwww element structures: (%d)", err);
        meshx_element_struct_deinit();
        return err;
    }
    return err;
}

/**
 * @brief Restore saved CW-WW model states.
 *
 * This function restores the CW-WW model states from the NVS.
 *
 * @param element_id    Relative Element ID.
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
static meshx_err_t meshx_restore_model_states(uint16_t element_id)
{
    uint16_t model_id = 0;
    meshx_err_t err = MESHX_SUCCESS;
    meshx_cwww_server_ctx_t const *el_ctx = cwww_element_init_ctrl.el_list[element_id].srv_ctx;

    for (size_t i = 0; i < CWWW_SRV_MODEL_SIG_CNT; i++)
    {
        err = meshx_get_model_id(cwww_element_init_ctrl.el_list[element_id].onoff_srv_model->meshx_server_sig_model,
                                 &model_id);
        if (err)
        {
            MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to get model ID (err: 0x%x)", err);
            return err;
        }
        if (model_id == MESHX_MODEL_ID_GEN_ONOFF_SRV)
        {
            err = meshx_gen_on_off_srv_state_restore(cwww_element_init_ctrl.el_list[element_id].onoff_srv_model->meshx_server_sig_model,
                                                     el_ctx->prev_state);
            if (err)
            {
                MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to restore on-off server state (err: 0x%x)", err);
                return err;
            }
        }
        else if (model_id == MESHX_MODEL_ID_LIGHT_CTL_SRV)
        {
            err = meshx_light_ctl_srv_state_restore(cwww_element_init_ctrl.el_list[element_id].ctl_srv_model,
                                                    el_ctx->prev_ctl_state);
            if (err)
            {
                MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to restore on-off server state (err: 0x%x)", err);
                return err;
            }
        }
    }
    return err;
}
/**
 * @brief Add CW-WW server models to the element list.
 *
 * This function adds the CW-WW server models to the specified element list.
 *
 * @param pdev Pointer to the device structure.
 * @param start_idx Pointer to the starting index.
 * @param n_max Maximum number of models.
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
static meshx_err_t meshx_add_cwww_srv_model_to_element_list(dev_struct_t *pdev, uint16_t *start_idx, uint16_t n_max)
{
    if (!pdev)
        return MESHX_INVALID_STATE;

    if (!start_idx || (n_max + *start_idx) > CONFIG_MAX_ELEMENT_COUNT)
    {
        MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "No of elements limit reached");
        return MESHX_NO_MEM;
    }
    meshx_err_t err = MESHX_SUCCESS;
    cwww_element_init_ctrl.element_id_start = *start_idx;

    for (uint16_t i = *start_idx; i < (n_max + *start_idx); i++)
    {
        if (i == 0)
            continue;

        err = meshx_plat_add_element_to_composition(
            i,
            pdev->elements,
            cwww_element_init_ctrl.el_list[i - *start_idx].cwww_srv_model_list,
            NULL,
            CWWW_SRV_MODEL_SIG_CNT,
            CWWW_SRV_MODEL_VEN_CNT);
        if (err)
        {
            MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to add element to composition: (%d)", err);
            return err;
        }

        err = meshx_nvs_element_ctx_get(
            i,
            &(cwww_element_init_ctrl.el_list[i - *start_idx].srv_ctx),
            sizeof(cwww_element_init_ctrl.el_list[i - *start_idx].srv_ctx));
        if (err != MESHX_SUCCESS)
        {
            MESHX_LOGW(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Failed to get cwww element context: (0x%x)", err);
        }
        else
        {
            err = meshx_restore_model_states(i - *start_idx);
            if (err != MESHX_SUCCESS)
            {
                MESHX_LOGW(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Failed to restore cwww model states: (0x%x)", err);
            }
        }
    }
    /* Increment the index for further registrations */
    cwww_element_init_ctrl.element_id_end = *start_idx += n_max;
    return MESHX_SUCCESS;
}

/**
 * @brief CW-WW server model event handler.
 *
 * This function handles events from the CW-WW server model.
 *
 * Event Types:
 * - CONTROL_TASK_MSG_EVT_EL_STATE_CH_SET_ON_OFF: Handles on/off state changes.
 *   - params: Pointer to MESHX_GEN_ONOFF_SRV containing the new on/off state.
 *   - Updates the on/off state of the corresponding element context.
 *
 * - CONTROL_TASK_MSG_EVT_EL_STATE_CH_SET_CTL: Handles CTL (Color Temperature Lightness) state changes.
 *   - params: Pointer to MESHX_LIGHT_CTL_SRV containing the new CTL state.
 *   - Updates the delta UV, lightness, temperature, and temperature range of the corresponding element context.
 *
 * If the element ID is out of range, the function exits without making any changes.
 * @param[in] pdev Pointer to the device structure.
 * @param[in] evt Event type.
 * @param[in] params Pointer to the event parameters.
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
static meshx_err_t meshx_api_control_task_handler(const dev_struct_t *pdev, const control_task_msg_evt_t evt, const void *params)
{
    MESHX_UNUSED(pdev);
    meshx_err_t err = MESHX_SUCCESS;
    uint16_t element_id = 0;
    size_t rel_el_id;
    meshx_cwww_server_ctx_t *el_ctx = NULL;
    meshx_api_light_cwww_server_evt_t app_msg;
    cwww_sig_id_t sig_func = CWWW_SIG_ONOFF_MODEL_ID;

    switch (evt)
    {
    case CONTROL_TASK_MSG_EVT_EL_STATE_CH_SET_ON_OFF:
    {
        const meshx_on_off_srv_el_msg_t *p_onoff_srv = (const meshx_on_off_srv_el_msg_t *)params;
        element_id = p_onoff_srv->model.el_id;
        if (!IS_EL_IN_RANGE(element_id))
            goto el_ctrl_task_hndlr_exit;

        rel_el_id = GET_RELATIVE_EL_IDX(element_id);
        el_ctx = cwww_element_init_ctrl.el_list[rel_el_id].srv_ctx;
        if (el_ctx->prev_state.on_off == p_onoff_srv->on_off_state)
            goto el_ctrl_task_hndlr_exit;

        sig_func = CWWW_SIG_ONOFF_MODEL_ID;
        el_ctx->prev_state.on_off = p_onoff_srv->on_off_state;
        app_msg.state_change.on_off.state = el_ctx->prev_state.on_off;
        break;
    }

    case CONTROL_TASK_MSG_EVT_EL_STATE_CH_SET_CTL:
    {
        const meshx_light_ctl_srv_t *p_ctl_srv = (const meshx_light_ctl_srv_t *)params;
        element_id = p_ctl_srv->model.el_id;
        if (!IS_EL_IN_RANGE(element_id))
            goto el_ctrl_task_hndlr_exit;

        rel_el_id = GET_RELATIVE_EL_IDX(element_id);
        el_ctx = cwww_element_init_ctrl.el_list[rel_el_id].srv_ctx;

        if (el_ctx->prev_ctl_state.delta_uv == p_ctl_srv->state.delta_uv &&
            el_ctx->prev_ctl_state.lightness == p_ctl_srv->state.lightness &&
            el_ctx->prev_ctl_state.temperature == p_ctl_srv->state.temperature &&
            el_ctx->prev_ctl_state.temperature_range_min == p_ctl_srv->state.temperature_range_min &&
            el_ctx->prev_ctl_state.temperature_range_max == p_ctl_srv->state.temperature_range_max)
            goto el_ctrl_task_hndlr_exit;

        el_ctx->prev_ctl_state.delta_uv = p_ctl_srv->state.delta_uv;
        el_ctx->prev_ctl_state.lightness = p_ctl_srv->state.lightness;
        el_ctx->prev_ctl_state.temperature = p_ctl_srv->state.temperature;
        el_ctx->prev_ctl_state.temperature_range_min = p_ctl_srv->state.temperature_range_min;
        el_ctx->prev_ctl_state.temperature_range_max = p_ctl_srv->state.temperature_range_max;

        sig_func = CWWW_SIG_L_CTL_MODEL_ID;
        app_msg.state_change.ctl.delta_uv = el_ctx->prev_ctl_state.delta_uv;
        app_msg.state_change.ctl.lightness = el_ctx->prev_ctl_state.lightness;
        app_msg.state_change.ctl.temperature = el_ctx->prev_ctl_state.temperature;
        app_msg.state_change.ctl.temp_range_min = el_ctx->prev_ctl_state.temperature_range_min;
        app_msg.state_change.ctl.temp_range_max = el_ctx->prev_ctl_state.temperature_range_max;
        break;
    }
    default:
        break;
    }

    err = meshx_nvs_element_ctx_set(element_id, el_ctx, sizeof(meshx_cwww_server_ctx_t));
    if (err != MESHX_SUCCESS)
        MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Failed to set relay element context: (%d)", err);

    err = meshx_send_msg_to_app(element_id,
                                MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER,
                                (uint16_t)sig_func,
                                sizeof(meshx_api_light_cwww_server_evt_t),
                                &app_msg);

    if (err != MESHX_SUCCESS)
        MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Failed to send relay state change message: (%d)", err);

el_ctrl_task_hndlr_exit:
    return MESHX_SUCCESS;
}

/**
 * @brief Callback function for relay server model events for Provisioning events.
 *
 * @param[in] pdev      Pointer to the device structure.
 * @param[in] evt       Relay server event type.
 * @param[in] params    Pointer to the parameters for the event.
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
static meshx_err_t cwww_prov_control_task_handler(dev_struct_t const *pdev, control_task_msg_evt_t evt, void const *params)
{
    MESHX_UNUSED(evt);
    MESHX_UNUSED(params);

    size_t rel_el_id = 0;
    meshx_err_t err = MESHX_SUCCESS;
    meshx_gen_srv_cb_param_t gen_srv_send;
    meshx_lighting_server_cb_param_t light_srv_send;

    for (size_t el_id = cwww_element_init_ctrl.element_id_start; el_id < cwww_element_init_ctrl.element_id_end; el_id++)
    {
        rel_el_id = GET_RELATIVE_EL_IDX(el_id);

        gen_srv_send.ctx.net_idx = pdev->meshx_store.net_key_id;
        gen_srv_send.ctx.app_idx = cwww_element_init_ctrl.el_list[rel_el_id].srv_ctx->app_id;
        gen_srv_send.ctx.dst_addr = cwww_element_init_ctrl.el_list[rel_el_id].srv_ctx->pub_addr;
        gen_srv_send.ctx.opcode = MESHX_MODEL_OP_GEN_ONOFF_STATUS;
        gen_srv_send.ctx.p_ctx = NULL;

        gen_srv_send.model.el_id = (uint16_t)el_id;
        gen_srv_send.model.p_model = cwww_element_init_ctrl.el_list[rel_el_id].onoff_srv_model->meshx_server_sig_model;

        if (gen_srv_send.ctx.dst_addr == MESHX_ADDR_UNASSIGNED || gen_srv_send.ctx.app_idx == MESHX_KEY_UNUSED)
            continue;

        err = meshx_gen_srv_send_msg_to_ble(
            CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF_SRV,
            &gen_srv_send
        );

        if (err)
        {
            MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Failed to send ONOFF status message (Err: %x)", err);
            return err;
        }

        light_srv_send.ctx.net_idx = pdev->meshx_store.net_key_id;
        light_srv_send.ctx.app_idx = cwww_element_init_ctrl.el_list[rel_el_id].srv_ctx->app_id;
        light_srv_send.ctx.dst_addr = cwww_element_init_ctrl.el_list[rel_el_id].srv_ctx->pub_addr;
        light_srv_send.ctx.opcode = MESHX_MODEL_OP_LIGHT_CTL_STATUS;
        light_srv_send.ctx.p_ctx = NULL;

        light_srv_send.model.el_id = (uint16_t)el_id;
        light_srv_send.model.p_model = cwww_element_init_ctrl.el_list[rel_el_id].ctl_srv_model->meshx_server_sig_model;

        err = meshx_gen_light_srv_send_msg_to_ble(
            CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL_SRV,
            &light_srv_send
        );
        if (err)
        {
            MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Failed to send CTL status message (Err: %x)", err);
            return err;
        }
    }

    return err;
}

/**
 * @brief Handler for sending messages from the CW-WW server model to BLE.
 *
 * This function handles the sending of messages from the CW-WW server model to BLE.
 * It processes different events and sends the appropriate status messages based on the event type.
 *
 * @param[in] pdev Pointer to the device structure.
 * @param[in] evt Event type to handle.
 * @param[in] params Pointer to the parameters for the event.
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
static meshx_err_t meshx_cwww_srv_msg_send_handler(
                    const dev_struct_t *pdev,
                    control_task_msg_evt_to_ble_t evt,
                    void *params)
{
    MESHX_UNUSED(pdev);
    if((evt & (CONTROL_TASK_MSG_EVT_TO_BLE_GEN_SRV_MASK)) == 0)
        return MESHX_SUCCESS;

    switch (evt)
    {
        case CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF_SRV:
        {
            meshx_gen_srv_cb_param_t *gen_srv_send = (meshx_gen_srv_cb_param_t *)params;
            meshx_err_t err = meshx_gen_on_off_srv_status_send(
                &gen_srv_send->model,
                &gen_srv_send->ctx,
                gen_srv_send->state_change.onoff_set.onoff
            );
            if (err)
            {
                MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Failed to send ONOFF status message (Err: %x)", err);
                return err;
            }
        }
        break;
        case CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL_SRV:
        {
            meshx_lighting_server_cb_param_t *light_srv_send = (meshx_lighting_server_cb_param_t *)params;
            meshx_err_t err = meshx_light_ctl_srv_status_send(
                &light_srv_send->model,
                &light_srv_send->ctx,
                light_srv_send->state_change.ctl_set.delta_uv,
                light_srv_send->state_change.ctl_set.lightness,
                light_srv_send->state_change.ctl_set.temperature
            );
            if (err)
            {
                MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Failed to send CTL status message (Err: %x)", err);
                return err;
            }
        }
        break;
        default:
            MESHX_LOGW(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Unhandled event: %d", evt);
            return MESHX_INVALID_STATE;
        }
    return MESHX_SUCCESS;
}

/**
 * @brief Create Dynamic CWWW Server Model Elements
 *
 * This function creates dynamic CWWW server model elements for the given device structure.
 *
 * @param[in] pdev          Pointer to device structure
 * @param[in] element_cnt   Maximum number of CWWW server models
 *
 * @return meshx_err_t Returns MESHX_SUCCESS on success or an error code on failure
 */
meshx_err_t meshx_create_cwww_elements(dev_struct_t *pdev, uint16_t element_cnt)
{
    meshx_err_t err;
    err = meshx_dev_create_cwww_model_space(element_cnt);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "CWWW Model create failed: (%d)", err);
        return err;
    }
    err = meshx_add_cwww_srv_model_to_element_list(pdev, (uint16_t *)&pdev->element_idx, element_cnt);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "CWWW Model create failed: (%d)", err);
        return err;
    }
#if CONFIG_ENABLE_CONFIG_SERVER
    err = meshx_config_server_cb_reg(
        (config_srv_cb_t)&cwww_server_config_srv_cb, CONFIG_SERVER_CB_MASK);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Relay Model configserver callback reg failed: (%d)", err);
        return err;
    }
#endif /* CONFIG_ENABLE_CONFIG_SERVER */
    err = control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_EL_STATE_CH,
        CONTROL_TASK_EVT_MASK,
        (control_task_msg_handle_t)&meshx_api_control_task_handler);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Failed to register control task callback: (%d)", err);
        return err;
    }

    err = control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_PROVISION,
        CONTROL_TASK_MSG_EVT_EN_NODE_PROV,
        (control_task_msg_handle_t)&cwww_prov_control_task_handler);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Failed to register control task callback: (%d)", err);
        return err;
    }
    err = control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_TO_BLE,
        CONTROL_TASK_MSG_EVT_TO_BLE_GEN_SRV_MASK,
        (control_task_msg_handle_t)&meshx_cwww_srv_msg_send_handler);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Failed to register control task callback: (%d)", err);
        return err;
    }
    err = meshx_on_off_server_init();
    if (err)
    {
        MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "meshx_on_off_server_init failed: (%d)", err);
        return err;
    }
    err = meshx_light_ctl_server_init();
    if (err)
    {
        MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "meshx_light_ctl_server_init failed: (%d)", err);
        return err;
    }
    return MESHX_SUCCESS;
}

REG_MESHX_ELEMENT_FN(cwww_srv_el, MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER, meshx_create_cwww_elements);

#endif /* CONFIG_LIGHT_CWWW_SRV_COUNT > 0*/
