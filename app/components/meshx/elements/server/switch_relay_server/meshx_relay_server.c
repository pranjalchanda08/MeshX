/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_relay_server.c
 * @brief Relay server model implementation for BLE Mesh networks.
 *
 * This file contains the implementation of the relay server model for BLE Mesh.
 * It includes functions to initialize, configure, and manage relay server elements.
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
#define CONFIG_SERVER_CB_MASK \
    CONFIG_EVT_MODEL_PUB_ADD  \
    | CONFIG_EVT_MODEL_SUB_ADD | CONFIG_EVT_MODEL_APP_KEY_BIND
#endif /* CONFIG_ENABLE_CONFIG_SERVER */

#define CONTROL_TASK_EVT_MASK   CONTROL_TASK_MSG_EVT_EL_STATE_CH_SET_ON_OFF
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

/**
 * @brief Structure to manage relay element initialization.
 */
static relay_elements_t relay_element_init_ctrl;

/**
 * @brief Template for SIG model initialization.
 */
static const MESHX_MODEL relay_sig_template = ESP_BLE_MESH_SIG_MODEL(
    ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV, NULL, NULL, NULL);

#if CONFIG_ENABLE_CONFIG_SERVER

/**
 * @brief Callback function for configuration server events.
 *
 * This function handles events from the configuration server, such as model publication
 * and application binding events.
 *
 * @param param Pointer to the callback parameter structure.
 * @param evt Configuration event type.
 */
static void relay_server_config_srv_cb(const esp_ble_mesh_cfg_server_cb_param_t *param, config_evt_t evt)
{
    relay_srv_model_ctx_t *el_ctx = NULL;
    size_t rel_el_id = 0;
    uint16_t element_id = 0;

    ESP_LOGD(TAG, "EVT: %p", (void *)evt);
    switch (evt)
    {
    case CONFIG_EVT_MODEL_APP_KEY_BIND:
        element_id = (param->value.state_change.mod_app_bind.element_addr - esp_ble_mesh_get_primary_element_address());
        if (!IS_EL_IN_RANGE(element_id))
            break;
        rel_el_id = GET_RELATIVE_EL_IDX(element_id);
        el_ctx = &relay_element_init_ctrl.meshx_gen_ctx[rel_el_id];
        el_ctx->app_id = param->value.state_change.appkey_add.app_idx;
        break;
    case CONFIG_EVT_MODEL_PUB_ADD:
    case CONFIG_EVT_MODEL_PUB_DEL:
        element_id = (param->value.state_change.mod_pub_set.element_addr - esp_ble_mesh_get_primary_element_address());
        if (!IS_EL_IN_RANGE(element_id))
            break;
        rel_el_id = GET_RELATIVE_EL_IDX(element_id);
        el_ctx = &relay_element_init_ctrl.meshx_gen_ctx[rel_el_id];
        el_ctx->pub_addr = evt == CONFIG_EVT_MODEL_PUB_ADD ? param->value.state_change.mod_pub_set.pub_addr
                                                           : ESP_BLE_MESH_ADDR_UNASSIGNED;
        el_ctx->app_id = param->value.state_change.mod_pub_set.app_idx;
        ESP_LOGI(TAG, "PUB_ADD: %d, %d, 0x%x, 0x%x", element_id, rel_el_id, el_ctx->pub_addr, el_ctx->app_id);
        break;
    default:
        break;
    }
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

    relay_element_init_ctrl.element_cnt = n_max;
    relay_element_init_ctrl.element_id_end = 0;
    relay_element_init_ctrl.element_id_start = 0;

    relay_element_init_ctrl.meshx_gen_ctx = (relay_srv_model_ctx_t *) calloc(n_max, sizeof(relay_srv_model_ctx_t));
    if (!relay_element_init_ctrl.meshx_gen_ctx)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for relay server context");
        return MESHX_NO_MEM;
    }
    relay_element_init_ctrl.relay_server_sig_model_list = (MESHX_MODEL **)calloc(n_max, sizeof(MESHX_MODEL *));
    if (!relay_element_init_ctrl.relay_server_sig_model_list)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for relay server SIG model list");
        return MESHX_NO_MEM;
    }
    else
    {
        for (size_t i = 0; i < n_max; i++)
        {
            relay_element_init_ctrl.relay_server_sig_model_list[i] = (MESHX_MODEL *)calloc(RELAY_SRV_MODEL_SIG_CNT, sizeof(MESHX_MODEL));
            if (!relay_element_init_ctrl.relay_server_sig_model_list[i])
            {
                ESP_LOGE(TAG, "Failed to allocate memory for relay server SIG model list");
                return MESHX_NO_MEM;
            }
        }
    }
    relay_element_init_ctrl.relay_server_pub_list = (MESHX_MODEL_PUB *)calloc(n_max, sizeof(MESHX_MODEL_PUB));
    if (!relay_element_init_ctrl.relay_server_pub_list)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for relay server publication list");
        return MESHX_NO_MEM;
    }
    relay_element_init_ctrl.relay_server_onoff_gen_list = (MESHX_GEN_ONOFF_SRV *)calloc(n_max, sizeof(MESHX_GEN_ONOFF_SRV));
    if (!relay_element_init_ctrl.relay_server_onoff_gen_list)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for relay server onoff generic list");
        return MESHX_NO_MEM;
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
 * @param n_max The maximum number of elements in the server signature model list
 *              and server publication list.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully deinitialized the mesh element structure.
 */
static meshx_err_t meshx_element_struct_deinit(uint16_t n_max)
{
    if (relay_element_init_ctrl.meshx_gen_ctx)
    {
        free(relay_element_init_ctrl.meshx_gen_ctx);
        relay_element_init_ctrl.meshx_gen_ctx = NULL;
    }
    if (relay_element_init_ctrl.relay_server_sig_model_list)
    {
        for (size_t i = 0; i < n_max; i++)
        {
            if (relay_element_init_ctrl.relay_server_sig_model_list[i])
            {
                free(relay_element_init_ctrl.relay_server_sig_model_list[i]);
                relay_element_init_ctrl.relay_server_sig_model_list[i] = NULL;
            }
        }
        free(relay_element_init_ctrl.relay_server_sig_model_list);
        relay_element_init_ctrl.relay_server_sig_model_list = NULL;
    }
    if (relay_element_init_ctrl.relay_server_pub_list)
    {
        free(relay_element_init_ctrl.relay_server_pub_list);
        relay_element_init_ctrl.relay_server_pub_list = NULL;
    }
    if (relay_element_init_ctrl.relay_server_onoff_gen_list)
    {
        free(relay_element_init_ctrl.relay_server_onoff_gen_list);
        relay_element_init_ctrl.relay_server_onoff_gen_list = NULL;
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
        ESP_LOGE(TAG, "Relay Model space create failed: (%d)", err);
        meshx_element_struct_deinit(n_max);
        return err;
    }
    for (size_t relay_model_id = 0; relay_model_id < n_max; relay_model_id++)
    {
#if CONFIG_GEN_ONOFF_SERVER_COUNT
        relay_element_init_ctrl.relay_server_onoff_gen_list[relay_model_id].rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;
        relay_element_init_ctrl.relay_server_onoff_gen_list[relay_model_id].rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;
        memcpy(&relay_element_init_ctrl.relay_server_sig_model_list[relay_model_id][RELAY_SIG_ONOFF_MODEL_ID],
               &relay_sig_template,
               sizeof(MESHX_MODEL));
        void **temp = (void **)&relay_element_init_ctrl.relay_server_sig_model_list[relay_model_id][RELAY_SIG_ONOFF_MODEL_ID].pub;
        *temp = relay_element_init_ctrl.relay_server_pub_list + relay_model_id;
        relay_element_init_ctrl.relay_server_sig_model_list[relay_model_id][RELAY_SIG_ONOFF_MODEL_ID].user_data =
            relay_element_init_ctrl.relay_server_onoff_gen_list + relay_model_id;
#endif /* CONFIG_GEN_ONOFF_SERVER_COUNT */
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
    meshx_err_t err = MESHX_SUCCESS;
    relay_srv_model_ctx_t const *el_ctx = &relay_element_init_ctrl.meshx_gen_ctx[element_id];
    MESHX_GEN_ONOFF_SRV *srv = NULL;
    for (size_t i = 0; i < RELAY_SRV_MODEL_SIG_CNT; i++)
    {
        srv = (MESHX_GEN_ONOFF_SRV *)relay_element_init_ctrl.relay_server_sig_model_list[element_id][i].user_data;
        srv->state.onoff = el_ctx->state;
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
        ESP_LOGE(TAG, "No of elements limit reached");
        return MESHX_NO_MEM;
    }

    meshx_err_t err = MESHX_SUCCESS;
    esp_ble_mesh_elem_t *elements = pdev->elements;
    relay_element_init_ctrl.element_id_start = *start_idx;

    for (uint16_t i = *start_idx; i < (n_max + *start_idx); i++)
    {
        if (i == 0)
        {
            memcpy(&elements[i].sig_models[1],
                   relay_element_init_ctrl.relay_server_sig_model_list[i],
                   sizeof(MESHX_MODEL));
            uint8_t *ref_ptr = (uint8_t *)&elements[i].sig_model_count;
            (*ref_ptr)++;
        }
        else
        {
            elements[i].sig_models = relay_element_init_ctrl.relay_server_sig_model_list[i - *start_idx];
            elements[i].vnd_models = ESP_BLE_MESH_MODEL_NONE; // No vendor models are assigned to this element
            uint8_t *ref_ptr = (uint8_t *)&elements[i].sig_model_count;
            // Set the number of SIG models for the relay server element
            *ref_ptr = RELAY_SRV_MODEL_SIG_CNT;
            ref_ptr = (uint8_t *)&elements[i].vnd_model_count;
            *ref_ptr = RELAY_SRV_MODEL_VEN_CNT;
        }
        err = meshx_nvs_element_ctx_get(i, &(relay_element_init_ctrl.meshx_gen_ctx[i - *start_idx]), sizeof(relay_element_init_ctrl.meshx_gen_ctx[i]));
        if (err != MESHX_SUCCESS)
        {
            ESP_LOGW(TAG, "Failed to get relay element context: (0x%x)", err);
        }
        else
        {
            err = meshx_restore_model_states(i - *start_idx);
            if (err != MESHX_SUCCESS)
            {
                ESP_LOGW(TAG, "Failed to restore relay model states: (0x%x)", err);
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
static meshx_err_t meshx_el_control_task_handler(dev_struct_t const *pdev, control_task_msg_evt_t evt, void *params)
{
    MESHX_UNUSED(pdev);
    MESHX_UNUSED(evt);
    size_t rel_el_id = 0;
    meshx_err_t err = MESHX_SUCCESS;
    relay_srv_model_ctx_t *el_ctx = NULL;
    MESHX_GEN_ONOFF_SRV const *p_onoff_srv = (MESHX_GEN_ONOFF_SRV*) params;
    meshx_el_relay_server_evt_t state;
    uint16_t element_id = p_onoff_srv->model->element_idx;

    if (!IS_EL_IN_RANGE(element_id))
        return MESHX_SUCCESS;

    rel_el_id = GET_RELATIVE_EL_IDX(element_id);
    el_ctx = &relay_element_init_ctrl.meshx_gen_ctx[rel_el_id];

    p_onoff_srv = (MESHX_GEN_ONOFF_SRV const *) params;
    el_ctx->state = p_onoff_srv->state.onoff;

    err = meshx_nvs_element_ctx_set(element_id, el_ctx, sizeof(relay_srv_model_ctx_t));
    if (err != MESHX_SUCCESS)
        ESP_LOGE(TAG, "Failed to set relay element context: (%d)", err);

    state.on_off = el_ctx->state;
    err = meshx_send_msg_to_app(element_id, MESHX_ELEMENT_TYPE_RELAY_SERVER, RELAY_SIG_ONOFF_MODEL_ID, sizeof(meshx_el_relay_server_evt_t), &state);
    if (err != MESHX_SUCCESS)
        ESP_LOGE(TAG, "Failed to send relay state change message: (%d)", err);

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
    MESHX_UNUSED(pdev);
    MESHX_UNUSED(evt);
    MESHX_UNUSED(params);

    size_t rel_el_id = 0;
    esp_ble_mesh_msg_ctx_t ctx;
    meshx_err_t err = MESHX_SUCCESS;

    for(size_t el_id = relay_element_init_ctrl.element_id_start; el_id < relay_element_init_ctrl.element_id_end; el_id++)
    {
        rel_el_id   = GET_RELATIVE_EL_IDX(el_id);
        ctx.net_idx = pdev->meshx_store.net_key_id;
        ctx.app_idx = relay_element_init_ctrl.meshx_gen_ctx[rel_el_id].app_id;
        ctx.addr    = relay_element_init_ctrl.meshx_gen_ctx[rel_el_id].pub_addr;
        ctx.send_ttl = ESP_BLE_MESH_TTL_DEFAULT;
        ctx.send_cred = 0;
        ctx.send_tag = BIT(1);

        if(ctx.addr == ESP_BLE_MESH_ADDR_UNASSIGNED || ctx.app_idx == ESP_BLE_MESH_KEY_UNUSED)
            continue;
        err = esp_ble_mesh_server_model_send_msg(&relay_element_init_ctrl.relay_server_sig_model_list[rel_el_id][RELAY_SIG_ONOFF_MODEL_ID],
                                           &ctx,
                                           ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS,
                                           sizeof(relay_element_init_ctrl.meshx_gen_ctx[rel_el_id].state),
                                           &relay_element_init_ctrl.meshx_gen_ctx[rel_el_id].state);
        if (err)
        {
            ESP_LOGE(TAG, "Failed to send ONOFF status message (Err: %x)", err);
            return err;
        }
    }

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
meshx_err_t create_relay_elements(dev_struct_t *pdev, uint16_t element_cnt)
{
    meshx_err_t err;
    err = meshx_dev_create_relay_model_space(element_cnt);
    if (err)
    {
        ESP_LOGE(TAG, "Relay Model create failed: (%d)", err);
        return err;
    }
    err = meshx_add_relay_srv_model_to_element_list(pdev, (uint16_t *)&pdev->element_idx, element_cnt);
    if (err)
    {
        ESP_LOGE(TAG, "Relay Model create failed: (%d)", err);
        return err;
    }
#if CONFIG_ENABLE_CONFIG_SERVER
    err = meshx_config_server_cb_reg(&relay_server_config_srv_cb, CONFIG_SERVER_CB_MASK);
    if (err)
    {
        ESP_LOGE(TAG, "Relay Model configserver callback reg failed: (%d)", err);
        return err;
    }
#endif /* CONFIG_ENABLE_CONFIG_SERVER */
    err = control_task_msg_subscribe(
            CONTROL_TASK_MSG_CODE_EL_STATE_CH,
            CONTROL_TASK_EVT_MASK,
            (control_task_msg_handle_t)&meshx_el_control_task_handler);
    if (err)
    {
        ESP_LOGE(TAG, "Failed to register control task callback: (%d)", err);
        return err;
    }

    err = control_task_msg_subscribe(
            CONTROL_TASK_MSG_CODE_PROVISION,
            CONTROL_TASK_MSG_EVT_EN_NODE_PROV,
            (control_task_msg_handle_t)&relay_prov_control_task_handler);
    if (err)
    {
        ESP_LOGE(TAG, "Failed to register control task callback: (%d)", err);
        return err;
    }
    err = meshx_on_off_server_init();
    if (err)
    {
        ESP_LOGE(TAG, "meshx_on_off_server_init failed: (%d)", err);
        return err;
    }
    return MESHX_SUCCESS;
}

REG_MESHX_ELEMENT_FN(relay_srv_el, MESHX_ELEMENT_TYPE_RELAY_SERVER, create_relay_elements);

#endif /* CONFIG_RELAY_SERVER_COUNT > 0 */
