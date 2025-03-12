/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_light_ctl_client.h
 * @brief Header file for the Light CTL (Color Temperature Light) Client model.
 *
 * This file contains the definitions and function declarations for the Light CTL Client model
 * used in ESP32 BLE Mesh applications. The Light CTL Client model is responsible for sending
 * messages to control the color temperature and lightness of a light.
 *
 * The file includes necessary BLE Mesh headers and defines the event types, callback function
 * types, and structures used for registering and handling Light CTL Client events.
 *
 */

#ifndef __LIGHT_CTL_CLIENT_H__
#define __LIGHT_CTL_CLIENT_H__

#include "meshx_err.h"
#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_lighting_model_api.h"
#include "esp_ble_mesh_local_data_operation_api.h"
#include "sys/queue.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef enum
{
    LIGHT_CTL_CLI_EVT_GET = BIT(ESP_BLE_MESH_LIGHT_CLIENT_GET_STATE_EVT),
    LIGHT_CTL_CLI_EVT_SET = BIT(ESP_BLE_MESH_LIGHT_CLIENT_SET_STATE_EVT),
    LIGHT_CTL_CLI_PUBLISH = BIT(ESP_BLE_MESH_LIGHT_CLIENT_PUBLISH_EVT),
    LIGHT_CTL_CLI_TIMEOUT = BIT(ESP_BLE_MESH_LIGHT_CLIENT_TIMEOUT_EVT),
    LIGHT_CTL_CLI_EVT_ALL = (LIGHT_CTL_CLI_EVT_GET | LIGHT_CTL_CLI_EVT_SET | LIGHT_CTL_CLI_PUBLISH | LIGHT_CTL_CLI_TIMEOUT)
} light_ctl_cli_evt_t;

typedef bool (*light_cli_cb)(const esp_ble_mesh_light_client_cb_param_t *param, light_ctl_cli_evt_t evt);

/**
 * @brief Structure to hold the Light CTL Client context.
 */
typedef struct light_ctl_cli_cb_reg
{
    light_cli_cb cb;                        /**< Registered callback function */
    uint32_t evt_bmap;                      /**< Bitmap of events the callback is registered for */
    SLIST_ENTRY(light_ctl_cli_cb_reg) next; /**< SLIST entry for the next callback registration */
} light_ctl_cli_cb_reg_t;

/**
 * @brief Structure to hold arguments for sending Light CTL messages.
 */
typedef struct light_ctl_send_args
{
    esp_ble_mesh_model_t *model; /**< Pointer to the BLE Mesh model. */
    uint16_t opcode;             /**< Opcode of the message to be sent. */
    uint16_t addr;               /**< Destination address of the message. */
    uint16_t net_idx;            /**< Network index to be used for sending the message. */
    uint16_t app_idx;            /**< Application index to be used for sending the message. */
    uint16_t lightness;          /**< Lightness value to be sent. */
    uint16_t temperature;        /**< Temperature value to be sent. */
    uint16_t delta_uv;           /**< Delta UV value to be sent. */
    uint16_t temp_range_min;     /**< Minimum temperature value in the range. */
    uint16_t temp_range_max;     /**< Maximum temperature value in the range. */
    uint8_t temp_range_flag;     /**< Flag to indicate if the temperature range is sent. */
    uint8_t tid;                 /**< Transaction ID to identify the message. */
} light_ctl_send_args_t;

/**
 * @brief Register a callback function for the Light CTL Client.
 *
 * This function registers a callback function that will be called when specific
 * events occur in the Light CTL Client model.
 *
 * @param[in] cb                The callback function to register.
 * @param[in] config_evt_bmap   A bitmap representing the configuration events to register for.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_INVALID_ARG: Invalid argument
 *    - ESP_FAIL: Other failures
 */
meshx_err_t meshx_light_ctl_cli_reg_cb(light_cli_cb cb, uint32_t config_evt_bmap);

/**
 * @brief Initialize the Light CTL Client model.
 *
 * This function initializes the Light CTL Client model, setting up necessary
 * resources and configurations.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_NO_MEM: Out of memory
 *    - ESP_FAIL: Other failures
 */
meshx_err_t meshx_light_ctl_client_init();

/**
 * @brief Send a Light CTL message.
 *
 * This function sends a Light CTL message with the specified parameters.
 *
 * @param[in] params Pointer to the structure containing the message parameters.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - Appropriate error code on failure
 */
meshx_err_t meshx_light_ctl_send_msg(light_ctl_send_args_t * params);

/**
 * @brief Sends a message to control the light temperature.
 *
 * This function sends a message to adjust the light temperature using the provided parameters.
 *
 * @param[in] params Pointer to a structure containing the parameters for the light temperature control message.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_INVALID_ARG: Invalid argument
 *    - ESP_FAIL: Sending message failed
 */
meshx_err_t meshx_light_ctl_temperature_send_msg(light_ctl_send_args_t * params);

#endif /*__LIGHT_CTL_CLIENT_H__*/
