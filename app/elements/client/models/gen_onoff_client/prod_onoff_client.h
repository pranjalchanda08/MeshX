#pragma once

#include <esp_ble_mesh_defs.h>
#include <esp_ble_mesh_common_api.h>
#include <esp_ble_mesh_networking_api.h>
#include <esp_ble_mesh_generic_model_api.h>
#include <esp_ble_mesh_local_data_operation_api.h>

#ifndef CONFIG_MAX_PROD_ONOFF_CLI_CB
#define CONFIG_MAX_PROD_ONOFF_CLI_CB  5
#endif

typedef struct prod_onoff_ctx
{
    uint8_t state;
    uint8_t tid;
    uint16_t pub_addr;
    uint16_t net_id;
    uint16_t app_id;
}prod_onoff_ctx_t;

esp_err_t prod_onoff_client_init();
esp_err_t prod_onoff_reg_cb(esp_ble_mesh_generic_client_cb_t callback);
