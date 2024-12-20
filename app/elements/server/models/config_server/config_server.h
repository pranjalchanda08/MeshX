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
    CONFIG_EVT_MODEL_APP_KEY_ADD        = 0x01,
    CONFIG_EVT_MODEL_APP_KEY_DEL        = 0x02,
    CONFIG_EVT_MODEL_APP_KEY_BIND       = 0x04,
    CONFIG_EVT_MODEL_APP_KEY_UNBIND     = 0x08,
    CONFIG_EVT_MODEL_SUB_ADD            = 0x10,
    CONFIG_EVT_MODEL_SUB_DEL            = 0x20,
    CONFIG_EVT_MODEL_PUB_ADD            = 0x40,
    CONFIG_EVT_MODEL_PUB_DEL            = 0x80,
    CONFIG_EVT_MODEL_NET_KEY_ADD        = 0x100,
    CONFIG_EVT_MODEL_NET_KEY_DEL        = 0x200,
    CONFIG_EVT_ALL                      = 0xFFFF
}config_evt_t;

typedef void (*config_srv_cb)(const esp_ble_mesh_cfg_server_cb_param_t *param, config_evt_t evt);

extern esp_ble_mesh_cfg_srv_t g_prod_config_server;

esp_err_t prod_init_config_server();
esp_err_t prod_config_server_cb_reg(config_srv_cb cb, uint32_t config_evt_bmap);

#endif
