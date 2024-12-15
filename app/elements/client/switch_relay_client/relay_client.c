#include "app_common.h"

#if CONFIG_RELAY_CLIENT_COUNT > 0
#include "relay_client_model.h"
#if CONFIG_ENABLE_CONFIG_SERVER
#include "config_server.h"

#define CONFIG_SERVER_CB_MASK \
    CONFIG_EVT_MODEL_PUB_ADD  \
    | CONFIG_EVT_MODEL_SUB_ADD | CONFIG_EVT_APP_BIND
#endif /* CONFIG_ENABLE_CONFIG_SERVER */

#define PROD_ONOFF_CB 1

#define GET_RELATIVE_EL_IDX(_element_id)    _element_id - relay_element_init_ctrl.element_id_start
#define IS_EL_IN_RANGE(_element_id)         (_element_id >= relay_element_init_ctrl.element_id_start \
                                            && _element_id < relay_element_init_ctrl.element_id_end)

static relay_client_elements_t relay_element_init_ctrl;

static esp_ble_mesh_model_t relay_sig_template = ESP_BLE_MESH_SIG_MODEL(
    ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_CLI,
    NULL, NULL, NULL);

static esp_err_t dev_create_relay_model_space(dev_struct_t const *pdev, uint16_t n_max)
{
    if (!pdev)
        return ESP_ERR_INVALID_STATE;

    /* Assign Spaces for Model List, Publish List and onoff gen list */
    relay_element_init_ctrl.model_cnt = n_max;

    for (size_t relay_model_id = 0; relay_model_id < n_max; relay_model_id++)
    {
        /* Perform memcpy to setup the constants */
        memcpy(&relay_element_init_ctrl.relay_cli_sig_model_list[relay_model_id][0],
               &relay_sig_template,
               sizeof(esp_ble_mesh_model_t));
        /* Set the dynamic spaces for the model */
        void **temp = (void **)&relay_element_init_ctrl.relay_cli_sig_model_list[relay_model_id][0].pub;
        *temp = relay_element_init_ctrl.relay_cli_pub_list + relay_model_id;
        relay_element_init_ctrl.relay_cli_sig_model_list[relay_model_id][0].user_data =
            relay_element_init_ctrl.relay_cli_onoff_gen_list + relay_model_id;
    }
    return ESP_OK;
}

static esp_err_t dev_add_relay_cli_model_to_element_list(dev_struct_t *pdev, uint16_t *start_idx, uint16_t n_max)
{
    if (!pdev)
        return ESP_ERR_INVALID_STATE;

    esp_ble_mesh_elem_t *elements = pdev->elements;

    relay_element_init_ctrl.element_id_start = *start_idx;

    for (size_t i = *start_idx; i < (n_max + *start_idx); i++)
    {
        if (i == 0)
        {
            /* Insert the first SIG model in root model to save element virtual addr space */
            memcpy(&elements[i].sig_models[1],
                   relay_element_init_ctrl.relay_cli_sig_model_list[i - *start_idx],
                   sizeof(esp_ble_mesh_model_t));
            uint8_t *ref_ptr = (uint8_t *)&elements[i].sig_model_count;
            (*ref_ptr)++;
        }
        else
        {
            elements[i].sig_models = relay_element_init_ctrl.relay_cli_sig_model_list[i - *start_idx];
            elements[i].vnd_models = ESP_BLE_MESH_MODEL_NONE;
            uint8_t *ref_ptr = (uint8_t *)&elements[i].sig_model_count;
            *ref_ptr = RELAY_CLI_MODEL_SIG_CNT;
            ref_ptr = (uint8_t *)&elements[i].vnd_model_count;
            *ref_ptr = RELAY_CLI_MODEL_VEN_CNT;
        }
    }
    /* Increment the index for further registrations */
    relay_element_init_ctrl.element_id_end = *start_idx += n_max;
    return ESP_OK;
}

#if PROD_ONOFF_CB
void relay_el_generic_client_cb(esp_ble_mesh_generic_client_cb_event_t event, const esp_ble_mesh_generic_client_cb_param_t *param)
{
    uint8_t element_id = param->params->model->element_idx;
    if (!IS_EL_IN_RANGE(element_id))
    {
        return;
    }

    size_t rel_el_id = GET_RELATIVE_EL_IDX(element_id);
    prod_onoff_ctx_t *el_ctx = &relay_element_init_ctrl.prod_onoff_ctx[rel_el_id];

    switch (event)
    {
    case ESP_BLE_MESH_GENERIC_CLIENT_GET_STATE_EVT:
        el_ctx->state = param->status_cb.onoff_status.present_onoff;
        ESP_LOGI(TAG, "GET resp: %d", el_ctx->state);
        break;
    case ESP_BLE_MESH_GENERIC_CLIENT_SET_STATE_EVT:
        el_ctx->state ^= 1;
        ESP_LOGI(TAG, "Next state: %d", el_ctx->state);
        break;
    case ESP_BLE_MESH_GENERIC_CLIENT_PUBLISH_EVT:
        break;
    case ESP_BLE_MESH_GENERIC_CLIENT_TIMEOUT_EVT:
        break;
    default:
        break;
    }
}

#if CONFIG_ENABLE_CONFIG_SERVER

static void relay_client_config_srv_cb(const esp_ble_mesh_cfg_server_cb_param_t *param, config_evt_t evt)
{
    prod_onoff_ctx_t *el_ctx = NULL;
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
        el_ctx = &relay_element_init_ctrl.prod_onoff_ctx[rel_el_id];
        el_ctx->app_id = param->value.state_change.appkey_add.app_idx;
        el_ctx->net_id = param->value.state_change.appkey_add.net_idx;
        
        break;
    case CONFIG_EVT_MODEL_PUB_ADD:
    case CONFIG_EVT_MODEL_PUB_DEL:
        element_id = param->value.state_change.mod_pub_set.element_addr - esp_ble_mesh_get_primary_element_address();
        if (!IS_EL_IN_RANGE(element_id))
            return;
        rel_el_id = GET_RELATIVE_EL_IDX(element_id);
        el_ctx = &relay_element_init_ctrl.prod_onoff_ctx[rel_el_id];
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

#endif /* #if PROD_ONOFF_CB */

esp_err_t send_relay_msg(dev_struct_t *pdev, uint16_t element_id, uint8_t set_get, uint8_t ack)
{
    if (!IS_EL_IN_RANGE(element_id))
        return ESP_ERR_INVALID_ARG;

    esp_err_t err = ESP_OK;
    esp_ble_mesh_client_common_param_t common = {0};
    esp_ble_mesh_generic_client_set_state_t set = {0};

    esp_ble_mesh_elem_t *element = &pdev->elements[element_id];
    esp_ble_mesh_model_t *model = &element->sig_models[0];

    size_t rel_el_id = element_id - relay_element_init_ctrl.element_id_start;
    prod_onoff_ctx_t *el_ctx = &relay_element_init_ctrl.prod_onoff_ctx[rel_el_id];

    common.model = model;
    if (false == set_get)
    {
        if (ack)
            common.opcode = ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET;
        else
            common.opcode = ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK;
    }
    else
        common.opcode = ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET;

    common.ctx.addr = el_ctx->pub_addr;
    common.ctx.net_idx = el_ctx->net_id;
    common.ctx.app_idx = el_ctx->app_id;
    common.msg_timeout = 0; /* 0 indicates that timeout value from menuconfig will be used */
    common.ctx.send_ttl = 3;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 2, 0)
    common.msg_role = ROLE_NODE;
#endif
    if (common.opcode != ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET)
    {
        set.onoff_set.op_en = false;
        set.onoff_set.onoff = el_ctx->state;
        set.onoff_set.tid = el_ctx->tid++;
    }
    err = esp_ble_mesh_generic_client_set_state(&common, &set);
    if (err)
    {
        ESP_LOGE(TAG, "Send Generic OnOff Set Unack failed");
        return err;
    }

    return ESP_OK;
}

esp_err_t create_relay_client_elements(dev_struct_t *pdev)
{
    esp_err_t err;

    err = dev_create_relay_model_space(pdev, CONFIG_RELAY_CLIENT_COUNT);
    if (err)
    {
        ESP_LOGE(TAG, "Relay Model space create failed: (%d)", err);
        return err;
    }

    err = dev_add_relay_cli_model_to_element_list(pdev, (uint16_t *)&pdev->element_idx, CONFIG_RELAY_CLIENT_COUNT);
    if (err)
    {
        ESP_LOGE(TAG, "Relay Model add to element create failed: (%d)", err);
        return err;
    }

#if PROD_ONOFF_CB
    err = prod_onoff_reg_cb((esp_ble_mesh_generic_client_cb_t)&relay_el_generic_client_cb);
    if (err)
    {
        ESP_LOGE(TAG, "Relay Model callback reg failed: (%d)", err);
        return err;
    }
#if CONFIG_ENABLE_CONFIG_SERVER
    err = prod_config_server_cb_reg(&relay_client_config_srv_cb, CONFIG_SERVER_CB_MASK);
    if (err)
    {
        ESP_LOGE(TAG, "Relay Model config server callback reg failed: (%d)", err);
        return err;
    }
#endif /* CONFIG_ENABLE_CONFIG_SERVER */
#endif

    err = prod_onoff_client_init();
    if (err)
    {
        ESP_LOGE(TAG, "prod_onoff_client_init failed: (%d)", err);
        return err;
    }

    return ESP_OK;
}
#endif /* CONFIG_RELAY_CLIENT_COUNT > 0*/
