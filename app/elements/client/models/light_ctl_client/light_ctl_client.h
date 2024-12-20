#ifndef __LIGHT_CTL_CLIENT_H__
#define __LIGHT_CTL_CLIENT_H__

#include <esp_ble_mesh_defs.h>
#include <esp_ble_mesh_common_api.h>
#include <esp_ble_mesh_networking_api.h>
#include <esp_ble_mesh_lighting_model_api.h>
#include <esp_ble_mesh_local_data_operation_api.h>

typedef enum
{
    LIGHT_CTL_CLI_EVT_GET = (1 << ESP_BLE_MESH_LIGHT_CLIENT_GET_STATE_EVT),
    LIGHT_CTL_CLI_EVT_SET = (1 << ESP_BLE_MESH_LIGHT_CLIENT_SET_STATE_EVT),
    LIGHT_CTL_CLI_PUBLISH = (1 << ESP_BLE_MESH_LIGHT_CLIENT_PUBLISH_EVT),
    LIGHT_CTL_CLI_TIMEOUT = (1 << ESP_BLE_MESH_LIGHT_CLIENT_TIMEOUT_EVT),
    LIGHT_CTL_CLI_EVT_ALL = (LIGHT_CTL_CLI_EVT_GET | LIGHT_CTL_CLI_EVT_SET | LIGHT_CTL_CLI_PUBLISH | LIGHT_CTL_CLI_TIMEOUT)
} light_ctl_cli_evt_t;

typedef void (*light_cli_cb)(const esp_ble_mesh_light_client_cb_param_t *param, light_ctl_cli_evt_t evt);
typedef struct light_ctl_cli_cb_reg
{
    light_cli_cb cb;                   /**< Registered callback function */
    uint32_t evt_bmap;                 /**< Bitmap of events the callback is registered for */
    struct light_ctl_cli_cb_reg *next; /**< Pointer to the next callback registration */
} light_ctl_cli_cb_reg_t;

esp_err_t prod_light_ctl_cli_reg_cb(light_cli_cb cb, uint32_t config_evt_bmap);
esp_err_t prod_light_ctl_client_init();

#endif /*__LIGHT_CTL_CLIENT_H__*/
