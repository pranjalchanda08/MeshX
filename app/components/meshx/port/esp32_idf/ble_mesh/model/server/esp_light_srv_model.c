/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_light_server.c
 * @brief Implementation of the BLE Mesh Lighting Server for the ESP32.
 *
 * This file contains the implementation of the BLE Mesh Lighting Server,
 * including initialization, event handling, and callback registration.
 *
 * @author Pranjal Chanda
 */

#include "meshx_platform_ble_mesh.h"
#include "meshx_light_server.h"
#include "meshx_gen_server.h"

#define TAG __func__

/**
 * @brief Light CTL status packet.
 */
typedef union ctl_status_pack
{
    struct
    {
        uint16_t lightness;   /**< Lightness level */
        uint16_t temperature; /**< Color temperature */
    } ctl_status;
    struct
    {
        uint16_t temperature; /**< Color temperature */
        uint16_t delta_uv;    /**< Delta UV value */
    } ctl_temp_status;
    struct
    {
        uint16_t lightness_def;   /**< Default lightness */
        uint16_t temperature_def; /**< Default temperature */
        uint16_t delta_uv_def;    /**< Default delta UV */
    } ctl_default;
    struct
    {
        uint8_t status_code; /**< Status code */
        uint16_t range_min;  /**< Minimum temperature range */
        uint16_t range_max;  /**< Maximum temperature range */
    } ctl_temp_range;
} ctl_status_t;

/**
 * @brief Template for SIG model initialization.
 */
static const MESHX_MODEL light_ctl_sig_template = ESP_BLE_MESH_SIG_MODEL(
    ESP_BLE_MESH_MODEL_ID_LIGHT_CTL_SRV, NULL, NULL, NULL);

/**
 * @brief Handles the BLE message sending for the Generic OnOff Server model.
 *
 * This function processes the event to send a BLE Mesh message for the
 * Generic OnOff Server model. It checks if the event type is
 * CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL_SRV and sends the OnOff status
 * message using the provided parameters.
 *
 * @param[in] pdev Pointer to the device structure.
 * @param[in] evt Event type for the control task message to BLE.
 * @param[in] params Parameters for the BLE Mesh Generic Server model.
 *
 * @return
 *     - MESHX_SUCCESS: Message sent successfully or event type not matched.
 *     - MESHX_FAIL: Failed to send the message.
 */
static meshx_err_t ble_send_msg_handle_t(
    const dev_struct_t *pdev,
    control_task_msg_evt_to_ble_t evt,
    meshx_lighting_server_cb_param_t *params)
{
    if (evt != CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL_SRV)
        return MESHX_SUCCESS;

    esp_ble_mesh_msg_ctx_t *ctx = params->ctx.p_ctx;
    ctx->addr = params->ctx.dst_addr;

    ctl_status_t ctl_status_union;
    uint8_t ctl_status_pack_len = 0;

    switch (params->ctx.opcode)
    {
    case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_STATUS:
        ctl_status_union.ctl_status.temperature = params->state_change.ctl_set.temperature;
        ctl_status_union.ctl_status.lightness = params->state_change.ctl_set.lightness;
        ctl_status_pack_len = sizeof(ctl_status_union.ctl_status);
        break;
    case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_STATUS:
        ctl_status_union.ctl_temp_status.temperature = params->state_change.ctl_temp_set.temperature;
        ctl_status_union.ctl_temp_status.delta_uv = params->state_change.ctl_temp_set.delta_uv;
        ctl_status_pack_len = sizeof(ctl_status_union.ctl_temp_status);
        break;
    case MESHX_MODEL_OP_LIGHT_CTL_DEFAULT_STATUS:
        ctl_status_union.ctl_default.delta_uv_def = params->state_change.ctl_default_set.delta_uv;
        ctl_status_union.ctl_default.lightness_def = params->state_change.ctl_default_set.lightness;
        ctl_status_union.ctl_default.temperature_def = params->state_change.ctl_default_set.temperature;
        ctl_status_pack_len = sizeof(ctl_status_union.ctl_default);
        break;
    case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_STATUS:
        ctl_status_union.ctl_temp_range.status_code = MESHX_SUCCESS;
        ctl_status_union.ctl_temp_range.range_min = params->state_change.ctl_temp_range_set.range_min;
        ctl_status_union.ctl_temp_range.range_max = params->state_change.ctl_temp_range_set.range_max;
        ctl_status_pack_len = sizeof(ctl_status_union.ctl_temp_range);
        break;
    default:
        return MESHX_INVALID_ARG;
    }

    esp_err_t err = esp_ble_mesh_server_model_send_msg(params->model.p_model,
                                                       ctx,
                                                       params->model.model_id,
                                                       ctl_status_pack_len,
                                                       (uint8_t *)&ctl_status_union);
    if (err)
    {
        ESP_LOGE(TAG, "Mesh Model msg send failed (err: 0x%x)", err);
        return MESHX_FAIL;
    }

    ESP_UNUSED(pdev);
    return MESHX_SUCCESS;
}

/**
 * @brief Callback function for BLE Mesh Lightness Server events.
 *
 * This function is called whenever a BLE Mesh Lightness Server event occurs.
 *
 * @param[in] event The event type for the BLE Mesh Lightness Server.
 * @param[in] param Parameters associated with the event.
 */
static void meshx_ble_lightness_server_cb(esp_ble_mesh_lighting_server_cb_event_t event,
                                          esp_ble_mesh_lighting_server_cb_param_t *param)
{
    ESP_LOGD(TAG, "evt|op|src|dst: %02x|%04x|%04x|%04x|%04x",
             event, (unsigned)param->ctx.recv_op, param->ctx.addr, param->ctx.recv_dst,
             param->model->model_id);

    MESHX_LIGHT_CTL_SRV *srv = (MESHX_LIGHT_CTL_SRV *)param->model->user_data;

    bool publish_flag = false;
    uint32_t op_code = param->ctx.recv_op;
    meshx_lighting_server_cb_param_t pub_param = {
        .ctx = {
            .net_idx = param->ctx.net_idx,
            .app_idx = param->ctx.app_idx,
            .dst_addr = param->ctx.recv_dst,
            .src_addr = param->ctx.addr,
            .opcode = param->ctx.recv_op,
            .p_ctx = &param->ctx},
        .model = {.pub_addr = param->model->pub->publish_addr, .model_id = param->model->model_id, .el_id = param->model->element_idx, .p_model = param->model}};

    switch (op_code)
    {
    /*!< Light CTL Message Opcode */
    case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_GET:
    case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_SET:
    case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_SET_UNACK:
        if (op_code != ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_GET)
        {
            srv->state->temperature = param->value.state_change.ctl_set.temperature;
            srv->state->lightness = param->value.state_change.ctl_set.lightness;
            srv->state->delta_uv = param->value.state_change.ctl_set.delta_uv;
            ESP_LOGD(TAG, "lightness|temp|del_uv:%d|%d|%d",
                     srv->state->lightness,
                     srv->state->temperature,
                     srv->state->delta_uv);

            pub_param.state_change.ctl_set.delta_uv = srv->state->delta_uv;
            pub_param.state_change.ctl_set.lightness = srv->state->lightness;
            pub_param.state_change.ctl_set.temperature = srv->state->temperature;

            publish_flag = true;
        }
        break;
    case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_GET:
    case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_SET:
    case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_SET_UNACK:
        if (op_code != ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_GET)
        {
            srv->state->temperature = param->value.state_change.ctl_temp_set.temperature;
            srv->state->delta_uv = param->value.state_change.ctl_temp_set.delta_uv;
            ESP_LOGI(TAG, "lightness|del_uv:%d|%d",
                     srv->state->temperature,
                     srv->state->delta_uv);

            pub_param.state_change.ctl_temp_set.delta_uv = srv->state->delta_uv;
            pub_param.state_change.ctl_temp_set.temperature = srv->state->temperature;

            publish_flag = true;
        }
        break;
    /*!< Light CTL Setup Message Opcode */
    case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_DEFAULT_SET:
    case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_DEFAULT_GET:
    case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_DEFAULT_SET_UNACK:
        if (op_code != ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_DEFAULT_GET)
        {
            ESP_LOGI(TAG, "lightness|temp|del_uv:%d|%d|%d",
                     param->value.state_change.ctl_default_set.lightness,
                     param->value.state_change.ctl_default_set.temperature,
                     param->value.state_change.ctl_default_set.delta_uv);
            srv->state->temperature_default = param->value.state_change.ctl_default_set.temperature;
            srv->state->lightness_default = param->value.state_change.ctl_default_set.lightness;
            srv->state->delta_uv_default = param->value.state_change.ctl_default_set.delta_uv;

            pub_param.state_change.ctl_default_set.delta_uv = srv->state->delta_uv_default;
            pub_param.state_change.ctl_default_set.lightness = srv->state->lightness_default;
            pub_param.state_change.ctl_default_set.temperature = srv->state->temperature_default;

            publish_flag = true;
        }
        break;
    case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_SET:
    case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_GET:
    case ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_SET_UNACK:
        if (op_code != ESP_BLE_MESH_MODEL_OP_LIGHT_CTL_TEMPERATURE_RANGE_GET)
        {
            ESP_LOGI(TAG, "temp min|max: %dK|%dK",
                     param->value.state_change.ctl_temp_range_set.range_min,
                     param->value.state_change.ctl_temp_range_set.range_max);
            srv->state->temperature_range_min = param->value.state_change.ctl_temp_range_set.range_min;
            srv->state->temperature_range_max = param->value.state_change.ctl_temp_range_set.range_max;

            pub_param.state_change.ctl_temp_range_set.range_max = srv->state->temperature_range_max;
            pub_param.state_change.ctl_temp_range_set.range_min = srv->state->temperature_range_min;

            publish_flag = true;
        }
        break;
    default:
        ESP_LOGW(TAG, "CTL Unhandled Event %p", (void *)param->ctx.recv_op);
        break;
    }
    if (publish_flag)
        control_task_msg_publish(CONTROL_TASK_MSG_CODE_FRM_BLE,
                                 pub_param.model.model_id,
                                 &pub_param,
                                 sizeof(meshx_lighting_server_cb_param_t));
}

meshx_err_t meshx_plat_light_srv_init(void)
{

    meshx_err_t err = control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_TO_BLE,
        CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL_SRV,
        (control_task_msg_handle_t)&ble_send_msg_handle_t);
    if (err)
        return err;

    esp_err_t esp_err = esp_ble_mesh_register_lighting_server_callback((esp_ble_mesh_lighting_server_cb_t)&meshx_ble_lightness_server_cb);
    if (esp_err != ESP_OK)
        err = MESHX_FAIL;

    return err;
}

meshx_err_t meshx_plat_light_ctl_srv_create(void **p_model, void **p_pub, void **p_ctl_srv)
{
    if (!p_model || !p_pub || !p_ctl_srv)
        return MESHX_INVALID_ARG;

    meshx_err_t err = MESHX_SUCCESS;

    err = meshx_plat_create_model_pub(p_model, p_pub, 1);
    if (err)
        return meshx_plat_del_model_pub(p_model, p_pub);

    *p_ctl_srv = (MESHX_GEN_ONOFF_SRV *)MESHX_CALOC(1, sizeof(MESHX_GEN_ONOFF_SRV));
    if (!*p_ctl_srv)
        return MESHX_NO_MEM;

    /* SIG ON OFF initialisation */

    memcpy(*p_model, &light_ctl_sig_template, sizeof(MESHX_MODEL));

    ((MESHX_GEN_ONOFF_SRV *)*p_ctl_srv)->rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;
    ((MESHX_GEN_ONOFF_SRV *)*p_ctl_srv)->rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;

    void **temp = (void **)&((MESHX_MODEL *)*p_model)->pub;

    *temp = *p_pub;

    return err;
}

meshx_err_t meshx_plat_light_ctl_srv_delete(void **p_model, void **p_pub, void **p_ctl_srv)
{
    if (p_ctl_srv)
    {
        MESHX_FREE(*p_ctl_srv);
        *p_ctl_srv = NULL;
    }

    return meshx_plat_del_model_pub(p_model, p_pub);
}

meshx_err_t meshx_plat_set_light_ctl_srv_state(void *p_model,
                                               uint16_t delta_uv,
                                               uint16_t lightness,
                                               uint16_t temperature,
                                               uint16_t temp_range_max,
                                               uint16_t temp_range_min
                                            )
{
    if (!p_model)
        return MESHX_INVALID_ARG;

    MESHX_MODEL *model = (MESHX_MODEL *)p_model;
    MESHX_LIGHT_CTL_SRV *srv = (MESHX_LIGHT_CTL_SRV *)model->user_data;
    srv->state->delta_uv = delta_uv;
    srv->state->lightness = lightness;
    srv->state->temperature = temperature;
    srv->state->temperature_range_min = temp_range_min;
    srv->state->temperature_range_max = temp_range_max;

    return MESHX_SUCCESS;
}
meshx_err_t meshx_plat_light_ctl_srv_restore(void *p_model,
                                             uint16_t delta_uv,
                                             uint16_t lightness,
                                             uint16_t temperature,
                                             uint16_t temp_range_max,
                                             uint16_t temp_range_min)
{
    return meshx_plat_set_light_ctl_srv_state(
        p_model,
        delta_uv,
        lightness,
        temperature,
        temp_range_max,
        temp_range_min
    );
}
