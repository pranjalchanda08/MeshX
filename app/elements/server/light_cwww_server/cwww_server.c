/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file cwww_server.c
 * @brief Implementation of the CW-WW server model for BLE Mesh.
 *
 * This file contains the implementation of the CW-WW server model for BLE Mesh,
 * including initialization, configuration, and event handling.
 */

#include <cwww_server_model.h>

#if CONFIG_LIGHT_CWWW_SRV_COUNT > 0

#if CONFIG_ENABLE_CONFIG_SERVER
#include "config_server.h"

/**
 * @brief Configuration server callback event mask for relay server.
 */
#define CONFIG_SERVER_CB_MASK \
    CONFIG_EVT_MODEL_PUB_ADD  \
    | CONFIG_EVT_MODEL_SUB_ADD | CONFIG_EVT_MODEL_APP_KEY_BIND
#endif /* CONFIG_ENABLE_CONFIG_SERVER */

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
        el_ctx->net_id = param->value.state_change.appkey_add.net_idx;
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
        ESP_LOGI(TAG, "PUB_ADD: %d, %d, 0x%x, 0x%x", element_id, rel_el_id, el_ctx->pub_addr, el_ctx->app_id);
        break;
    default:
        break;
    }
}
#endif /* CONFIG_ENABLE_CONFIG_SERVER */

/**
 * @brief Create space for CW-WW models.
 *
 * This function allocates and initializes the space required for CW-WW models.
 *
 * @param n_max Maximum number of models.
 * @return ESP_OK on success, or an error code on failure.
 */
static esp_err_t dev_create_cwww_model_space(uint16_t n_max)
{
    /* Assign Spaces for Model List, Publish List and onoff gen list */
    cwww_element_init_ctrl.model_cnt = n_max;
    void ** temp;
    for (size_t cwww_model_id = 0; cwww_model_id < n_max; cwww_model_id++)
    {
#if CONFIG_GEN_ONOFF_SERVER_COUNT
        cwww_element_init_ctrl.cwww_server_onoff_gen_list[cwww_model_id].rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;
        cwww_element_init_ctrl.cwww_server_onoff_gen_list[cwww_model_id].rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;

        /* Perform memcpy to setup the constants */
        memcpy( &cwww_element_init_ctrl.cwww_server_sig_model_list[cwww_model_id][CWWW_SIG_ONOFF_MODEL_ID],
                &cwww_sig_template[CWWW_SIG_ONOFF_MODEL_ID],
                sizeof(esp_ble_mesh_model_t)
            );
        /* Set the dynamic spaces for the model */
        temp = (void**) &cwww_element_init_ctrl.cwww_server_sig_model_list[cwww_model_id][CWWW_SIG_ONOFF_MODEL_ID].pub;
        *temp = &cwww_element_init_ctrl.cwww_server_pub_list[cwww_model_id][CWWW_SIG_ONOFF_MODEL_ID];
        cwww_element_init_ctrl.cwww_server_sig_model_list[cwww_model_id][CWWW_SIG_ONOFF_MODEL_ID].user_data =
            cwww_element_init_ctrl.cwww_server_onoff_gen_list + cwww_model_id;
#endif /* CONFIG_GEN_ONOFF_SERVER_COUNT */

#if CONFIG_ENABLE_LIGHT_CTL_SERVER
        cwww_element_init_ctrl.cwww_server_light_ctl_list[cwww_model_id].rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;
        cwww_element_init_ctrl.cwww_server_light_ctl_list[cwww_model_id].rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;
        cwww_element_init_ctrl.cwww_server_light_ctl_list[cwww_model_id].state = &cwww_element_init_ctrl.cwww_light_ctl_state[cwww_model_id];

        memcpy( &cwww_element_init_ctrl.cwww_server_sig_model_list[cwww_model_id][CWWW_SIG_L_CTL_MODEL_ID],
                &cwww_sig_template[CWWW_SIG_L_CTL_MODEL_ID],
                sizeof(esp_ble_mesh_model_t)
            );
        temp = (void**) &cwww_element_init_ctrl.cwww_server_sig_model_list[cwww_model_id][CWWW_SIG_L_CTL_MODEL_ID].pub;
        *temp = &cwww_element_init_ctrl.cwww_server_pub_list[cwww_model_id][CWWW_SIG_L_CTL_MODEL_ID];
        cwww_element_init_ctrl.cwww_server_sig_model_list[cwww_model_id][CWWW_SIG_L_CTL_MODEL_ID].user_data =
            cwww_element_init_ctrl.cwww_server_light_ctl_list + cwww_model_id;
#endif /* CONFIG_ENABLE_LIGHT_CTL_SERVER */
    }
    return ESP_OK;
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
static esp_err_t dev_add_cwww_srv_model_to_element_list(dev_struct_t *pdev, uint16_t *start_idx, uint16_t n_max)
{
    if (!pdev)
        return ESP_ERR_INVALID_STATE;

    if (!start_idx || (n_max + *start_idx) > CONFIG_MAX_ELEMENT_COUNT)
    {
        ESP_LOGE(TAG, "No of elements limit reached");
        return ESP_ERR_NO_MEM;
    }
    uint8_t *ref_ptr = NULL;
    esp_ble_mesh_elem_t *elements = pdev->elements;
    cwww_element_init_ctrl.element_id_start = *start_idx;

    for (size_t i = *start_idx; i < (n_max + *start_idx); i++)
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
    }
    /* Increment the index for further registrations */
    cwww_element_init_ctrl.element_id_end = *start_idx += n_max;
    return ESP_OK;
}

/**
 * @brief Create CW-WW elements.
 *
 * This function creates the CW-WW elements for the specified device.
 *
 * @param pdev Pointer to the device structure.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t create_cwww_elements(dev_struct_t *pdev)
{
    esp_err_t err;
    err = dev_create_cwww_model_space(CONFIG_LIGHT_CWWW_SRV_COUNT);
    if (err)
    {
        ESP_LOGE(TAG, "CWWW Model create failed: (%d)", err);
        return err;
    }
    err = dev_add_cwww_srv_model_to_element_list(pdev, (uint16_t *)&pdev->element_idx, CONFIG_LIGHT_CWWW_SRV_COUNT);
    if (err)
    {
        ESP_LOGE(TAG, "CWWW Model create failed: (%d)", err);

        return err;
    }
#if CONFIG_ENABLE_CONFIG_SERVER
    err = prod_config_server_cb_reg(&cwww_server_config_srv_cb, CONFIG_SERVER_CB_MASK);
    if (err)
    {
        ESP_LOGE(TAG, "Relay Model configserver callback reg failed: (%d)", err);
        return err;
    }
#endif /* CONFIG_ENABLE_CONFIG_SERVER */
    err = prod_on_off_server_init();
    if (err)
    {
        ESP_LOGE(TAG, "prod_on_off_server_init failed: (%d)", err);

        return err;
    }
    err = prod_light_ctl_server_init();
    if (err)
    {
        ESP_LOGE(TAG, "prod_light_ctl_server_init failed: (%d)", err);

        return err;
    }
    return ESP_OK;
}
#endif /* CONFIG_LIGHT_CWWW_SRV_COUNT > 0*/
