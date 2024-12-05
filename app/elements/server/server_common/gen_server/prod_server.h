#include <esp_ble_mesh_defs.h>
#include <esp_ble_mesh_common_api.h>
#include <esp_ble_mesh_networking_api.h>
#include <esp_ble_mesh_generic_model_api.h>
#include <esp_ble_mesh_local_data_operation_api.h>

#ifndef CONFIG_MAX_PROD_SERVER_CB
#define CONFIG_MAX_PROD_SERVER_CB   10
#endif

#define ESP_BLE_MESH_ADDR_BROADCAST(_x) _x == ESP_BLE_MESH_ADDR_ALL_NODES

typedef esp_err_t (* prod_server_cb) (esp_ble_mesh_generic_server_cb_param_t *param);

typedef struct prod_server_cb_reg
{
    uint32_t model_id;
    prod_server_cb cb;
}prod_server_cb_reg_t;

esp_err_t prod_srv_reg_cb(uint32_t model_id, prod_server_cb cb);
esp_err_t prod_srv_init(void);
