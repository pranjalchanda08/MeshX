/**
 * @file esp_sensor_srv_model.c
 * @brief Implementation of the BLE Mesh Sensor Server Model for ESP32.
 *
 * @author Pranjal Chanda
 * @date 2026
 * @copyright Copyright 2026 - 2025 MeshX
 */

#include "meshx_platform_ble_mesh.h"
#include "interface/ble_mesh/server/meshx_ble_mesh_sensor_srv.h"

static const MESHX_MODEL sensor_srv_sig_template = ESP_BLE_MESH_SIG_MODEL(MESHX_MODEL_ID_SENSOR_SRV, NULL, NULL, NULL);

/**
 * @brief Delete a Sensor Server instance.
 */
meshx_err_t meshx_plat_sensor_srv_delete(meshx_ptr_t *p_pub)
{
    return MESHX_SUCCESS;
}

/**
 * @brief String representation of the server state change events.
 */
static const char *server_state_str[] = {
    [ESP_BLE_MESH_SENSOR_SERVER_RECV_GET_MSG_EVT] = "SRV_RECV_GET",
    [ESP_BLE_MESH_SENSOR_SERVER_RECV_SET_MSG_EVT] = "SRV_RECV_SET"
};

/**
 * @brief Callback function for BLE Mesh Sensor Server events.
 *
 * @param event The event type received by the Sensor Server.
 * @param param Pointer to the structure containing event-specific parameters.
 */
static void meshx_ble_sensor_server_cb(MESHX_SENSOR_SRV_CB_EVT event,
                                           MESHX_SENSOR_SRV_CB_PARAM *param)
{
    ESP_UNUSED(server_state_str);
    MESHX_LOGD(MODULE_ID_MODEL_SERVER, "%s, op|src|dst:%04" PRIx32 "|%04x|%04x",
            server_state_str[event], param->ctx.recv_op, param->ctx.addr, param->ctx.recv_dst);

    meshx_sensor_server_cb_param_t pub_param = {
        .ctx = {
            .net_idx    = param->ctx.net_idx,
            .app_idx    = param->ctx.app_idx,
            .dst_addr   = param->ctx.recv_dst,
            .src_addr   = param->ctx.addr,
            .opcode     = param->ctx.recv_op,
            .p_ctx      = &param->ctx
        },
        .model = {
            .pub_addr   = param->model->pub->publish_addr,
            .model_id   = param->model->model_id,
            .el_id      = param->model->element_idx,
            .p_model    = param->model
        }
    };

    if (event == ESP_BLE_MESH_SENSOR_SERVER_RECV_GET_MSG_EVT) {
        pub_param.sensor_status.property_id = param->value.get.sensor_data.property_id;
        pub_param.sensor_status.data_len = 0;
    }
    else if (event == ESP_BLE_MESH_SENSOR_SERVER_RECV_SET_MSG_EVT) {
        // Handle set if needed
    }

    control_task_msg_publish(CONTROL_TASK_MSG_CODE_FRM_BLE,
                             pub_param.model.model_id,
                             &pub_param,
                             sizeof(meshx_sensor_server_cb_param_t));
}

meshx_err_t meshx_plat_sensor_srv_init(void)
{
    esp_err_t esp_err = esp_ble_mesh_register_sensor_server_callback(
        (MESHX_SENSOR_SRV_CB)meshx_ble_sensor_server_cb
    );
    if (esp_err != ESP_OK) {
        return MESHX_ERR_PLAT;
    }
    return MESHX_SUCCESS;
}

meshx_err_t meshx_plat_sensor_srv_create(meshx_ptr_t p_model, meshx_ptr_t *p_pub, meshx_ptr_t *p_sensor_srv)
{
    if (!p_model || !p_pub || !p_sensor_srv) return MESHX_INVALID_ARG;

    meshx_err_t err = meshx_plat_create_model_pub(p_pub, 1);
    if (err) return err;

    MESHX_SENSOR_SRV *srv = (MESHX_SENSOR_SRV *)MESHX_CALOC(1, sizeof(MESHX_SENSOR_SRV));
    if (!srv) {
        meshx_plat_del_model_pub(p_pub);
        return MESHX_NO_MEM;
    }

    // Default config
    srv->rsp_ctrl.get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;
    srv->rsp_ctrl.set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP;

    MESHX_MODEL *plat_model = (MESHX_MODEL *)((meshx_model_t*)p_model)->p_model;
    memcpy(plat_model, &sensor_srv_sig_template, sizeof(MESHX_MODEL));
    plat_model->user_data = srv;
    
    void **temp_pub = (void**) &plat_model->pub;
    *temp_pub = *p_pub;

    *p_sensor_srv = (meshx_ptr_t)srv;

    return MESHX_SUCCESS;
}

meshx_err_t meshx_plat_sensor_srv_send_status(const meshx_model_t *p_model, const meshx_ctx_t *p_ctx, const meshx_sensor_server_state_change_t *state_change)
{
    if (!p_model || !p_ctx || !state_change) return MESHX_INVALID_ARG;

    esp_ble_mesh_msg_ctx_t ctx = {0};
    const esp_ble_mesh_msg_ctx_t *pctx = (esp_ble_mesh_msg_ctx_t *)p_ctx->p_ctx;
    if (pctx) {
        memcpy(&ctx, pctx, sizeof(esp_ble_mesh_msg_ctx_t));
    }

    ctx.net_idx    = p_ctx->net_idx;
    ctx.app_idx    = p_ctx->app_idx;
    ctx.addr       = p_ctx->dst_addr;
    ctx.send_ttl   = ESP_BLE_MESH_TTL_DEFAULT;
    ctx.send_tag   = BIT1;

    uint8_t status_data[MESHX_BLE_MESH_SENSOR_DATA_MAX_LEN + 3];
    uint16_t status_len = 0;

    uint16_t prop_id = state_change->sensor_status.property_id;
    uint16_t data_len = state_change->sensor_status.data_len;

    if (data_len > 0 && data_len <= MESHX_BLE_MESH_SENSOR_DATA_MAX_LEN) {
        status_data[0] = (uint8_t)(((data_len - 1) << 1) | 0x01);
        status_data[1] = (uint8_t)(prop_id & 0xFF);
        status_data[2] = (uint8_t)(prop_id >> 8);
        memcpy(&status_data[3], state_change->sensor_status.data, data_len);
        status_len = 3 + data_len;
    } else {
        // Only property ID
        status_data[0] = (uint8_t)(prop_id & 0xFF);
        status_data[1] = (uint8_t)(prop_id >> 8);
        status_len = 2;
    }

    esp_err_t esp_err = esp_ble_mesh_server_model_send_msg((MESHX_MODEL *)p_model->p_model,
                                                       &ctx,
                                                       MESHX_MODEL_OP_SENSOR_STATUS,
                                                       status_len,
                                                       status_data);
    if (esp_err) {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to send sensor status: %d", esp_err);
        return MESHX_ERR_PLAT;
    }

    return MESHX_SUCCESS;
}
