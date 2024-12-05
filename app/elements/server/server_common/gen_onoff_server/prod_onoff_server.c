#include "prod_onoff_server.h"

#define TAG "ONOFF_SRV"

static esp_err_t prod_perform_hw_change(esp_ble_mesh_generic_server_cb_param_t *param)
{
    if (ESP_BLE_MESH_ADDR_IS_UNICAST(param->ctx.recv_dst) 
    || (ESP_BLE_MESH_ADDR_BROADCAST(param->ctx.recv_dst))
    || (ESP_BLE_MESH_ADDR_IS_GROUP(param->ctx.recv_dst) 
        && (esp_ble_mesh_is_model_subscribed_to_group(param->model, param->ctx.recv_dst))) 
    )
    {
        /* Send msg for hw manipulation */
        ESP_LOGI(TAG, "HW change requested, Element_id: 0x%x, stateL 0x%x",
                    param->model->element_idx,
                    param->value.set.onoff.onoff);
        return ESP_OK;
    }
    return ESP_ERR_NOT_ALLOWED;
}

static esp_err_t prod_handle_gen_onoff_msg(esp_ble_mesh_generic_server_cb_param_t *param)
{
    esp_ble_mesh_gen_onoff_srv_t *srv = (esp_ble_mesh_gen_onoff_srv_t *)param->model->user_data;
    srv->state.onoff = param->value.set.onoff.onoff;

    bool send_status_msg = false;

    switch (param->ctx.recv_op)
    {
    case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET:
        send_status_msg = true;
        break;
    case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET:
    case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK:

        send_status_msg = param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET;
        
        /* Publish STATUS to respective subcribers */
        esp_ble_mesh_model_publish(param->model, 
                                    ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS,
                                    sizeof(srv->state.onoff), 
                                    &srv->state.onoff, 
                                    ROLE_NODE);
        ESP_ERROR_CHECK(prod_perform_hw_change(param));
        break;
    default:
        break;
    }
    if (send_status_msg)
        esp_ble_mesh_server_model_send_msg(param->model, 
                                    &param->ctx,
                                    ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS, 
                                    sizeof(srv->state.onoff), &srv->state.onoff);
    
    return ESP_OK;
}

esp_err_t prod_on_off_server_init()
{
    esp_err_t err;
    err = prod_srv_reg_cb(ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV, prod_handle_gen_onoff_msg);
    if(err)
        ESP_LOGE(TAG, "Failed to initialize prod_srv_reg_cb (Err: %d)", err);

#if CONFIG_ENABLE_SERVER_COMMON
    err = prod_srv_init();
    if(err)
        ESP_LOGE(TAG, "Failed to initialize prod server", err);
#endif /* CONFIG_ENABLE_SERVER_COMMON */
    return prod_srv_init();
}
