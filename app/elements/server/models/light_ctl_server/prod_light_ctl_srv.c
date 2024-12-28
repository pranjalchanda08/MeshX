/**
 * @file prod_light_ctl_srv.c
 * @brief Implementation of the Light CTL Server model for BLE Mesh Node.
 *
 * This file contains the implementation of the Light CTL (Color Temperature Light) Server model
 * for the BLE Mesh Node. The Light CTL Server model is responsible for controlling the color
 * temperature and lightness of a light.
 *
 * @auther Pranjal Chanda
 */

#include <prod_light_ctl_srv.h>

#define TAG __func__
#define CTL_REPLY_PACK_LEN_MAX  9

uint8_t ctl_status_pack[CTL_REPLY_PACK_LEN_MAX];


/**
 * @brief Perform hardware change for the light control server model.
 *
 * This function is responsible for performing the necessary hardware changes
 * based on the parameters provided by the BLE Mesh lighting server callback.
 *
 * @param param Pointer to the BLE Mesh lighting server callback parameters.
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_FAIL: Failure
 */
static esp_err_t prod_perform_hw_change(esp_ble_mesh_lighting_server_cb_param_t *param)
{
    const esp_ble_mesh_light_ctl_srv_t *srv = (esp_ble_mesh_light_ctl_srv_t*) param->model->user_data;

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
                            CONTROL_TASK_MSG_EVT_TO_HAL_SET_CTL,
                            srv,
                            sizeof(esp_ble_mesh_light_ctl_srv_t));
        return err;
    }
    return ESP_ERR_NOT_ALLOWED;
}

/**
 * @brief Handle Light CTL messages for the lighting server model.
 *
 * This function processes incoming Light CTL messages and performs the necessary
 * actions based on the message parameters.
 *
 * @param param Pointer to the BLE Mesh lighting server callback parameter structure.
 *
 * @return
 *    - ESP_OK: Success
 *    - ESP_ERR_INVALID_ARG: Invalid argument
 *    - ESP_FAIL: Other failures
 */
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
                ESP_LOGI(TAG, "lightness %d, temperature %d, delta uv %d",
                     srv->state->lightness,
                     srv->state->temperature,
                     srv->state->delta_uv);
                err = prod_perform_hw_change(param);
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
                ESP_LOGI(TAG, "temperature %d, delta uv %d",
                        srv->state->temperature,
                        srv->state->delta_uv);
                err = prod_perform_hw_change(param);
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
                ESP_LOGI(TAG, "lightness %d, temperature %d, delta uv %d",
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
                ESP_LOGI(TAG, "temperature min %d, max %d",
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
    }

    return err;
}

/**
 * @brief Initialize the Light CTL Server model.
 *
 * This function initializes the Light CTL (Color Temperature Light) Server model
 * for the BLE Mesh Node. It sets up the necessary configurations and state for
 * the Light CTL Server to operate correctly.
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_FAIL: Failure
 */
esp_err_t prod_light_ctl_server_init(void)
{
    esp_err_t err = ESP_OK;

    err = prod_lighting_srv_init();
    if(err)
        ESP_LOGE(TAG, "Failed to initialize prod server");

    err = prod_lighting_reg_cb(ESP_BLE_MESH_MODEL_ID_LIGHT_CTL_SRV, prod_handle_light_ctl_msg);
    if(err)
        ESP_LOGE(TAG, "Failed to initialize prod_gen_srv_reg_cb (Err: %d)", err);

    return err;
}
