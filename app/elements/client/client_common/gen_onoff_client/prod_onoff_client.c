#include "prod_onoff_client.h"

#define TAG "ONOFF_CLI"

static void app_ble_mesh_generic_client_cb( esp_ble_mesh_generic_client_cb_event_t event,
                                            esp_ble_mesh_generic_client_cb_param_t *param)
{
    ESP_LOGI(TAG, "event 0x%02x, opcode 0x%04" PRIx32 ", src 0x%04x, dst 0x%04x",
             event, param->params->ctx.recv_op, param->params->ctx.addr, param->params->ctx.recv_dst);

    switch(event)
    {
        case ESP_BLE_MESH_GENERIC_CLIENT_GET_STATE_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_GENERIC_CLIENT_GET_STATE_EVT");
            break;
        case ESP_BLE_MESH_GENERIC_CLIENT_SET_STATE_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_GENERIC_CLIENT_SET_STATE_EVT");
            break;
        case ESP_BLE_MESH_GENERIC_CLIENT_PUBLISH_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_GENERIC_CLIENT_PUBLISH_EVT");
            break;
        case ESP_BLE_MESH_GENERIC_CLIENT_TIMEOUT_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_GENERIC_CLIENT_TIMEOUT_EVT");
            break;
        default:
            break;
    }
}

esp_err_t prod_client_init()
{
    return esp_ble_mesh_register_generic_client_callback(app_ble_mesh_generic_client_cb);
}
