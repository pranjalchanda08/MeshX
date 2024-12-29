/**
 * @file light_ctl_client.h
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

typedef void (*light_cli_cb)(const esp_ble_mesh_light_client_cb_param_t *param, light_ctl_cli_evt_t evt);
typedef struct light_ctl_cli_cb_reg
{
    light_cli_cb cb;                   /**< Registered callback function */
    uint32_t evt_bmap;                 /**< Bitmap of events the callback is registered for */
    SLIST_ENTRY(light_ctl_cli_cb_reg) next; /**< SLIST entry for the next callback registration */
} light_ctl_cli_cb_reg_t;

/**
 * @brief Register a callback function for the Light CTL Client.
 *
 * This function registers a callback function that will be called when specific
 * events occur in the Light CTL Client model.
 *
 * @param[in] cb The callback function to register.
 * @param[in] config_evt_bmap A bitmap representing the configuration events to register for.
 *
 * @return
 *    - ESP_OK: Success
 *    - ESP_ERR_INVALID_ARG: Invalid argument
 *    - ESP_FAIL: Other failures
 */
esp_err_t prod_light_ctl_cli_reg_cb(light_cli_cb cb, uint32_t config_evt_bmap);

/**
 * @brief Initialize the Light CTL Client model.
 *
 * This function initializes the Light CTL Client model, setting up necessary
 * resources and configurations.
 *
 * @return
 *    - ESP_OK: Success
 *    - ESP_ERR_NO_MEM: Out of memory
 *    - ESP_FAIL: Other failures
 */
esp_err_t prod_light_ctl_client_init();

/**
 * @brief Send a Light CTL message.
 *
 * This function sends a Light CTL message with the specified parameters.
 *
 * @param model Pointer to the BLE Mesh model.
 * @param opcode Opcode of the message to be sent.
 * @param addr Destination address of the message.
 * @param net_idx Network index to be used.
 * @param app_idx Application index to be used.
 * @param lightness Lightness value to be sent.
 * @param temperature Temperature value to be sent.
 * @param delta_uv Delta UV value to be sent.
 * @param tid Transaction ID.
 *
 * @return ESP_OK on success or an error code on failure.
 */
esp_err_t prod_light_ctl_send_msg(esp_ble_mesh_model_t *model,
                                  uint16_t opcode,
                                  uint16_t addr,
                                  uint16_t net_idx,
                                  uint16_t app_idx,
                                  uint16_t lightness,
                                  uint16_t temperature,
                                  uint16_t delta_uv,
                                  uint8_t tid);

/**
 * @brief Send a Light CTL Temperature message.
 *
 * This function sends a Light CTL Temperature message to a specified address.
 *
 * @param model       Pointer to the BLE Mesh model.
 * @param opcode      Opcode of the message.
 * @param addr        Destination address of the message.
 * @param net_idx     Network index to be used.
 * @param app_idx     Application index to be used.
 * @param temperature Light CTL Temperature value.
 * @param delta_uv    Light CTL Delta UV value.
 * @param tid         Transaction ID.
 *
 * @return
 *     - ESP_OK on success
 *     - Appropriate error code on failure
 */
esp_err_t prod_light_ctl_temperature_send_msg(esp_ble_mesh_model_t *model,
                                              uint16_t opcode,
                                              uint16_t addr,
                                              uint16_t net_idx,
                                              uint16_t app_idx,
                                              uint16_t temperature,
                                              uint16_t delta_uv,
                                              uint8_t tid);

#endif /*__LIGHT_CTL_CLIENT_H__*/
