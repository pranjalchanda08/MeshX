/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_light_ctl_srv.c
 * @brief Implementation of the Light CTL Server model for BLE Mesh Node.
 *
 * This file contains the implementation of the Light CTL (Color Temperature Light) Server model
 * for the BLE Mesh Node. The Light CTL Server model is responsible for controlling the color
 * temperature and lightness of a light.
 *
 */

#include <meshx_light_ctl_srv.h>

#define TAG __func__

/**
 * @brief Light CTL status packet.
 */
typedef union ctl_status_pack{
    struct{
        uint16_t lightness;         /**< Lightness level */
        uint16_t temperature;       /**< Color temperature */
    }ctl_status;
    struct{
        uint16_t temperature;       /**< Color temperature */
        uint16_t delta_uv;          /**< Delta UV value */
    }ctl_temp_status;
    struct{
        uint16_t lightness_def;     /**< Default lightness */
        uint16_t temperature_def;   /**< Default temperature */
        uint16_t delta_uv_def;      /**< Default delta UV */
    }ctl_default;
    struct{
        uint8_t status_code;        /**< Status code */
        uint16_t range_min;         /**< Minimum temperature range */
        uint16_t range_max;         /**< Maximum temperature range */
    }ctl_temp_range;
}ctl_status_t;

/**
 * @brief Perform hardware change for the light control server model.
 *
 * This function is responsible for performing the necessary hardware changes
 * based on the parameters provided by the BLE Mesh lighting server callback.
 *
 * @param param Pointer to the BLE Mesh lighting server callback parameters.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_FAIL: Failure
 */
static meshx_err_t meshx_perform_hw_change(esp_ble_mesh_lighting_server_cb_param_t *param)
{
    const esp_ble_mesh_light_ctl_srv_t *srv = (esp_ble_mesh_light_ctl_srv_t*) param->model->user_data;

    if (ESP_BLE_MESH_ADDR_IS_UNICAST(param->ctx.recv_dst)
    || (ESP_BLE_MESH_ADDR_BROADCAST(param->ctx.recv_dst))
    || (ESP_BLE_MESH_ADDR_IS_GROUP(param->ctx.recv_dst)
        && (esp_ble_mesh_is_model_subscribed_to_group(param->model, param->ctx.recv_dst)))
    )
    {
        /* Send msg for hw manipulation */
        ESP_LOGD(TAG, "HW change requested, Element_id: 0x%x",
                    param->model->element_idx);

        meshx_err_t err = control_task_msg_publish(
                            CONTROL_TASK_MSG_CODE_EL_STATE_CH,
                            CONTROL_TASK_MSG_EVT_EL_STATE_CH_SET_CTL,
                            srv,
                            sizeof(esp_ble_mesh_light_ctl_srv_t));
        return err;
    }
    return MESHX_NOT_SUPPORTED;
}

#if !CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE
/**
 * @brief Handle Light CTL messages for the lighting server model.
 *
 * This function processes incoming Light CTL messages and performs the necessary
 * actions based on the message parameters.
 *
 * @param param Pointer to the BLE Mesh lighting server callback parameter structure.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_INVALID_ARG: Invalid argument
 *    - MESHX_FAIL: Other failures
 */
static meshx_err_t meshx_handle_light_ctl_msg(esp_ble_mesh_lighting_server_cb_param_t *param)
{
#else
/**
 * @brief Handle Light CTL messages for the lighting server model.
 *
 * This function processes incoming Light CTL messages and performs the necessary
 * actions based on the message parameters.
 *
 * @param pdev Pointer to the device structure.
 * @param evt Event type of the control task message.
 * @param param Pointer to the BLE Mesh lighting server callback parameter structure.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_INVALID_ARG: Invalid argument
 *    - MESHX_FAIL: Other failures
 */
static meshx_err_t meshx_handle_light_ctl_msg(const dev_struct_t *pdev,
    const control_task_msg_evt_t evt,
    esp_ble_mesh_lighting_server_cb_param_t *param)
{
    if(!pdev || (evt != ESP_BLE_MESH_MODEL_ID_LIGHT_CTL_SRV && evt != ESP_BLE_MESH_MODEL_ID_LIGHT_CTL_SETUP_SRV))
        return MESHX_INVALID_ARG;
#endif /* !CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE */
    esp_ble_mesh_light_ctl_srv_t *srv = (esp_ble_mesh_light_ctl_srv_t*) param->model->user_data;

    uint16_t status_op = 0;
    bool send_reply_to_src = false;
    uint8_t ctl_status_pack_len = 0;
    uint32_t op_code = param->ctx.recv_op;
    meshx_err_t err = MESHX_SUCCESS;

    ctl_status_t ctl_status_union;

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
                ESP_LOGD(TAG, "lightness|temp|del_uv:%d|%d|%d",
                     srv->state->lightness,
                     srv->state->temperature,
                     srv->state->delta_uv);
                err = meshx_perform_hw_change(param);
                if(err)
                    return err;
            }
            if(op_code != ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_SET_UNACK)
                send_reply_to_src = true;

            ctl_status_union.ctl_status.temperature = srv->state->temperature;
            ctl_status_union.ctl_status.lightness = srv->state->lightness;
            ctl_status_pack_len = sizeof(ctl_status_union.ctl_status);

            break;
        case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_GET:
        case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_SET:
        case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_SET_UNACK:
            status_op = ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_STATUS;
            if(op_code != ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_GET){
                srv->state->temperature = param->value.state_change.ctl_temp_set.temperature;
                srv->state->delta_uv = param->value.state_change.ctl_temp_set.delta_uv;
                ESP_LOGI(TAG, "lightness|del_uv:%d|%d",
                        srv->state->temperature,
                        srv->state->delta_uv);
                err = meshx_perform_hw_change(param);
                if(err)
                    return err;
            }
            if(op_code != ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_SET_UNACK)
                send_reply_to_src = true;

            ctl_status_union.ctl_temp_status.temperature = srv->state->temperature;
            ctl_status_union.ctl_temp_status.delta_uv = srv->state->delta_uv;
            ctl_status_pack_len = sizeof(ctl_status_union.ctl_temp_status);

            break;
        /*!< Light CTL Setup Message Opcode */
        case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_DEFAULT_SET:
        case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_DEFAULT_GET:
        case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_DEFAULT_SET_UNACK:
            if(op_code != ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_DEFAULT_GET)
            {
                ESP_LOGI(TAG, "lightness|temp|del_uv:%d|%d|%d",
                        param->value.state_change.ctl_default_set.lightness,
                        param->value.state_change.ctl_default_set.temperature,
                        param->value.state_change.ctl_default_set.delta_uv);
                srv->state->temperature_default = param->value.state_change.ctl_default_set.temperature;
                srv->state->lightness_default = param->value.state_change.ctl_default_set.lightness;
                srv->state->delta_uv_default = param->value.state_change.ctl_default_set.delta_uv;
            }
            if(op_code != ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_DEFAULT_SET_UNACK)
                send_reply_to_src = true;

            ctl_status_union.ctl_default.delta_uv_def = srv->state->delta_uv_default;
            ctl_status_union.ctl_default.lightness_def = srv->state->lightness_default;
            ctl_status_union.ctl_default.temperature_def = srv->state->temperature_default;
            ctl_status_pack_len = sizeof(ctl_status_union.ctl_default);

            status_op = ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_DEFAULT_STATUS;
            break;
        case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_SET:
        case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_GET:
        case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_SET_UNACK:
            if(op_code != ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_GET)
            {
                ESP_LOGI(TAG, "temp min|max: %dK|%dK",
                        param->value.state_change.ctl_temp_range_set.range_min,
                        param->value.state_change.ctl_temp_range_set.range_max);
                srv->state->temperature_range_min = param->value.state_change.ctl_temp_range_set.range_min;
                srv->state->temperature_range_max = param->value.state_change.ctl_temp_range_set.range_max;
            }
            if(op_code != ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_SET_UNACK)
            {
                send_reply_to_src = true;
            }

            ctl_status_union.ctl_temp_range.status_code = MESHX_SUCCESS;
            ctl_status_union.ctl_temp_range.range_min = srv->state->temperature_range_min;
            ctl_status_union.ctl_temp_range.range_max = srv->state->temperature_range_max;
            ctl_status_pack_len = sizeof(ctl_status_union.ctl_temp_range);

            status_op = ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_STATUS;
            break;
        default:
            ESP_LOGW(TAG, "CTL Unhandled Event %p", (void*)param->ctx.recv_op);
            break;
    }
    if (send_reply_to_src
    /* This is meant to notify the respective publish client */
    || param->ctx.addr != param->model->pub->publish_addr)
    {
        /* Here the message was received from unregistered source and mention the state to the respective client */
        ESP_LOGD(TAG, "PUB: src|pub %x|%x", param->ctx.addr, param->model->pub->publish_addr);
        /* ACK to Source */
        err = esp_ble_mesh_server_model_send_msg(param->model,
                                &param->ctx,
                                status_op,
                                ctl_status_pack_len,
                                (uint8_t*)&ctl_status_union);
        if(err)
            ESP_LOGE(TAG, "Failed to send CTL status message (Err: %x)", err);
    }

    return err;
}

/**
 * @brief Send the Light CTL Status message to the client.
 *
 * This function sends the Light CTL Status message to the client with the
 * specified lightness and temperature values.
 *
 * @param model Pointer to the Light CTL Server model.
 * @param ctx Pointer to the BLE Mesh message context.
 * @param lightness Lightness value to send.
 * @param temperature Temperature value to send.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_FAIL: Failure
 */
meshx_err_t meshx_send_ctl_status(esp_ble_mesh_model_t *model, esp_ble_mesh_msg_ctx_t* ctx, uint16_t lightness, uint16_t temperature)
{
    ctl_status_t ctl_status_pack;

    ctl_status_pack.ctl_status.lightness = lightness;
    ctl_status_pack.ctl_status.temperature = temperature;

    return esp_ble_mesh_server_model_send_msg(model, ctx, ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_STATUS, sizeof(ctl_status_pack.ctl_status), (uint8_t*)&ctl_status_pack);
}
/**
 * @brief Initialize the Light CTL Server model.
 *
 * This function initializes the Light CTL (Color Temperature Light) Server model
 * for the BLE Mesh Node. It sets up the necessary configurations and state for
 * the Light CTL Server to operate correctly.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_FAIL: Failure
 */
meshx_err_t meshx_light_ctl_server_init(void)
{
    meshx_err_t err = MESHX_SUCCESS;
#if CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE
    /* Protect only one registration */
    static uint8_t init_cntr = 0;
    if (init_cntr)
        return MESHX_SUCCESS;
    init_cntr++;
#endif /* CONFIG_BLE_CONTROL_TASK_OFFLOAD_ENABLE */

    err = meshx_lighting_srv_init();
    if(err)
        ESP_LOGE(TAG, "Failed to initialize prod server");

    err = meshx_lighting_reg_cb(ESP_BLE_MESH_MODEL_ID_LIGHT_CTL_SRV, (meshx_lighting_server_cb)&meshx_handle_light_ctl_msg);
    if(err)
        ESP_LOGE(TAG, "Failed to initialize meshx_gen_srv_reg_cb (Err: %d)", err);

    err = meshx_lighting_reg_cb(ESP_BLE_MESH_MODEL_ID_LIGHT_CTL_SETUP_SRV, (meshx_lighting_server_cb)&meshx_handle_light_ctl_msg);
    if(err)
        ESP_LOGE(TAG, "Failed to initialize meshx_gen_srv_reg_cb (Err: %d)", err);

    return err;
}
