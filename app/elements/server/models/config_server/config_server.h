#ifndef __PROD_CONFIG_SERVER__
#define __PROD_CONFIG_SERVER__

#include <esp_ble_mesh_defs.h>
#include <esp_ble_mesh_common_api.h>
#include <esp_ble_mesh_networking_api.h>
#include <esp_ble_mesh_generic_model_api.h>
#include <esp_ble_mesh_local_data_operation_api.h>
#include <esp_ble_mesh_config_model_api.h>

#define PROD_CONFIG_SERVER_INSTANCE g_prod_config_server

typedef void (*config_srv_cb)(esp_ble_mesh_cfg_server_cb_param_t *param);

typedef struct config_server_params
{
    config_srv_cb on_app_key_cb;
}config_server_params_t;

extern esp_ble_mesh_cfg_srv_t g_prod_config_server;

esp_err_t prod_init_config_server(config_server_params_t * svr_cfg);

#endif
