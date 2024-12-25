#ifndef __PROD_CONFIG_SERVER__
#define __PROD_CONFIG_SERVER__

#include <esp_ble_mesh_defs.h>
#include <esp_ble_mesh_common_api.h>
#include <esp_ble_mesh_networking_api.h>
#include <esp_ble_mesh_generic_model_api.h>
#include <esp_ble_mesh_local_data_operation_api.h>
#include <esp_ble_mesh_config_model_api.h>

#define PROD_CONFIG_SERVER_INSTANCE g_prod_config_server

typedef enum
{
    CONFIG_EVT_MODEL_APP_KEY_ADD        = BIT0,
    CONFIG_EVT_MODEL_APP_KEY_DEL        = BIT1,
    CONFIG_EVT_MODEL_APP_KEY_BIND       = BIT2,
    CONFIG_EVT_MODEL_APP_KEY_UNBIND     = BIT3,
    CONFIG_EVT_MODEL_SUB_ADD            = BIT4,
    CONFIG_EVT_MODEL_SUB_DEL            = BIT5,
    CONFIG_EVT_MODEL_PUB_ADD            = BIT6,
    CONFIG_EVT_MODEL_PUB_DEL            = BIT7,
    CONFIG_EVT_MODEL_NET_KEY_ADD        = BIT8,
    CONFIG_EVT_MODEL_NET_KEY_DEL        = BIT9,
    CONFIG_EVT_ALL                      = 0xFFFFFFFF,
}config_evt_t;

typedef void (*config_srv_cb)(const esp_ble_mesh_cfg_server_cb_param_t *param, config_evt_t evt);

extern esp_ble_mesh_cfg_srv_t g_prod_config_server;

esp_err_t prod_init_config_server();
esp_err_t prod_config_server_cb_reg(config_srv_cb cb, uint32_t config_evt_bmap);

#endif
