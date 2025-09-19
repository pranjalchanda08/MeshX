/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_relay_server.c
 * @brief Relay server model implementation for BLE Mesh networks.
 *
 * This file contains the implementation of the relay server model for BLE Mesh.
 * It includes functions to initialize, configure, and manage relay server elements.
 *
 * @author Pranjal Chanda
 */

#include "meshx_relay_server_element.h"
#include "meshx_nvs.h"
#include "meshx_api.h"

#if CONFIG_RELAY_SERVER_COUNT > 0

#if CONFIG_ENABLE_CONFIG_SERVER
#include "meshx_config_server.h"

/**
 * @brief Configuration server callback event mask for relay server.
 */
#define CONFIG_SERVER_CB_MASK    \
    CONTROL_TASK_MSG_EVT_PUB_ADD \
    | CONTROL_TASK_MSG_EVT_SUB_ADD | CONTROL_TASK_MSG_EVT_APP_KEY_BIND
#endif /* CONFIG_ENABLE_CONFIG_SERVER */

#define CONTROL_TASK_EVT_MASK CONTROL_TASK_MSG_EVT_EL_STATE_CH_SET_ON_OFF

#define CONTROL_TASK_MSG_EVT_TO_BLE_GEN_SRV_MASK CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF_SRV

/**
 * @brief Get the relative index of an element ID.
 * @param _element_id The element ID.
 * @return The relative index of the element.
 */
#define GET_RELATIVE_EL_IDX(_element_id) ((_element_id) - (relay_element_init_ctrl.element_id_start))

/**
 * @brief Check if an element ID is within range.
 * @param _element_id The element ID.
 * @return True if the element ID is within range, false otherwise.
 */
#define IS_EL_IN_RANGE(_element_id) ((_element_id) >= relay_element_init_ctrl.element_id_start && (_element_id) < relay_element_init_ctrl.element_id_end)
#define RELAY_SRV_EL(_el_id) relay_element_init_ctrl.el_list[_el_id]
/**
 * @brief Structure to manage relay element initialization.
 */
static meshx_relay_element_ctrl_t relay_element_init_ctrl;

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
 *
 * @return MESHX_SUCCESS on success, an error code otherwise.
 */
static meshx_err_t relay_server_config_srv_cb(
    const dev_struct_t *pdev,
    control_task_msg_evt_t evt,
    const meshx_config_srv_cb_param_t *params)
{
    MESHX_UNUSED(pdev);
    meshx_relay_srv_model_ctx_t *el_ctx = NULL;
    size_t rel_el_id = 0;
    uint16_t element_id = 0;
    uint16_t base_el_id = 0;
    bool nvs_save = false;
    meshx_get_base_element_id(&base_el_id);
    MESHX_LOGD(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "EVT: %d", evt);
    switch (evt)
    {
    case CONTROL_TASK_MSG_EVT_APP_KEY_BIND:
        element_id = params->state_change.mod_app_bind.element_addr - base_el_id;
        MESHX_LOGD(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "ele: %d", element_id);
        if (!IS_EL_IN_RANGE(element_id))
            break;
        rel_el_id = GET_RELATIVE_EL_IDX(element_id);
        el_ctx = RELAY_SRV_EL(rel_el_id).srv_ctx;
        el_ctx->app_id = params->state_change.appkey_add.app_idx;
        nvs_save = true;
        break;
    case CONTROL_TASK_MSG_EVT_PUB_ADD:
    case CONTROL_TASK_MSG_EVT_PUB_DEL:
        element_id = params->state_change.mod_pub_set.element_addr - base_el_id;
        MESHX_LOGD(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "ele: %d", element_id);
        if (!IS_EL_IN_RANGE(element_id))
            break;
        rel_el_id = GET_RELATIVE_EL_IDX(element_id);
        el_ctx = RELAY_SRV_EL(rel_el_id).srv_ctx;
        el_ctx->pub_addr = evt == CONTROL_TASK_MSG_EVT_PUB_ADD ? params->state_change.mod_pub_set.pub_addr
                                                               : MESHX_ADDR_UNASSIGNED;
        el_ctx->app_id = params->state_change.mod_pub_set.app_idx;
        MESHX_LOGI(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "PUB_ADD: %d, %d, 0x%X, 0x%X", element_id, rel_el_id, el_ctx->pub_addr, el_ctx->app_id);
        nvs_save = true;
        break;
    default:
        break;
    }
    if (nvs_save)
    {
        meshx_err_t err = meshx_nvs_element_ctx_set(element_id, el_ctx, sizeof(meshx_relay_srv_model_ctx_t));
        if (err != MESHX_SUCCESS)
        {
            MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Failed to set relay server element context: (%d)", err);
        }
    }
    return MESHX_SUCCESS;
}
#endif /* CONFIG_ENABLE_CONFIG_SERVER */

/**
 * @brief Initialize the mesh element structure by allocating memory for various components.
 *
 * This function initializes the mesh element structure by allocating memory for
 * various components, including server context, server signature model list,
 * server publication list, server on/off generic list, and server light control list.
 *
 * @param n_max The maximum number of elements in the server signature model list
 *              and server publication list.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully initialized the mesh element structure.
 *     - MESHX_NO_MEM: Failed to allocate memory for the mesh element structure.
 */
static meshx_err_t meshx_element_struct_init(uint16_t n_max)
{
    if (!n_max)
        return MESHX_INVALID_ARG;

    if (relay_element_init_ctrl.el_list)
    {
        MESHX_LOGW(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Relay element list already initialized");
        return MESHX_INVALID_STATE;
    }

    meshx_err_t err = MESHX_SUCCESS;

    relay_element_init_ctrl.element_cnt = n_max;
    relay_element_init_ctrl.element_id_end = 0;
    relay_element_init_ctrl.element_id_start = 0;

    relay_element_init_ctrl.el_list =
        (meshx_relay_element_t *)MESHX_CALOC(relay_element_init_ctrl.element_cnt, sizeof(meshx_relay_element_t));

    if (!relay_element_init_ctrl.el_list)
        return MESHX_NO_MEM;

    for (size_t i = 0; i < relay_element_init_ctrl.element_cnt; i++)
    {
        RELAY_SRV_EL(i).srv_ctx =
            (meshx_relay_srv_model_ctx_t *)MESHX_CALOC(1, sizeof(meshx_relay_srv_model_ctx_t));

        if (!RELAY_SRV_EL(i).srv_ctx)
            return MESHX_NO_MEM;

        err = meshx_on_off_server_create(&RELAY_SRV_EL(i).onoff_srv_model,
                                         &RELAY_SRV_EL(i).relay_srv_model_list[RELAY_SIG_ONOFF_MODEL_ID]);
        if (err)
        {
            MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Meshx On Off Server create failed (Err : 0x%x)", err);
            return err;
        }
        RELAY_SRV_EL(i).onoff_srv_model->meshx_server_sig_model
            = &RELAY_SRV_EL(i).relay_srv_model_list[RELAY_SIG_ONOFF_MODEL_ID];
    }

    return err;
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
 * @return
 *     - MESHX_SUCCESS: Successfully deinitialized the mesh element structure.
 */
static meshx_err_t meshx_element_struct_deinit(void)
{
    if (!relay_element_init_ctrl.element_cnt || !relay_element_init_ctrl.el_list)
    {
        MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Relay element list not initialized");
        return MESHX_INVALID_STATE;
    }

    meshx_err_t err;

    for (size_t i = 0; i < relay_element_init_ctrl.element_cnt; i++)
    {
        if (RELAY_SRV_EL(i).srv_ctx)
        {
            MESHX_FREE(RELAY_SRV_EL(i).srv_ctx);
            RELAY_SRV_EL(i).srv_ctx = NULL;
        }
        err = meshx_on_off_server_delete(&RELAY_SRV_EL(i).onoff_srv_model);
        if (err)
            MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Meshx On Off Server delete failed (Err : 0x%x)", err);
    }

    if (relay_element_init_ctrl.el_list)
    {
        MESHX_FREE(relay_element_init_ctrl.el_list);
        relay_element_init_ctrl.el_list = NULL;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Create relay model space.
 *
 * Allocates memory and initializes space for relay models.
 *
 * @param n_max Maximum number of relay models.
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
static meshx_err_t meshx_dev_create_relay_model_space(uint16_t n_max)
{
    meshx_err_t err = meshx_element_struct_init(n_max);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Relay Model space create failed: (%d)", err);
        meshx_element_struct_deinit();
        return err;
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Restore saved relay model states.
 *
 * Restores the relay model states from the NVS.
 *
 * @param element_id Relative element ID.
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
static meshx_err_t meshx_restore_model_states(uint16_t element_id)
{
    uint16_t model_id = 0;
    meshx_err_t err = MESHX_SUCCESS;
    meshx_relay_srv_model_ctx_t const *el_ctx = RELAY_SRV_EL(element_id).srv_ctx;
    for (size_t i = 0; i < RELAY_SRV_MODEL_SIG_CNT; i++)
    {
        err = meshx_get_model_id(RELAY_SRV_EL(element_id).onoff_srv_model->meshx_server_sig_model,
                                 &model_id);
        if (err)
        {
            MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Failed to get model ID (err: 0x%x)", err);
            return err;
        }

        if (model_id == MESHX_MODEL_ID_GEN_ONOFF_SRV)
        {
            err = meshx_gen_on_off_srv_state_restore(RELAY_SRV_EL(element_id).onoff_srv_model->meshx_server_sig_model,
                                                     el_ctx->state);
            if (err)
            {
                MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Failed to restore on-off server state (err: 0x%x)", err);
                return err;
            }
        }
    }
    return err;
}
/**
 * @brief Add relay server models to the element list.
 *
 * Registers the relay server models to the BLE Mesh element list.
 *
 * @param pdev Pointer to the device structure.
 * @param start_idx Pointer to the start index of elements.
 * @param n_max Maximum number of elements to add.
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
static meshx_err_t meshx_add_relay_srv_model_to_element_list(dev_struct_t *pdev, uint16_t *start_idx, uint16_t n_max)
{
    if (!pdev)
        return MESHX_INVALID_STATE;

    if ((n_max + *start_idx) > CONFIG_MAX_ELEMENT_COUNT)
    {
        MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "No of elements limit reached");
        return MESHX_NO_MEM;
    }

    meshx_err_t err = MESHX_SUCCESS;
    relay_element_init_ctrl.element_id_start = *start_idx;

    for (uint16_t i = *start_idx; i < (n_max + *start_idx); i++)
    {

        if (i == 0)
            continue;
        err = meshx_plat_add_element_to_composition(
            i,
            pdev->elements,
            RELAY_SRV_EL(i - *start_idx).relay_srv_model_list,
            NULL,
            RELAY_SRV_MODEL_SIG_CNT,
            RELAY_SRV_MODEL_VEN_CNT);
        if (err)
        {
            MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Failed to add element to composition: (%d)", err);
            return err;
        }
        err = meshx_nvs_element_ctx_get(
            i,
            RELAY_SRV_EL(i - *start_idx).srv_ctx,
            sizeof(meshx_relay_srv_model_ctx_t));
        if (err != MESHX_SUCCESS)
        {
            MESHX_LOGW(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Failed to get relay element context: (0x%x)", err);
        }
        else
        {
            err = meshx_restore_model_states(i - *start_idx);
            if (err != MESHX_SUCCESS)
            {
                MESHX_LOGW(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Failed to restore relay model states: (0x%x)", err);
            }
        }
    }
    relay_element_init_ctrl.element_id_end = (*start_idx += n_max);
    return MESHX_SUCCESS;
}

/**
 * @brief Callback function for relay server model events.
 *
 * This function handles events from the relay server model, such as setting the relay state.
 *
 * @param[in] pdev      Pointer to the callback parameter structure.
 * @param[in] evt       Relay server event type.
 * @param[in] params    Pointer to the parameters for the event.
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
static meshx_err_t meshx_api_control_task_handler(dev_struct_t const *pdev, control_task_msg_evt_t evt, void *params)
{
    MESHX_UNUSED(pdev);
    MESHX_UNUSED(evt);
    size_t rel_el_id = 0;
    meshx_err_t err = MESHX_SUCCESS;
    meshx_relay_srv_model_ctx_t *el_ctx = NULL;
    const meshx_on_off_srv_el_msg_t *p_onoff_srv = (meshx_on_off_srv_el_msg_t *)params;
    meshx_api_relay_server_evt_t state;
    uint16_t element_id = p_onoff_srv->model.el_id;

    if (!IS_EL_IN_RANGE(element_id))
        return MESHX_SUCCESS;

    rel_el_id = GET_RELATIVE_EL_IDX(element_id);
    el_ctx = RELAY_SRV_EL(rel_el_id).srv_ctx;

    el_ctx->state.on_off = p_onoff_srv->on_off_state;

    err = meshx_nvs_element_ctx_set(element_id, el_ctx, sizeof(meshx_relay_srv_model_ctx_t));
    if (err != MESHX_SUCCESS)
        MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Failed to set relay element context: (%d)", err);

    state.on_off = el_ctx->state.on_off;
    err = meshx_send_msg_to_app(
        element_id,
        MESHX_ELEMENT_TYPE_RELAY_SERVER,
        RELAY_SIG_ONOFF_MODEL_ID,
        sizeof(meshx_api_relay_server_evt_t),
        &state);
    if (err != MESHX_SUCCESS)
        MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Failed to send relay state change message: (%d)", err);

    return MESHX_SUCCESS;
}

/**
 * @brief Callback function for relay server model events for Provisioning events.
 *
 * @param[in] pdev      Pointer to the device structure.
 * @param[in] evt       Relay server event type.
 * @param[in] params    Pointer to the parameters for the event.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
static meshx_err_t relay_prov_control_task_handler(dev_struct_t const *pdev, control_task_msg_evt_t evt, void const *params)
{
    MESHX_UNUSED(params);

    size_t rel_el_id = 0;
    meshx_gen_srv_cb_param_t gen_srv_send;
    meshx_err_t err = MESHX_SUCCESS;

    switch (evt)
    {
        case CONTROL_TASK_MSG_EVT_EN_NODE_PROV:
            for (size_t el_id = relay_element_init_ctrl.element_id_start; el_id < relay_element_init_ctrl.element_id_end; el_id++)
            {
                rel_el_id = GET_RELATIVE_EL_IDX(el_id);

                err = meshx_gen_on_off_srv_send_pack_create(
                    RELAY_SRV_EL(rel_el_id).onoff_srv_model->meshx_server_sig_model,
                    (uint16_t)el_id,
                    pdev->meshx_store.net_key_id,
                    RELAY_SRV_EL(rel_el_id).srv_ctx->app_id,
                    RELAY_SRV_EL(rel_el_id).srv_ctx->pub_addr,
                    RELAY_SRV_EL(rel_el_id).srv_ctx->state.on_off,
                    &gen_srv_send
                );

                if ((err != MESHX_SUCCESS)
                || (gen_srv_send.ctx.dst_addr == MESHX_ADDR_UNASSIGNED)
                || (gen_srv_send.ctx.app_idx == MESHX_KEY_UNUSED))
                {
                    continue;
                }

                err = meshx_gen_srv_send_msg_to_ble(
                    CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF_SRV,
                    &gen_srv_send
                );
                if (err)
                {
                    MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Failed to send ONOFF status message (Err: %x)", err);
                    return err;
                }
            }
            break;

        default:
            MESHX_LOGW(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Unhandled event: %d", evt);
            break;
    }
    return MESHX_SUCCESS;
}

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
static meshx_err_t meshx_relay_srv_msg_send_handler(
                    const dev_struct_t *pdev,
                    control_task_msg_evt_to_ble_t evt,
                    meshx_gen_srv_cb_param_t *params)
{
    if((evt & CONTROL_TASK_MSG_EVT_TO_BLE_GEN_SRV_MASK) == 0)
        return MESHX_SUCCESS;

    uint16_t element_id = params->model.el_id;

    if (!IS_EL_IN_RANGE(element_id))
        return MESHX_SUCCESS;

    meshx_err_t err = meshx_gen_on_off_srv_status_send(
        &params->model,
        &params->ctx,
        params->state_change.onoff_set.onoff
    );
    if(err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Mesh Model msg send failed (err: 0x%x)", err);
        return MESHX_ERR_PLAT;
    }

    ESP_UNUSED(pdev);
    return MESHX_SUCCESS;
}

/**
 * @brief Create Dynamic Relay Model Elements
 *
 * @param[in] pdev          Pointer to device structure
 * @param[in] element_cnt   Maximum number of relay models
 *
 * @return meshx_err_t
 */
meshx_err_t meshx_create_relay_elements(dev_struct_t *pdev, uint16_t element_cnt)
{
    meshx_err_t err;
    err = meshx_dev_create_relay_model_space(element_cnt);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Relay Model create failed: (%d)", err);
        return err;
    }
    err = meshx_add_relay_srv_model_to_element_list(pdev, (uint16_t *)&pdev->element_idx, element_cnt);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Relay Model create failed: (%d)", err);
        return err;
    }
#if CONFIG_ENABLE_CONFIG_SERVER
    err = meshx_config_server_cb_reg(
        (config_srv_cb_t)&relay_server_config_srv_cb,
        CONFIG_SERVER_CB_MASK);
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

    err = meshx_prov_srv_reg_el_server_cb((prov_srv_cb_t)&relay_prov_control_task_handler);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_ELEMENT_SWITCH_RELAY_SERVER, "Failed to register control task callback: (%d)", err);
        return err;
    }
    err = control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_TO_BLE,
        CONTROL_TASK_MSG_EVT_TO_BLE_GEN_SRV_MASK,
        (control_task_msg_handle_t)&meshx_relay_srv_msg_send_handler);
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
    return MESHX_SUCCESS;
}

REG_MESHX_ELEMENT_FN(relay_srv_el, MESHX_ELEMENT_TYPE_RELAY_SERVER, meshx_create_relay_elements);

#endif /* CONFIG_RELAY_SERVER_COUNT > 0 */
