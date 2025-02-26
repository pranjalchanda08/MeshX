/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file cwww_server.c
 * @brief Implementation of the CW-WW server model for BLE Mesh.
 *
 * This file contains the implementation of the CW-WW server model for BLE Mesh,
 * including initialization, configuration, and event handling.
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
#define CONFIG_SERVER_CB_MASK \
    CONFIG_EVT_MODEL_PUB_ADD  \
    | CONFIG_EVT_MODEL_SUB_ADD | CONFIG_EVT_MODEL_APP_KEY_BIND
#endif /* CONFIG_ENABLE_CONFIG_SERVER */

#define CONTROL_TASK_EVT_MASK   CONTROL_TASK_MSG_EVT_EL_STATE_CH_SET_ON_OFF \
                              | CONTROL_TASK_MSG_EVT_EL_STATE_CH_SET_CTL

#define GET_RELATIVE_EL_IDX(_element_id)    _element_id - cwww_element_init_ctrl.element_id_start
#define IS_EL_IN_RANGE(_element_id)         (_element_id >= cwww_element_init_ctrl.element_id_start \
                                            && _element_id < cwww_element_init_ctrl.element_id_end)

#define CWWW_TEMP_MIN   2700
#define CWWW_TEMP_MAX   6500

static cwww_elements_t cwww_element_init_ctrl;

static const esp_ble_mesh_model_t cwww_sig_template[CWWW_SRV_MODEL_SIG_CNT] =
{
    ESP_BLE_MESH_SIG_MODEL(ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV, NULL, NULL, NULL),
    ESP_BLE_MESH_SIG_MODEL(ESP_BLE_MESH_MODEL_ID_LIGHT_CTL_SRV, NULL, NULL, NULL),
};

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
static void cwww_server_config_srv_cb(const esp_ble_mesh_cfg_server_cb_param_t *param, config_evt_t evt)
{
    cwww_server_ctx_t *el_ctx = NULL;
    size_t rel_el_id = 0;
    uint16_t element_id = 0;
    bool nvs_save = false;

    ESP_LOGD(TAG, "EVT: %p", (void *)evt);
    switch (evt)
    {
    case CONFIG_EVT_MODEL_APP_KEY_BIND:
        element_id = param->value.state_change.mod_app_bind.element_addr - esp_ble_mesh_get_primary_element_address();
        if (!IS_EL_IN_RANGE(element_id))
            break;
        rel_el_id = GET_RELATIVE_EL_IDX(element_id);
        el_ctx = &cwww_element_init_ctrl.cwww_server_ctx[rel_el_id];
        el_ctx->app_id = param->value.state_change.appkey_add.app_idx;
        nvs_save = true;

        break;
    case CONFIG_EVT_MODEL_PUB_ADD:
    case CONFIG_EVT_MODEL_PUB_DEL:
        element_id = param->value.state_change.mod_pub_set.element_addr - esp_ble_mesh_get_primary_element_address();
        if (!IS_EL_IN_RANGE(element_id))
            break;
        rel_el_id = GET_RELATIVE_EL_IDX(element_id);
        el_ctx = &cwww_element_init_ctrl.cwww_server_ctx[rel_el_id];
        el_ctx->pub_addr = evt == CONFIG_EVT_MODEL_PUB_ADD ? param->value.state_change.mod_pub_set.pub_addr
                                                           : ESP_BLE_MESH_ADDR_UNASSIGNED;
        el_ctx->app_id = param->value.state_change.mod_pub_set.app_idx;
        nvs_save = true;
        ESP_LOGI(TAG, "PUB_ADD: %d, %d, 0x%x, 0x%x", element_id, rel_el_id, el_ctx->pub_addr, el_ctx->app_id);
        break;
    default:
        break;
    }
    if(nvs_save)
    {
        esp_err_t err = meshx_nvs_elemnt_ctx_set(element_id, el_ctx, sizeof(cwww_server_ctx_t));
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set cwww server element context: (%d)", err);
        }
    }
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
 * @param n_max The maximum number of elements in the server signature model list
 *              and server publication list.
 *
 * @return
 *     - ESP_OK: Successfully deinitialized the mesh element structure.
 */
static esp_err_t meshx_element_struct_init(uint16_t n_max)
{
    cwww_element_init_ctrl.element_cnt = n_max;
    cwww_element_init_ctrl.element_id_end = 0;
    cwww_element_init_ctrl.element_id_start = 0;

    cwww_element_init_ctrl.cwww_server_ctx = (cwww_server_ctx_t *)calloc(n_max, sizeof(cwww_server_ctx_t));
    if (!cwww_element_init_ctrl.cwww_server_ctx)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for cwww server context");
        return ESP_ERR_NO_MEM;
    }
    cwww_element_init_ctrl.cwww_server_sig_model_list = (esp_ble_mesh_model_t **)calloc(n_max, sizeof(esp_ble_mesh_model_t *));
    if (!cwww_element_init_ctrl.cwww_server_sig_model_list)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for cwww server sig model list");
        return ESP_ERR_NO_MEM;
    }
    else
    {
        for (size_t i = 0; i < n_max; i++)
        {
            cwww_element_init_ctrl.cwww_server_sig_model_list[i] = (esp_ble_mesh_model_t *)calloc(CWWW_SRV_MODEL_SIG_CNT, sizeof(esp_ble_mesh_model_t));
            if (!cwww_element_init_ctrl.cwww_server_sig_model_list[i])
            {
                ESP_LOGE(TAG, "Failed to allocate memory for cwww server sig model list");
                return ESP_ERR_NO_MEM;
            }
        }
    }
    cwww_element_init_ctrl.cwww_server_pub_list = (esp_ble_mesh_model_pub_t **)calloc(n_max, sizeof(esp_ble_mesh_model_pub_t *));
    if (!cwww_element_init_ctrl.cwww_server_pub_list)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for cwww server pub list");
        return ESP_ERR_NO_MEM;
    }
    else
    {
        for (size_t i = 0; i < n_max; i++)
        {
            cwww_element_init_ctrl.cwww_server_pub_list[i] = (esp_ble_mesh_model_pub_t *)calloc(CWWW_SRV_MODEL_SIG_CNT, sizeof(esp_ble_mesh_model_pub_t));
            if (!cwww_element_init_ctrl.cwww_server_pub_list[i])
            {
                ESP_LOGE(TAG, "Failed to allocate memory for cwww server pub list");
                return ESP_ERR_NO_MEM;
            }
        }
    }
    cwww_element_init_ctrl.cwww_server_onoff_gen_list = (esp_ble_mesh_gen_onoff_srv_t *)calloc(n_max, sizeof(esp_ble_mesh_gen_onoff_srv_t));
    if (!cwww_element_init_ctrl.cwww_server_onoff_gen_list)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for cwww server onoff gen list");
        return ESP_ERR_NO_MEM;
    }
    cwww_element_init_ctrl.cwww_server_light_ctl_list = (esp_ble_mesh_light_ctl_srv_t *)calloc(n_max, sizeof(esp_ble_mesh_light_ctl_srv_t));
    if (!cwww_element_init_ctrl.cwww_server_light_ctl_list)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for cwww server light ctl list");
        return ESP_ERR_NO_MEM;
    }
    cwww_element_init_ctrl.cwww_light_ctl_state = (esp_ble_mesh_light_ctl_state_t *)calloc(n_max, sizeof(esp_ble_mesh_light_ctl_state_t));
    if (!cwww_element_init_ctrl.cwww_light_ctl_state)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for cwww light ctl state");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
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
 *     - ESP_OK: Successfully deinitialized the mesh element structure.
 */

static esp_err_t meshx_element_struct_deinit(uint16_t n_max)
{
    if (cwww_element_init_ctrl.cwww_server_ctx)
    {
        free(cwww_element_init_ctrl.cwww_server_ctx);
        cwww_element_init_ctrl.cwww_server_ctx = NULL;
    }
    if (cwww_element_init_ctrl.cwww_server_sig_model_list)
    {
        for (size_t i = 0; i < n_max; i++)
        {
            if (cwww_element_init_ctrl.cwww_server_sig_model_list[i])
            {
                free(cwww_element_init_ctrl.cwww_server_sig_model_list[i]);
                cwww_element_init_ctrl.cwww_server_sig_model_list[i] = NULL;
            }
        }
        free(cwww_element_init_ctrl.cwww_server_sig_model_list);
        cwww_element_init_ctrl.cwww_server_sig_model_list = NULL;
    }
    if (cwww_element_init_ctrl.cwww_server_pub_list)
    {
        for (size_t i = 0; i < n_max; i++)
        {
            if (cwww_element_init_ctrl.cwww_server_pub_list[i])
            {
                free(cwww_element_init_ctrl.cwww_server_pub_list[i]);
                cwww_element_init_ctrl.cwww_server_pub_list[i] = NULL;
            }
        }
        free(cwww_element_init_ctrl.cwww_server_pub_list);
        cwww_element_init_ctrl.cwww_server_pub_list = NULL;
    }
    if (cwww_element_init_ctrl.cwww_server_onoff_gen_list)
    {
        free(cwww_element_init_ctrl.cwww_server_onoff_gen_list);
        cwww_element_init_ctrl.cwww_server_onoff_gen_list = NULL;
    }
    if (cwww_element_init_ctrl.cwww_server_light_ctl_list)
    {
        free(cwww_element_init_ctrl.cwww_server_light_ctl_list);
        cwww_element_init_ctrl.cwww_server_light_ctl_list = NULL;
    }
    if (cwww_element_init_ctrl.cwww_light_ctl_state)
    {
        free(cwww_element_init_ctrl.cwww_light_ctl_state);
        cwww_element_init_ctrl.cwww_light_ctl_state = NULL;
    }
    return ESP_OK;
}
/**
 * @brief Create space for CW-WW models.
 *
 * This function allocates and initializes the space required for CW-WW models.
 *
 * @param n_max Maximum number of models.
 * @return ESP_OK on success, or an error code on failure.
 */
static esp_err_t meshx_dev_create_cwww_model_space(uint16_t n_max)
{
    /* Assign Spaces for Model List, Publish List and onoff gen list */
    void ** temp;
    esp_err_t err = meshx_element_struct_init(n_max);
    if (err)
    {
        ESP_LOGE(TAG, "Failed to initialize cwww element structures: (%d)", err);
        meshx_element_struct_deinit(n_max);
        return err;
    }
    for (size_t cwww_rel_el_id = 0; cwww_rel_el_id < n_max; cwww_rel_el_id++)
    {
#if CONFIG_GEN_ONOFF_SERVER_COUNT
        cwww_element_init_ctrl.cwww_server_onoff_gen_list[cwww_rel_el_id].rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;
        cwww_element_init_ctrl.cwww_server_onoff_gen_list[cwww_rel_el_id].rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;

        /* Perform memcpy to setup the constants */
        memcpy( &cwww_element_init_ctrl.cwww_server_sig_model_list[cwww_rel_el_id][CWWW_SIG_ONOFF_MODEL_ID],
                &cwww_sig_template[CWWW_SIG_ONOFF_MODEL_ID],
                sizeof(esp_ble_mesh_model_t)
            );
        /* Set the dynamic spaces for the model */
        temp = (void**) &cwww_element_init_ctrl.cwww_server_sig_model_list[cwww_rel_el_id][CWWW_SIG_ONOFF_MODEL_ID].pub;
        *temp = &cwww_element_init_ctrl.cwww_server_pub_list[cwww_rel_el_id][CWWW_SIG_ONOFF_MODEL_ID];
        cwww_element_init_ctrl.cwww_server_sig_model_list[cwww_rel_el_id][CWWW_SIG_ONOFF_MODEL_ID].user_data =
            cwww_element_init_ctrl.cwww_server_onoff_gen_list + cwww_rel_el_id;
#endif /* CONFIG_GEN_ONOFF_SERVER_COUNT */

#if CONFIG_ENABLE_LIGHT_CTL_SERVER
        cwww_element_init_ctrl.cwww_server_light_ctl_list[cwww_rel_el_id].rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;
        cwww_element_init_ctrl.cwww_server_light_ctl_list[cwww_rel_el_id].rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;
        cwww_element_init_ctrl.cwww_server_light_ctl_list[cwww_rel_el_id].state = &cwww_element_init_ctrl.cwww_light_ctl_state[cwww_rel_el_id];

        memcpy( &cwww_element_init_ctrl.cwww_server_sig_model_list[cwww_rel_el_id][CWWW_SIG_L_CTL_MODEL_ID],
                &cwww_sig_template[CWWW_SIG_L_CTL_MODEL_ID],
                sizeof(esp_ble_mesh_model_t)
            );
        temp = (void**) &cwww_element_init_ctrl.cwww_server_sig_model_list[cwww_rel_el_id][CWWW_SIG_L_CTL_MODEL_ID].pub;
        *temp = &cwww_element_init_ctrl.cwww_server_pub_list[cwww_rel_el_id][CWWW_SIG_L_CTL_MODEL_ID];
        cwww_element_init_ctrl.cwww_server_sig_model_list[cwww_rel_el_id][CWWW_SIG_L_CTL_MODEL_ID].user_data =
            cwww_element_init_ctrl.cwww_server_light_ctl_list + cwww_rel_el_id;
#endif /* CONFIG_ENABLE_LIGHT_CTL_SERVER */
    }
    return err;
}

/**
 * @brief Restore saved CW-WW model states.
 *
 * This function restores the CW-WW model states from the NVS.
 *
 * @param element_id    Relative Element ID.
 * @return ESP_OK on success, or an error code on failure.
 */
static esp_err_t meshx_restore_model_states(uint16_t element_id)
{
    esp_err_t err = ESP_OK;
    cwww_server_ctx_t const *el_ctx = &cwww_element_init_ctrl.cwww_server_ctx[element_id];

    for (size_t i = 0; i < CWWW_SRV_MODEL_SIG_CNT; i++)
    {
        if (cwww_element_init_ctrl.cwww_server_sig_model_list[element_id][i].model_id == ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV)
        {
            esp_ble_mesh_gen_onoff_srv_t *srv = (esp_ble_mesh_gen_onoff_srv_t *)cwww_element_init_ctrl.cwww_server_sig_model_list[element_id][i].user_data;
            srv->state.onoff = el_ctx->prev_state.on_off;
        }
        else if (cwww_element_init_ctrl.cwww_server_sig_model_list[element_id][i].model_id == ESP_BLE_MESH_MODEL_ID_LIGHT_CTL_SRV)
        {
            esp_ble_mesh_light_ctl_srv_t *srv = (esp_ble_mesh_light_ctl_srv_t *)cwww_element_init_ctrl.cwww_server_sig_model_list[element_id][i].user_data;
            srv->state->delta_uv = el_ctx->prev_ctl_state.delta_uv;
            srv->state->lightness = el_ctx->prev_ctl_state.lightness;
            srv->state->temperature = el_ctx->prev_ctl_state.temperature;
            srv->state->temperature_range_min = el_ctx->prev_ctl_state.temp_range_min;
            srv->state->temperature_range_max = el_ctx->prev_ctl_state.temp_range_max;
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
 * @return ESP_OK on success, or an error code on failure.
 */
static esp_err_t meshx_add_cwww_srv_model_to_element_list(dev_struct_t *pdev, uint16_t *start_idx, uint16_t n_max)
{
    if (!pdev)
        return ESP_ERR_INVALID_STATE;

    if (!start_idx || (n_max + *start_idx) > CONFIG_MAX_ELEMENT_COUNT)
    {
        ESP_LOGE(TAG, "No of elements limit reached");
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = ESP_OK;
    uint8_t *ref_ptr = NULL;
    esp_ble_mesh_elem_t *elements = pdev->elements;
    cwww_element_init_ctrl.element_id_start = *start_idx;

    for (uint16_t i = *start_idx; i < (n_max + *start_idx); i++)
    {
        if (i == 0)
        {
            /* Insert the first SIG model in root model to save element virtual addr space */
            memcpy(&elements[i].sig_models[1],
                   cwww_element_init_ctrl.cwww_server_sig_model_list[i - *start_idx],
                   sizeof(esp_ble_mesh_model_t));
            ref_ptr = (uint8_t *)&elements[i].sig_model_count;
            (*ref_ptr)++;
        }
        else
        {
            elements[i].sig_models = cwww_element_init_ctrl.cwww_server_sig_model_list[i - *start_idx];
            elements[i].vnd_models = ESP_BLE_MESH_MODEL_NONE;
            ref_ptr = (uint8_t *)&elements[i].sig_model_count;
            *ref_ptr = CWWW_SRV_MODEL_SIG_CNT;
            ref_ptr = (uint8_t *)&elements[i].vnd_model_count;
            *ref_ptr = CWWW_SRV_MODEL_VEN_CNT;
        }

        err = meshx_nvs_elemnt_ctx_get(i, &(cwww_element_init_ctrl.cwww_server_ctx[i - *start_idx]), sizeof(cwww_server_ctx_t));
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to get cwww element context: (0x%x)", err);
        }
        else
        {
            err = meshx_restore_model_states(i - *start_idx);
            if (err != ESP_OK)
            {
                ESP_LOGW(TAG, "Failed to restore cwww model states: (0x%x)", err);
            }
        }
    }
    /* Increment the index for further registrations */
    cwww_element_init_ctrl.element_id_end = *start_idx += n_max;
    return ESP_OK;
}

/**
 * @brief CW-WW server model event handler.
 *
 * This function handles events from the CW-WW server model.
 *
 * Event Types:
 * - CONTROL_TASK_MSG_EVT_EL_STATE_CH_SET_ON_OFF: Handles on/off state changes.
 *   - params: Pointer to esp_ble_mesh_gen_onoff_srv_t containing the new on/off state.
 *   - Updates the on/off state of the corresponding element context.
 *
 * - CONTROL_TASK_MSG_EVT_EL_STATE_CH_SET_CTL: Handles CTL (Color Temperature Lightness) state changes.
 *   - params: Pointer to esp_ble_mesh_light_ctl_srv_t containing the new CTL state.
 *   - Updates the delta UV, lightness, temperature, and temperature range of the corresponding element context.
 *
 * If the element ID is out of range, the function exits without making any changes.
 * @param pdev Pointer to the device structure.
 * @param evt Event type.
 * @param params Pointer to the event parameters.
 * @return ESP_OK on success, or an error code on failure.
 */
static esp_err_t meshx_el_control_task_handler(dev_struct_t const *pdev, control_task_msg_evt_t evt, void const *params)
{
    ESP_UNUSED(pdev);
    esp_err_t err = ESP_OK;
    uint16_t element_id = 0;
    size_t rel_el_id;
    cwww_server_ctx_t *el_ctx = NULL;
    meshx_el_light_cwww_server_evt_t app_msg;
    cwww_sig_id_t sig_func = CWWW_SIG_ONOFF_MODEL_ID;

    switch (evt) {
        case CONTROL_TASK_MSG_EVT_EL_STATE_CH_SET_ON_OFF:
        {
            const esp_ble_mesh_gen_onoff_srv_t *p_onoff_srv = (const esp_ble_mesh_gen_onoff_srv_t *)params;
            element_id = p_onoff_srv->model->element_idx;
            if (!IS_EL_IN_RANGE(element_id))
                goto el_ctrl_task_hndlr_exit;

            rel_el_id = GET_RELATIVE_EL_IDX(element_id);
            el_ctx = &cwww_element_init_ctrl.cwww_server_ctx[rel_el_id];
            if(el_ctx->prev_state.on_off == p_onoff_srv->state.onoff)
                goto el_ctrl_task_hndlr_exit;

            el_ctx->prev_state.on_off = p_onoff_srv->state.onoff;
            sig_func = CWWW_SIG_ONOFF_MODEL_ID;
            app_msg.state_change.on_off.state = el_ctx->prev_state.on_off;
            break;
        }
        case CONTROL_TASK_MSG_EVT_EL_STATE_CH_SET_CTL:
        {
            const esp_ble_mesh_light_ctl_srv_t *p_ctl_srv = (const esp_ble_mesh_light_ctl_srv_t *)params;
            element_id = p_ctl_srv->model->element_idx;
            if (!IS_EL_IN_RANGE(element_id)) goto el_ctrl_task_hndlr_exit;
            rel_el_id = GET_RELATIVE_EL_IDX(element_id);
            el_ctx = &cwww_element_init_ctrl.cwww_server_ctx[rel_el_id];

            if(el_ctx->prev_ctl_state.delta_uv == p_ctl_srv->state->delta_uv &&
               el_ctx->prev_ctl_state.lightness == p_ctl_srv->state->lightness &&
               el_ctx->prev_ctl_state.temperature == p_ctl_srv->state->temperature &&
               el_ctx->prev_ctl_state.temp_range_min == p_ctl_srv->state->temperature_range_min &&
               el_ctx->prev_ctl_state.temp_range_max == p_ctl_srv->state->temperature_range_max)
                goto el_ctrl_task_hndlr_exit;

            el_ctx->prev_ctl_state.delta_uv = p_ctl_srv->state->delta_uv;
            el_ctx->prev_ctl_state.lightness = p_ctl_srv->state->lightness;
            el_ctx->prev_ctl_state.temperature = p_ctl_srv->state->temperature;
            el_ctx->prev_ctl_state.temp_range_min = p_ctl_srv->state->temperature_range_min;
            el_ctx->prev_ctl_state.temp_range_max = p_ctl_srv->state->temperature_range_max;

            sig_func = CWWW_SIG_L_CTL_MODEL_ID;
            app_msg.state_change.ctl.delta_uv = el_ctx->prev_ctl_state.delta_uv;
            app_msg.state_change.ctl.lightness = el_ctx->prev_ctl_state.lightness;
            app_msg.state_change.ctl.temperature = el_ctx->prev_ctl_state.temperature;
            app_msg.state_change.ctl.temp_range_min = el_ctx->prev_ctl_state.temp_range_min;
            app_msg.state_change.ctl.temp_range_max = el_ctx->prev_ctl_state.temp_range_max;
            break;
        }
        default:
            break;
    }

    err = meshx_nvs_elemnt_ctx_set(element_id, el_ctx, sizeof(cwww_server_ctx_t));
    if (err != ESP_OK)
        ESP_LOGE(TAG, "Failed to set relay element context: (%d)", err);

    err = meshx_send_msg_to_app(element_id, MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER, (uint16_t) sig_func, sizeof(meshx_el_light_cwww_server_evt_t), &app_msg);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "Failed to send relay state change message: (%d)", err);

el_ctrl_task_hndlr_exit:
    return ESP_OK;
}

/**
 * @brief Callback function for relay server model events for Provisioning events.
 *
 * @param param Pointer to the callback parameter structure.
 * @param evt Relay server event type.
 * @param params Pointer to the parameters for the event.
 * @return ESP_OK on success, error code otherwise.
 */
static esp_err_t cwww_prov_control_task_handler(dev_struct_t const *pdev, control_task_msg_evt_t evt, void const *params)
{
    ESP_UNUSED(pdev);
    ESP_UNUSED(evt);
    ESP_UNUSED(params);

    size_t rel_el_id = 0;
    esp_ble_mesh_msg_ctx_t ctx;
    esp_err_t err = ESP_OK;

    for(size_t el_id = cwww_element_init_ctrl.element_id_start; el_id < cwww_element_init_ctrl.element_id_end; el_id++)
    {
        rel_el_id   = GET_RELATIVE_EL_IDX(el_id);
        ctx.net_idx = pdev->meshx_store.net_key_id;
        ctx.app_idx = cwww_element_init_ctrl.cwww_server_ctx[rel_el_id].app_id;
        ctx.addr    = cwww_element_init_ctrl.cwww_server_ctx[rel_el_id].pub_addr;
        ctx.send_ttl = ESP_BLE_MESH_TTL_DEFAULT;
        ctx.send_cred = 0;
        ctx.send_tag = BIT(1);

        if(ctx.addr == ESP_BLE_MESH_ADDR_UNASSIGNED || ctx.app_idx == ESP_BLE_MESH_KEY_UNUSED)
            continue;

        err = esp_ble_mesh_server_model_send_msg(&cwww_element_init_ctrl.cwww_server_sig_model_list[rel_el_id][CWWW_SIG_ONOFF_MODEL_ID],
                                                 &ctx,
                                                 ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS,
                                                 sizeof(cwww_element_init_ctrl.cwww_server_ctx[rel_el_id].state.on_off),
                                                 &cwww_element_init_ctrl.cwww_server_ctx[rel_el_id].state.on_off);
        if (err)
        {
            ESP_LOGE(TAG, "Failed to send ONOFF status message (Err: %x)", err);
            return err;
        }

        err = meshx_send_ctl_status(&cwww_element_init_ctrl.cwww_server_sig_model_list[rel_el_id][CWWW_SIG_L_CTL_MODEL_ID],
                                   &ctx,
                                   cwww_element_init_ctrl.cwww_server_ctx[rel_el_id].prev_ctl_state.lightness,
                                   cwww_element_init_ctrl.cwww_server_ctx[rel_el_id].prev_ctl_state.temperature);
        if (err)
        {
            ESP_LOGE(TAG, "Failed to send CTL status message (Err: %x)", err);
            return err;
        }
    }

    return err;
}

/**
 * @brief Create CW-WW elements.
 *
 * This function creates the CW-WW elements for the specified device.
 *
 * @param pdev Pointer to the device structure.
 * @param element_cnt Maximum number of CW-WW elements.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t create_cwww_elements(dev_struct_t *pdev, uint16_t element_cnt)
{
    esp_err_t err;
    err = meshx_dev_create_cwww_model_space(element_cnt);
    if (err)
    {
        ESP_LOGE(TAG, "CWWW Model create failed: (%d)", err);
        return err;
    }
    err = meshx_add_cwww_srv_model_to_element_list(pdev, (uint16_t *)&pdev->element_idx, element_cnt);
    if (err)
    {
        ESP_LOGE(TAG, "CWWW Model create failed: (%d)", err);

        return err;
    }
#if CONFIG_ENABLE_CONFIG_SERVER
    err = meshx_config_server_cb_reg(&cwww_server_config_srv_cb, CONFIG_SERVER_CB_MASK);
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
            (control_task_msg_handle_t)&cwww_prov_control_task_handler);
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
    err = meshx_light_ctl_server_init();
    if (err)
    {
        ESP_LOGE(TAG, "meshx_light_ctl_server_init failed: (%d)", err);

        return err;
    }
    return ESP_OK;
}

REG_MESHX_ELEMENT_FN(cwww_srv_el, MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER, create_cwww_elements);

#endif /* CONFIG_LIGHT_CWWW_SRV_COUNT > 0*/
