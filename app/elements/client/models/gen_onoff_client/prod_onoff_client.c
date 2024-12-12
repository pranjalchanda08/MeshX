#include "prod_onoff_client.h"

#define TAG __func__

#define PROD_CLIENT_INIT_MAGIC  0x2378

static uint16_t prod_client_init_flag = 0;

static const char* client_state_str [] = 
{
    [ESP_BLE_MESH_GENERIC_CLIENT_GET_STATE_EVT] = "GET_STATE_EVT",
    [ESP_BLE_MESH_GENERIC_CLIENT_SET_STATE_EVT] = "SET_STATE_EVT",
    [ESP_BLE_MESH_GENERIC_CLIENT_PUBLISH_EVT] = "PUBLISH_EVT",
    [ESP_BLE_MESH_GENERIC_CLIENT_TIMEOUT_EVT] = "TIMEOUT_EVT",
};

esp_ble_mesh_generic_client_cb_t prod_onoff_cb_reg[CONFIG_MAX_PROD_ONOFF_CLI_CB];
uint16_t prod_onoff_cb_reg_idx;

static void app_ble_mesh_generic_client_cb( esp_ble_mesh_generic_client_cb_event_t event,
                                            esp_ble_mesh_generic_client_cb_param_t *param)
{
    ESP_LOGI(TAG, "event 0x%02x, opcode 0x%04" PRIx32 ", src 0x%04x, dst 0x%04x",
             event, param->params->ctx.recv_op, param->params->ctx.addr, param->params->ctx.recv_dst);
    ESP_LOGI(TAG, "%s", client_state_str[event]);
    
    if(prod_onoff_cb_reg_idx >= CONFIG_MAX_PROD_ONOFF_CLI_CB)
    {
        return;
    }

    for (size_t i = 0; i < prod_onoff_cb_reg_idx; i++)
    {
        if(prod_onoff_cb_reg[i] != NULL)
            prod_onoff_cb_reg[i](event, param);
    }
}

esp_err_t prod_onoff_reg_cb(esp_ble_mesh_generic_client_cb_t callback)
{
    if(prod_onoff_cb_reg_idx >= CONFIG_MAX_PROD_ONOFF_CLI_CB)
    {
        return ESP_ERR_NO_MEM;
    }
    prod_onoff_cb_reg[prod_onoff_cb_reg_idx++] = callback;

    return ESP_OK;
}

esp_err_t prod_onoff_client_init()
{
    return (prod_client_init_flag == PROD_CLIENT_INIT_MAGIC) ?  ESP_OK : 
        esp_ble_mesh_register_generic_client_callback(app_ble_mesh_generic_client_cb);
}
