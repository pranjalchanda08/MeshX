#pragma once

/**
 * @file prod_onoff_client.h
 * @brief Header file for the On/Off client model in BLE mesh.
 *
 * This file contains the definitions and function prototypes for the On/Off client model.
 */

#include <esp_ble_mesh_defs.h>
#include <esp_ble_mesh_common_api.h>
#include <esp_ble_mesh_networking_api.h>
#include <esp_ble_mesh_generic_model_api.h>
#include <esp_ble_mesh_local_data_operation_api.h>

/**
 * @brief Enumeration of On/Off client events.
 */
typedef enum
{
    PROD_ONOFF_CLI_EVT_GET = (1 << ESP_BLE_MESH_GENERIC_CLIENT_GET_STATE_EVT),
    PROD_ONOFF_CLI_EVT_SET = (1 << ESP_BLE_MESH_GENERIC_CLIENT_SET_STATE_EVT),
    PROD_ONOFF_CLI_PUBLISH = (1 << ESP_BLE_MESH_GENERIC_CLIENT_PUBLISH_EVT),
    PROD_ONOFF_CLI_TIMEOUT = (1 << ESP_BLE_MESH_GENERIC_CLIENT_TIMEOUT_EVT),
    PROD_ONOFF_CLI_EVT_ALL = (PROD_ONOFF_CLI_EVT_GET | PROD_ONOFF_CLI_EVT_SET | PROD_ONOFF_CLI_PUBLISH | PROD_ONOFF_CLI_TIMEOUT)
} prod_onoff_cli_evt_t;

/**
 * @brief Callback function type for On/Off client events.
 *
 * @param param Pointer to the callback parameter structure.
 * @param evt Event type.
 */
typedef void (*prod_onoff_cli_cb)(const esp_ble_mesh_generic_client_cb_param_t *param, prod_onoff_cli_evt_t evt);

/**
 * @brief Structure for On/Off client callback registration.
 */
typedef struct prod_onoff_cli_cb_reg
{
    prod_onoff_cli_cb cb;               /**< Registered callback function */
    uint32_t evt_bmap;                  /**< Bitmap of events the callback is registered for */
    struct prod_onoff_cli_cb_reg *next; /**< Pointer to the next callback registration */
} prod_onoff_cli_cb_reg_t;

/**
 * @brief Initialize the On/Off client model.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t prod_onoff_client_init(void);

/**
 * @brief Register a callback function for On/Off client events.
 *
 * @param cb Callback function to register.
 * @param config_evt_bmap Bitmap of events to register the callback for.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t prod_onoff_reg_cb(prod_onoff_cli_cb cb, uint32_t config_evt_bmap);
