/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file relay_server.c
 * @brief Relay server model implementation for BLE Mesh networks.
 *
 * This file contains the implementation of the relay server model for BLE Mesh.
 * It includes functions to initialize, configure, and manage relay server elements.
 */

#include "relay_server_model.h"
#include "meshx_nvs.h"

#if CONFIG_RELAY_SERVER_COUNT > 0

#if CONFIG_ENABLE_CONFIG_SERVER
#include "config_server.h"

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
static const esp_ble_mesh_model_t relay_sig_template = ESP_BLE_MESH_SIG_MODEL(
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
        el_ctx = &relay_element_init_ctrl.prod_gen_ctx[rel_el_id];
        el_ctx->app_id = param->value.state_change.appkey_add.app_idx;
        break;
    case CONFIG_EVT_MODEL_PUB_ADD:
    case CONFIG_EVT_MODEL_PUB_DEL:
        element_id = (param->value.state_change.mod_pub_set.element_addr - esp_ble_mesh_get_primary_element_address());
        if (!IS_EL_IN_RANGE(element_id))
            break;
        rel_el_id = GET_RELATIVE_EL_IDX(element_id);
        el_ctx = &relay_element_init_ctrl.prod_gen_ctx[rel_el_id];
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
 * @brief Create relay model space.
 *
 * Allocates memory and initializes space for relay models.
 *
 * @param n_max Maximum number of relay models.
 * @return ESP_OK on success, error code otherwise.
 */
static esp_err_t dev_create_relay_model_space(uint16_t n_max)
{
    relay_element_init_ctrl.model_cnt = n_max;

    for (size_t relay_model_id = 0; relay_model_id < n_max; relay_model_id++)
    {
#if CONFIG_GEN_ONOFF_SERVER_COUNT
        relay_element_init_ctrl.relay_server_onoff_gen_list[relay_model_id].rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;
        relay_element_init_ctrl.relay_server_onoff_gen_list[relay_model_id].rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;
        memcpy(&relay_element_init_ctrl.relay_server_sig_model_list[relay_model_id][RELAY_SIG_ONOFF_MODEL_ID],
               &relay_sig_template,
               sizeof(esp_ble_mesh_model_t));
        void **temp = (void **)&relay_element_init_ctrl.relay_server_sig_model_list[relay_model_id][RELAY_SIG_ONOFF_MODEL_ID].pub;
        *temp = relay_element_init_ctrl.relay_server_pub_list + relay_model_id;
        relay_element_init_ctrl.relay_server_sig_model_list[relay_model_id][RELAY_SIG_ONOFF_MODEL_ID].user_data =
            relay_element_init_ctrl.relay_server_onoff_gen_list + relay_model_id;
#endif /* CONFIG_GEN_ONOFF_SERVER_COUNT */
    }
    return ESP_OK;
}

/**
 * @brief Add relay server models to the element list.
 *
 * Registers the relay server models to the BLE Mesh element list.
 *
 * @param pdev Pointer to the device structure.
 * @param start_idx Pointer to the start index of elements.
 * @param n_max Maximum number of elements to add.
 * @return ESP_OK on success, error code otherwise.
 */
static esp_err_t dev_add_relay_srv_model_to_element_list(dev_struct_t *pdev, uint16_t *start_idx, uint16_t n_max)
{
    if (!pdev)
        return ESP_ERR_INVALID_STATE;

    if ((n_max + *start_idx) > CONFIG_MAX_ELEMENT_COUNT)
    {
        ESP_LOGE(TAG, "No of elements limit reached");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = ESP_OK;
    esp_ble_mesh_elem_t *elements = pdev->elements;
    relay_element_init_ctrl.element_id_start = *start_idx;

    for (uint16_t i = *start_idx; i < (n_max + *start_idx); i++)
    {
        if (i == 0)
        {
            memcpy(&elements[i].sig_models[1],
                   relay_element_init_ctrl.relay_server_sig_model_list[i],
                   sizeof(esp_ble_mesh_model_t));
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
        err = meshx_nvs_elemnt_ctx_get(i, &(relay_element_init_ctrl.prod_gen_ctx[i - *start_idx]), sizeof(relay_element_init_ctrl.prod_gen_ctx[i]));
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to get relay element context: (0x%x)", err);
        }
    }
    relay_element_init_ctrl.element_id_end = (*start_idx += n_max);
    return ESP_OK;
}

/**
 * @brief Callback function for relay server model events.
 *
 * This function handles events from the relay server model, such as setting the relay state.
 *
 * @param param Pointer to the callback parameter structure.
 * @return ESP_OK on success, error code otherwise.
 */
static esp_err_t meshx_el_control_task_handler(dev_struct_t const *pdev, control_task_msg_evt_t evt, void *params)
{
    ESP_UNUSED(pdev);
    size_t rel_el_id = 0;
    esp_err_t err = ESP_OK;
    relay_srv_model_ctx_t *el_ctx = NULL;
    esp_ble_mesh_gen_onoff_srv_t const *p_onoff_srv = NULL;
    esp_ble_mesh_model_t const *p_model = (esp_ble_mesh_model_t *) params;

    uint16_t element_id = p_model->element_idx;

    if (!IS_EL_IN_RANGE(element_id))
        return ESP_OK;

    rel_el_id = GET_RELATIVE_EL_IDX(element_id);
    el_ctx = &relay_element_init_ctrl.prod_gen_ctx[rel_el_id];

    p_onoff_srv = (esp_ble_mesh_gen_onoff_srv_t const *) params;
    el_ctx->state = p_onoff_srv->state.onoff;

    err = meshx_nvs_elemnt_ctx_set(element_id, el_ctx, sizeof(relay_srv_model_ctx_t));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set relay element context: (%d)", err);
    }
    return ESP_OK;
}

/**
 * @brief Create relay elements.
 *
 * Initializes and registers relay elements in the BLE Mesh network.
 *
 * @param pdev Pointer to the device structure.
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t create_relay_elements(dev_struct_t *pdev)
{
    esp_err_t err;
    err = dev_create_relay_model_space(CONFIG_RELAY_SERVER_COUNT);
    if (err)
    {
        ESP_LOGE(TAG, "Relay Model create failed: (%d)", err);
        return err;
    }
    err = dev_add_relay_srv_model_to_element_list(pdev, (uint16_t *)&pdev->element_idx, CONFIG_RELAY_SERVER_COUNT);
    if (err)
    {
        ESP_LOGE(TAG, "Relay Model create failed: (%d)", err);
        return err;
    }
#if CONFIG_ENABLE_CONFIG_SERVER
    err = prod_config_server_cb_reg(&relay_server_config_srv_cb, CONFIG_SERVER_CB_MASK);
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
    err = prod_on_off_server_init();
    if (err)
    {
        ESP_LOGE(TAG, "prod_on_off_server_init failed: (%d)", err);
        return err;
    }
    return ESP_OK;
}
#endif /* CONFIG_RELAY_SERVER_COUNT > 0 */
