#include <prod_light_ctl_srv.h>

#define TAG __func__
#define CTL_REPLY_PACK_LEN_MAX  9

uint8_t ctl_status_pack[CTL_REPLY_PACK_LEN_MAX];
esp_ble_mesh_light_ctl_setup_srv_t light_ctl_setup_server;

static esp_err_t send_hw_msg(esp_ble_mesh_lighting_server_cb_param_t *param, control_task_msg_evt_t msg_evt)
{
    const esp_ble_mesh_light_ctl_srv_t *srv = (esp_ble_mesh_light_ctl_srv_t*) param->model->user_data;
    const esp_ble_mesh_light_ctl_state_t *state = srv->state;

    if (ESP_BLE_MESH_ADDR_IS_UNICAST(param->ctx.recv_dst) 
    || (ESP_BLE_MESH_ADDR_BROADCAST(param->ctx.recv_dst))
    || (ESP_BLE_MESH_ADDR_IS_GROUP(param->ctx.recv_dst) 
        && (esp_ble_mesh_is_model_subscribed_to_group(param->model, param->ctx.recv_dst))) 
    )
    {
        /* Send msg for hw manipulation */
        ESP_LOGI(TAG, "HW change requested, Element_id: 0x%x",
                    param->model->element_idx);

        esp_err_t err = control_task_send_msg(
                            CONTROL_TASK_MSG_CODE_TO_HAL, 
                            msg_evt,
                            state,
                            sizeof(esp_ble_mesh_light_ctl_srv_t));
        return err ? err : ESP_OK;
    }
    return ESP_ERR_NOT_ALLOWED;
}

static esp_err_t prod_perform_hw_change(esp_ble_mesh_lighting_server_cb_param_t *param, uint16_t status_op)
{
    esp_err_t err = ESP_OK;
    switch (status_op)
    {
        case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_STATUS:
            err += send_hw_msg(param, CONTROL_TASK_MSG_EVT_TO_HAL_SET_TEMP | CONTROL_TASK_MSG_EVT_TO_HAL_SET_LIGHTNESS);
            break;
        case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_STATUS:
            err += send_hw_msg(param, CONTROL_TASK_MSG_EVT_TO_HAL_SET_TEMP);
            break;
        default:
            break;
    }
    return err;
}

static esp_err_t prod_handle_light_ctl_msg(esp_ble_mesh_lighting_server_cb_param_t *param)
{
    esp_ble_mesh_light_ctl_srv_t *srv = (esp_ble_mesh_light_ctl_srv_t*) param->model->user_data;
    
    uint16_t status_op = 0;
    bool send_reply_to_src = false;
    uint8_t ctl_status_pack_idx = 0;
    uint32_t op_code = param->ctx.recv_op;
    esp_err_t err = ESP_OK;

    memset(ctl_status_pack, 0, sizeof(ctl_status_pack));

    switch (op_code)
    {
        /*!< Light CTL Message Opcode */
        case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_GET:
        case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_SET:
        case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_SET_UNACK:
            status_op = ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_STATUS;
            if(op_code != ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_GET){
                srv->state->temperature = param->value.state_change.ctl_set.temperature;
                srv->state->lightness = param->value.state_change.ctl_set.lightness;
                srv->state->delta_uv = param->value.state_change.ctl_set.delta_uv;
                ESP_LOGI(TAG, "lightness 0x%04x, temperature 0x%04x, delta uv 0x%04x",
                     srv->state->lightness,
                     srv->state->temperature,
                     srv->state->delta_uv);
                err = prod_perform_hw_change(param, status_op);
                if(err)
                    return err;
            }
            if(op_code != ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_SET_UNACK)
                send_reply_to_src = true;

            ctl_status_pack[ctl_status_pack_idx++] = srv->state->temperature >> 8;
            ctl_status_pack[ctl_status_pack_idx++] = srv->state->temperature & 0xFF;
            ctl_status_pack[ctl_status_pack_idx++] = srv->state->lightness >> 8;
            ctl_status_pack[ctl_status_pack_idx++] = srv->state->lightness & 0xFF;
            
            break;
        case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_GET:
        case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_SET:
        case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_SET_UNACK:
            status_op = ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_STATUS;
            if(op_code != ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_GET){
                srv->state->temperature = param->value.state_change.ctl_temp_set.temperature;
                srv->state->delta_uv = param->value.state_change.ctl_temp_set.delta_uv;
                ESP_LOGI(TAG, "temperature 0x%04x, delta uv 0x%04x",
                        srv->state->temperature,
                        srv->state->delta_uv);
                err = prod_perform_hw_change(param, status_op);
                if(err)
                    return err;
            }
            if(op_code != ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_SET_UNACK)
                send_reply_to_src = true;

            ctl_status_pack[ctl_status_pack_idx++] = srv->state->temperature >> 8;
            ctl_status_pack[ctl_status_pack_idx++] = srv->state->temperature & 0xFF;
            ctl_status_pack[ctl_status_pack_idx++] = srv->state->delta_uv >> 8;
            ctl_status_pack[ctl_status_pack_idx++] = srv->state->delta_uv & 0xFF;
            break;
        /*!< Light CTL Setup Message Opcode */
        case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_DEFAULT_SET:
        case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_DEFAULT_GET:
        case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_DEFAULT_SET_UNACK:
            if(op_code != ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_DEFAULT_GET)
            {
                ESP_LOGI(TAG, "lightness 0x%04x, temperature 0x%04x, delta uv 0x%04x",
                        param->value.state_change.ctl_default_set.lightness,
                        param->value.state_change.ctl_default_set.temperature,
                        param->value.state_change.ctl_default_set.delta_uv);
                srv->state->temperature_default = param->value.state_change.ctl_default_set.temperature;
                srv->state->lightness_default = param->value.state_change.ctl_default_set.lightness;
                srv->state->delta_uv_default = param->value.state_change.ctl_default_set.delta_uv;
            }
            if(op_code != ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_DEFAULT_SET_UNACK)
                send_reply_to_src = true;

            ctl_status_pack[ctl_status_pack_idx++] = srv->state->lightness_default >> 8;
            ctl_status_pack[ctl_status_pack_idx++] = srv->state->lightness_default & 0xFF;
            ctl_status_pack[ctl_status_pack_idx++] = srv->state->temperature_default >> 8;
            ctl_status_pack[ctl_status_pack_idx++] = srv->state->temperature_default & 0xFF;
            ctl_status_pack[ctl_status_pack_idx++] = srv->state->delta_uv_default >> 8;
            ctl_status_pack[ctl_status_pack_idx++] = srv->state->delta_uv_default & 0xFF;
            status_op = ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_DEFAULT_STATUS;
            break;
        case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_SET:
        case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_GET:
        case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_SET_UNACK:
            if(op_code != ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_GET)
            {
                ESP_LOGI(TAG, "temperature min 0x%04x, max 0x%04x",
                        param->value.state_change.ctl_temp_range_set.range_min,
                        param->value.state_change.ctl_temp_range_set.range_max);
                srv->state->temperature_range_min = param->value.state_change.ctl_temp_range_set.range_min;
                srv->state->temperature_range_max = param->value.state_change.ctl_temp_range_set.range_max;
            }
            if(op_code != ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_SET_UNACK)
            {
                send_reply_to_src = true;
            }
            ctl_status_pack[ctl_status_pack_idx++] = 1; // Status Code
            ctl_status_pack[ctl_status_pack_idx++] = srv->state->temperature_range_min >> 8;
            ctl_status_pack[ctl_status_pack_idx++] = srv->state->temperature_range_min & 0xFF;
            ctl_status_pack[ctl_status_pack_idx++] = srv->state->temperature_range_max & 0xFF;
            ctl_status_pack[ctl_status_pack_idx++] = srv->state->temperature_range_max >> 8;
            status_op = ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_STATUS;
            break;
        default:
            ESP_LOGW(TAG, "CTL Unhandled Event %p", (void*)param->ctx.recv_op);
            break;
    }
    if (send_reply_to_src)
    {
        /* ACK to Source */
        err = esp_ble_mesh_server_model_send_msg(param->model, 
                                &param->ctx,
                                status_op, 
                                ctl_status_pack_idx, ctl_status_pack);
        if(err)
            return err;
    }

    /* Publish current Status to the group */
    return esp_ble_mesh_model_publish(param->model,
                    status_op,
                    ctl_status_pack_idx,
                    ctl_status_pack,
                    ROLE_NODE);
}

esp_err_t prod_light_ctl_server_init(void)
{
    esp_err_t err = ESP_OK;
    err = prod_lighting_reg_cb(ESP_BLE_MESH_MODEL_ID_LIGHT_CTL_SRV, prod_handle_light_ctl_msg);
    if(err)
        ESP_LOGE(TAG, "Failed to initialize prod_gen_srv_reg_cb (Err: %d)", err);

    err = prod_lighting_srv_init();
    if(err)
        ESP_LOGE(TAG, "Failed to initialize prod server");

    return err;
}
