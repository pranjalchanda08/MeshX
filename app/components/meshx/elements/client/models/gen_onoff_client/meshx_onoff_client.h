/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_onoff_client.h
 * @brief Header file for the On/Off client model in BLE mesh.
 *
 * This file contains the definitions and function prototypes for the On/Off client model.
 */

#pragma once

#include "meshx_err.h"
#include "meshx_platform.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <sys/queue.h>
/**
 * @brief Enumeration of On/Off client events.
 */
typedef enum
{
    MESHX_ONOFF_CLI_EVT_GET = BIT(ESP_BLE_MESH_GENERIC_CLIENT_GET_STATE_EVT),
    MESHX_ONOFF_CLI_EVT_SET = BIT(ESP_BLE_MESH_GENERIC_CLIENT_SET_STATE_EVT),
    MESHX_ONOFF_CLI_PUBLISH = BIT(ESP_BLE_MESH_GENERIC_CLIENT_PUBLISH_EVT),
    MESHX_ONOFF_CLI_TIMEOUT = BIT(ESP_BLE_MESH_GENERIC_CLIENT_TIMEOUT_EVT),
    MESHX_ONOFF_CLI_EVT_ALL = (MESHX_ONOFF_CLI_EVT_GET | MESHX_ONOFF_CLI_EVT_SET | MESHX_ONOFF_CLI_PUBLISH | MESHX_ONOFF_CLI_TIMEOUT)
} meshx_onoff_cli_evt_t;

/**
 * @brief Callback function type for On/Off client events.
 *
 * @param param Pointer to the callback parameter structure.
 * @param evt Event type.
 */
typedef void (*meshx_onoff_cli_cb)(const esp_ble_mesh_generic_client_cb_param_t *param, meshx_onoff_cli_evt_t evt);

/**
 * @brief Structure for On/Off client callback registration.
 */
typedef struct meshx_onoff_cli_cb_reg {
    meshx_onoff_cli_cb cb;
    uint32_t evt_bmap;
    SLIST_ENTRY(meshx_onoff_cli_cb_reg) entries;
} meshx_onoff_cli_cb_reg_t;

/**
 * @brief Initialize the On/Off client model.
 *
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
meshx_err_t meshx_onoff_client_init(void);

/**
 * @brief Register a callback for OnOff Client events.
 *
 * This function allows users to register a callback for handling specific
 * OnOff Client events based on a provided event bitmap.
 *
 * @param[in] cb              Pointer to the callback function to register.
 * @param[in] config_evt_bmap Bitmap of events to register for.
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
meshx_err_t meshx_onoff_reg_cb(meshx_onoff_cli_cb cb, uint32_t config_evt_bmap);


/**
 * @brief Send a generic on/off client message.
 *
 * This function sends a generic on/off client message with the specified parameters.
 *
 * @param[in] model   Pointer to the BLE Mesh model structure.
 * @param[in] opcode  The operation code of the message.
 * @param[in] addr    The destination address to which the message is sent.
 * @param[in] net_idx The network index to be used for sending the message.
 * @param[in] app_idx The application index to be used for sending the message.
 * @param[in] state   The state value to be sent in the message.
 * @param[in] tid     The transaction ID to be used for the message.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_INVALID_ARG: Invalid argument
 *    - MESHX_NO_MEM: Out of memory
 *    - MESHX_FAIL: Sending message failed
 */
meshx_err_t meshx_onoff_client_send_msg(MESHX_MODEL *model,
        uint16_t opcode,
        uint16_t addr,
        uint16_t net_idx,
        uint16_t app_idx,
        uint8_t state,
        uint8_t tid
);
