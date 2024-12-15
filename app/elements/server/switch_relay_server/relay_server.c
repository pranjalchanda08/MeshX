
#include "relay_server_model.h"

#if CONFIG_RELAY_SERVER_COUNT > 0

#if CONFIG_ENABLE_CONFIG_SERVER
#include "config_server.h"

#define CONFIG_SERVER_CB_MASK \
    CONFIG_EVT_MODEL_PUB_ADD  \
    | CONFIG_EVT_MODEL_SUB_ADD | CONFIG_EVT_APP_BIND
#endif /* CONFIG_ENABLE_CONFIG_SERVER */

#define RELAY_OFF 0
#define RELAY_ON !RELAY_OFF

#define GET_RELATIVE_EL_IDX(_element_id)    _element_id - relay_element_init_ctrl.element_id_start
#define IS_EL_IN_RANGE(_element_id)         (_element_id >= relay_element_init_ctrl.element_id_start \
                                            && _element_id < relay_element_init_ctrl.element_id_end)


static relay_elements_t relay_element_init_ctrl;

static esp_ble_mesh_model_t relay_sig_template = ESP_BLE_MESH_SIG_MODEL(
            ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV, NULL, NULL, NULL);

#if CONFIG_ENABLE_CONFIG_SERVER

static void relay_server_config_srv_cb(const esp_ble_mesh_cfg_server_cb_param_t *param, config_evt_t evt)
{
    prod_gen_ctx_t *el_ctx = NULL;
    size_t rel_el_id = 0;
    uint16_t element_id = 0;
    
    ESP_LOGD(TAG, "EVT: %p", (void *)evt);
    switch (evt)
    {

    case CONFIG_EVT_APP_BIND:
        element_id = param->value.state_change.mod_app_bind.element_addr - esp_ble_mesh_get_primary_element_address();
        if (!IS_EL_IN_RANGE(element_id)) 
            return;
        rel_el_id = GET_RELATIVE_EL_IDX(element_id);
        el_ctx = &relay_element_init_ctrl.prod_gen_ctx[rel_el_id];
        el_ctx->app_id = param->value.state_change.appkey_add.app_idx;
        el_ctx->net_id = param->value.state_change.appkey_add.net_idx;
        
        break;
    case CONFIG_EVT_MODEL_PUB_ADD:
    case CONFIG_EVT_MODEL_PUB_DEL:
        element_id = param->value.state_change.mod_pub_set.element_addr - esp_ble_mesh_get_primary_element_address();
        if (!IS_EL_IN_RANGE(element_id))  
            return;
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
#endif /* #if CONFIG_ENABLE_CONFIG_SERVER */

static esp_err_t dev_create_relay_model_space(uint16_t n_max)
{
    /* Assign Spaces for Model List, Publish List and onoff gen list */
    relay_element_init_ctrl.model_cnt = n_max;

    for (size_t relay_model_id = 0; relay_model_id < n_max; relay_model_id++)
    {
#if CONFIG_GEN_ONOFF_SERVER_COUNT
        relay_element_init_ctrl.relay_server_onoff_gen_list[relay_model_id].rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;
        relay_element_init_ctrl.relay_server_onoff_gen_list[relay_model_id].rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;
        /* Perform memcpy to setup the constants */
        memcpy( &relay_element_init_ctrl.relay_server_sig_model_list[relay_model_id][RELAY_SIG_ONOFF_MODEL_ID],
                &relay_sig_template,
                sizeof(esp_ble_mesh_model_t)
            );
        /* Set the dynamic spaces for the model */
        void ** temp = (void**) &relay_element_init_ctrl.relay_server_sig_model_list[relay_model_id][RELAY_SIG_ONOFF_MODEL_ID].pub;
        *temp = relay_element_init_ctrl.relay_server_pub_list + relay_model_id;
        relay_element_init_ctrl.relay_server_sig_model_list[relay_model_id][RELAY_SIG_ONOFF_MODEL_ID].user_data =
            relay_element_init_ctrl.relay_server_onoff_gen_list + relay_model_id;
#endif /* CONFIG_GEN_ONOFF_SERVER_COUNT */
    }
    return ESP_OK;
}

static esp_err_t dev_add_relay_srv_model_to_element_list(dev_struct_t *pdev, uint16_t *start_idx, uint16_t n_max)
{
    if (!pdev)
        return ESP_ERR_INVALID_STATE;

    if ((n_max + *start_idx) >= CONFIG_MAX_ELEMENT_COUNT)
    {
        ESP_LOGE(TAG, "No of elements limit reached");
        return ESP_ERR_NO_MEM;
    }

    esp_ble_mesh_elem_t *elements = pdev->elements;
    relay_element_init_ctrl.element_id_start = *start_idx;

    for (size_t i = *start_idx; i < (n_max + *start_idx); i++)
    {
        if (i == 0)
        {
            /* Insert the first SIG model in root model to save element virtual addr space */
            memcpy(&elements[i].sig_models[1],
                   relay_element_init_ctrl.relay_server_sig_model_list[i - *start_idx],
                   sizeof(esp_ble_mesh_model_t));
            uint8_t *ref_ptr = (uint8_t *)&elements[i].sig_model_count;
            (*ref_ptr)++;
        }
        else
        {
            elements[i].sig_models = relay_element_init_ctrl.relay_server_sig_model_list[i - *start_idx];
            elements[i].vnd_models = ESP_BLE_MESH_MODEL_NONE;
            uint8_t *ref_ptr = (uint8_t *)&elements[i].sig_model_count;
            *ref_ptr = RELAY_SRV_MODEL_SIG_CNT;
            ref_ptr = (uint8_t *)&elements[i].vnd_model_count;
            *ref_ptr = RELAY_SRV_MODEL_VEN_CNT;
        }
    }
    /* Increment the index for further registrations */
    relay_element_init_ctrl.element_id_end = *start_idx += n_max;
    return ESP_OK;
}

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
        ESP_LOGE(TAG, "Relay Model config server callback reg failed: (%d)", err);
        return err;
    }
#endif /* CONFIG_ENABLE_CONFIG_SERVER */
    err = prod_on_off_server_init();
    if (err)
    {
        ESP_LOGE(TAG, "prod_on_off_server_init failed: (%d)", err);
     
        return err;
    }
    return ESP_OK;
}
#endif /* CONFIG_RELAY_SERVER_COUNT > 0*/

