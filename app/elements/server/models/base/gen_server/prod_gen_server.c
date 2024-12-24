#include <prod_gen_server.h>

#define TAG __func__

#define PROD_SERVER_INIT_MAGIC_NO   0x1121

static const char* server_state_str[] =
{
    [ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT] = "SRV_STATE_CH",
    [ESP_BLE_MESH_GENERIC_SERVER_RECV_GET_MSG_EVT] = "SRV_RECV_GET",
    [ESP_BLE_MESH_GENERIC_SERVER_RECV_SET_MSG_EVT] = "SRV_RECV_SET"
};

static uint16_t prod_server_init = 0;
static uint16_t prod_gen_srv_reg_table_idx = 0;
static prod_server_cb_reg_t prod_gen_srv_reg_table[CONFIG_MAX_PROD_SERVER_CB];

static void prod_ble_mesh_generic_server_cb(esp_ble_mesh_generic_server_cb_event_t event,
                                           esp_ble_mesh_generic_server_cb_param_t *param)
{
    ESP_LOGI(TAG, "%s, op|src|dst:%04" PRIx32 "|%04x|%04x",
             server_state_str[event], param->ctx.recv_op, param->ctx.addr, param->ctx.recv_dst);

    if(event != ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT)
        return;

    for (size_t i = 0; (i < prod_gen_srv_reg_table_idx) && (prod_gen_srv_reg_table_idx < CONFIG_MAX_PROD_SERVER_CB); i++)
    {
        if ((prod_gen_srv_reg_table[i].model_id == param->model->model_id
        || prod_gen_srv_reg_table[i].model_id == param->model->vnd.model_id)
        && prod_gen_srv_reg_table[i].cb)
        {
            /* Despacth callback to generic model type */
            prod_gen_srv_reg_table[i].cb(param);
        }
    }
}

esp_err_t prod_gen_srv_reg_cb(uint32_t model_id, prod_server_cb cb)
{
    if(prod_gen_srv_reg_table_idx >= CONFIG_MAX_PROD_SERVER_CB)
    {
        ESP_LOGE(TAG, "No Memory left in prod_gen_srv_reg_table");
        return ESP_ERR_NO_MEM;
    }

    for (size_t i = 0; i < prod_gen_srv_reg_table_idx; i++)
    {
        if (prod_gen_srv_reg_table[i].model_id == model_id)
        {
            /* If already registered over-write */
            prod_gen_srv_reg_table[i].cb = cb;
            return ESP_OK;
        }
    }

    prod_gen_srv_reg_table[prod_gen_srv_reg_table_idx].cb = cb;
    prod_gen_srv_reg_table[prod_gen_srv_reg_table_idx++].model_id = model_id;

    return ESP_OK;
}

esp_err_t prod_gen_srv_init(void)
{
    if(prod_server_init == PROD_SERVER_INIT_MAGIC_NO)
        return ESP_OK;
    prod_server_init = PROD_SERVER_INIT_MAGIC_NO;
    return esp_ble_mesh_register_generic_server_callback(prod_ble_mesh_generic_server_cb);
}

