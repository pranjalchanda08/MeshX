#include "prod_onoff_server.h"

static esp_err_t prod_perform_hw_change(esp_ble_mesh_generic_server_cb_param_t *param)
{
    esp_ble_mesh_gen_onoff_srv_t const *srv = (esp_ble_mesh_gen_onoff_srv_t *)param->model->user_data;
    esp_ble_mesh_gen_onoff_state_t state = srv->state;

    if (ESP_BLE_MESH_ADDR_IS_UNICAST(param->ctx.recv_dst)
    || (ESP_BLE_MESH_ADDR_BROADCAST(param->ctx.recv_dst))
    || (ESP_BLE_MESH_ADDR_IS_GROUP(param->ctx.recv_dst)
        && (esp_ble_mesh_is_model_subscribed_to_group(param->model, param->ctx.recv_dst)))
    )
    {
        /* Send msg for hw manipulation */
        ESP_LOGI(TAG, "HW change requested, Element_id: 0x%x, state 0x%x",
                    param->model->element_idx,
                    state.onoff);

        esp_err_t err = control_task_send_msg(
                            CONTROL_TASK_MSG_CODE_TO_HAL,
                            CONTROL_TASK_MSG_EVT_TO_HAL_SET_ON_OFF,
                            srv,
                            sizeof(esp_ble_mesh_gen_onoff_srv_t));
        return err ? err : ESP_OK;
    }
    return ESP_ERR_NOT_ALLOWED;
}

static esp_err_t prod_handle_gen_onoff_msg(esp_ble_mesh_generic_server_cb_param_t *param)
{
    esp_ble_mesh_gen_onoff_srv_t *srv = (esp_ble_mesh_gen_onoff_srv_t *)param->model->user_data;

    switch (param->ctx.recv_op)
    {
        case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET:
            break;
        case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET:
        case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK:
            srv->state.onoff = param->value.state_change.onoff_set.onoff;
            ESP_LOGI(TAG, "state_change: %d", srv->state.onoff);
            if(param->ctx.recv_dst != param->model->pub->publish_addr)
            {
                /* Here the message wa received from unregistered source and mention the state to the respective client */
                ESP_LOGI(TAG, "Publishing to 0x%x", param->model->pub->publish_addr);
                param->ctx.addr = param->model->pub->publish_addr;
                esp_ble_mesh_server_model_send_msg(param->model,
                                            &param->ctx,
                                            ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS,
                                            sizeof(srv->state.onoff), &srv->state.onoff);
            }
            ESP_ERROR_CHECK(prod_perform_hw_change(param));
            break;
        default:
            break;
    }
    return ESP_OK;
}

esp_err_t prod_on_off_server_init()
{
    esp_err_t err;
    err = prod_gen_srv_reg_cb(ESP_BLE_MESH_MODEL_ID_GEN_ONOFF_SRV, prod_handle_gen_onoff_msg);
    if(err)
        ESP_LOGE(TAG, "Failed to initialize prod_gen_srv_reg_cb (Err: %d)", err);

#if CONFIG_ENABLE_SERVER_COMMON
    err = prod_gen_srv_init();
    if(err)
        ESP_LOGE(TAG, "Failed to initialize prod server");
#endif /* CONFIG_ENABLE_SERVER_COMMON */
    return err;
}
