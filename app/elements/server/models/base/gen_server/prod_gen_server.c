#include <prod_gen_server.h>

#define TAG "P_GEN_SRV"

#define PROD_SERVER_INIT_MAGIC_NO   0x1121

static const char* server_state_str[] = 
{
    [ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT] = "SERVER_STATE_CHANGE_EVT",
    [ESP_BLE_MESH_GENERIC_SERVER_RECV_GET_MSG_EVT] = "SERVER_RECV_GET_MSG_EVT",
    [ESP_BLE_MESH_GENERIC_SERVER_RECV_SET_MSG_EVT] = "SERVER_RECV_SET_MSG_EVT"
};

static uint16_t prod_server_init = 0;
static uint16_t prod_server_cb_reg_table_idx = 0;
static prod_server_cb_reg_t prod_server_cb_reg_table[CONFIG_MAX_PROD_SERVER_CB];

static void prod_ble_mesh_generic_server_cb(esp_ble_mesh_generic_server_cb_event_t event,
                                           esp_ble_mesh_generic_server_cb_param_t *param)
{
    ESP_LOGI(TAG, "event 0x%02x, opcode 0x%04" PRIx32 ", src 0x%04x, dst 0x%04x",
             event, param->ctx.recv_op, param->ctx.addr, param->ctx.recv_dst);

    ESP_LOGI(TAG, "%s", server_state_str[event]);
    for (size_t i = 0; i < prod_server_cb_reg_table_idx; i++)
    {
        if ((prod_server_cb_reg_table[i].model_id == param->model->model_id 
        || prod_server_cb_reg_table[i].model_id == param->model->vnd.model_id)
        && prod_server_cb_reg_table[i].cb)
        {
            /* Despacth callback to generic model type */
            prod_server_cb_reg_table[i].cb(param);
        }
    }
}

esp_err_t prod_srv_reg_cb(uint32_t model_id, prod_server_cb cb)
{
    if(prod_server_cb_reg_table_idx > CONFIG_MAX_PROD_SERVER_CB)
    {
        ESP_LOGE(TAG, "No Memory left in prod_server_cb_reg_table");
        return ESP_ERR_NO_MEM;
    }

    for (size_t i = 0; i < prod_server_cb_reg_table_idx; i++)
    {
        if (prod_server_cb_reg_table[i].model_id == model_id)
        {
            /* If already registered over-write */
            prod_server_cb_reg_table[i].cb = cb;
            return ESP_OK;
        }
    }
    
    prod_server_cb_reg_table[prod_server_cb_reg_table_idx].cb = cb;
    prod_server_cb_reg_table[prod_server_cb_reg_table_idx++].model_id = model_id;

    return ESP_OK;
}

esp_err_t prod_gen_srv_init(void)
{
    if(prod_server_init == PROD_SERVER_INIT_MAGIC_NO)
        return ESP_OK;
    prod_server_init = PROD_SERVER_INIT_MAGIC_NO;
    return esp_ble_mesh_register_generic_server_callback(prod_ble_mesh_generic_server_cb);
}

