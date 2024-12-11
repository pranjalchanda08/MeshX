#include <prod_light_server.h>
#include <prod_gen_server.h>

#define TAG __func__

#define PROD_SERVER_INIT_MAGIC_NO   0x2483

static const char* server_state_str[] = 
{
    [ESP_BLE_MESH_LIGHTING_SERVER_STATE_CHANGE_EVT] = "SERVER_STATE_CHANGE_EVT",
    [ESP_BLE_MESH_LIGHTING_SERVER_RECV_GET_MSG_EVT] = "SERVER_RECV_GET_MSG_EVT",
    [ESP_BLE_MESH_LIGHTING_SERVER_RECV_SET_MSG_EVT] = "SERVER_RECV_SET_MSG_EVT",
    [ESP_BLE_MESH_LIGHTING_SERVER_RECV_STATUS_MSG_EVT] = "SERVER_RECV_STATUS_MSG_EVT",
};

static uint16_t prod_lighting_server_init = 0;
static uint16_t prod_lighting_server_cb_reg_table_idx = 0;
static prod_lighting_server_cb_reg_t prod_lighting_server_cb_reg_table[CONFIG_MAX_PROD_LIGHTING_SRV_CB];

static void prod_ble_lightness_server_cb(esp_ble_mesh_lighting_server_cb_event_t event,
                                         esp_ble_mesh_lighting_server_cb_param_t *param)
{
    ESP_LOGI(TAG, "event 0x%02x, opcode 0x%04" PRIx32 ", src 0x%04x, dst 0x%04x",
             event, param->ctx.recv_op, param->ctx.addr, param->ctx.recv_dst);

    ESP_LOGI(TAG, "%s", server_state_str[event]);
    for (size_t i = 0; i < prod_lighting_server_cb_reg_table_idx; i++)
    {
        if ((prod_lighting_server_cb_reg_table[i].model_id == param->model->model_id 
        || prod_lighting_server_cb_reg_table[i].model_id == param->model->vnd.model_id)
        && prod_lighting_server_cb_reg_table[i].cb)
        {
            /* Despacth callback to lightng model type */
            prod_lighting_server_cb_reg_table[i].cb(param);
        }
    }
}

esp_err_t prod_lighting_reg_cb(uint32_t model_id, prod_lighting_server_cb cb)
{
    if(prod_lighting_server_cb_reg_table_idx >= CONFIG_MAX_PROD_LIGHTING_SRV_CB)
    {
        ESP_LOGE(TAG, "No Memory left in prod_lighting_server_cb_reg_table");
        return ESP_ERR_NO_MEM;
    }

    for (size_t i = 0; i < prod_lighting_server_cb_reg_table_idx; i++)
    {
        if (prod_lighting_server_cb_reg_table[i].model_id == model_id)
        {
            /* If already registered over-write */
            prod_lighting_server_cb_reg_table[i].cb = cb;
            return ESP_OK;
        }
    }
    
    prod_lighting_server_cb_reg_table[prod_lighting_server_cb_reg_table_idx].cb = cb;
    prod_lighting_server_cb_reg_table[prod_lighting_server_cb_reg_table_idx++].model_id = model_id;

    return ESP_OK;
}

esp_err_t prod_lighting_srv_init(void)
{
    if(prod_lighting_server_init == PROD_SERVER_INIT_MAGIC_NO)
        return ESP_OK;
    prod_lighting_server_init = PROD_SERVER_INIT_MAGIC_NO;
    esp_err_t err = prod_gen_srv_init();
    
    if (err)
        return err;
    
    return esp_ble_mesh_register_lighting_server_callback(prod_ble_lightness_server_cb);
}

