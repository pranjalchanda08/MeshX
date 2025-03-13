/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_relay_client_element.h
 * @brief Header file for the Relay Client Model in BLE Mesh.
 *
 * This file contains the definitions and function declarations for the Relay Client Model
 * used in BLE Mesh applications. The Relay Client Model is responsible for managing relay
 * client elements, sending messages to relay nodes or groups, and handling the state and
 * context of relay clients.
 *
 * @details
 * The file defines constants, data structures, and function prototypes for creating and
 * managing relay client elements. It includes the following key components:
 * - Definitions for the number of relay client elements, SIG models, and message types.
 * - Data structures for relay client context, messages, and elements.
 * - Function prototypes for creating relay client elements and sending messages.
 *
 */
#pragma once

#include "app_common.h"
#include "meshx_onoff_client.h"

/**
 * @def CONFIG_RELAY_CLIENT_COUNT
 * @brief Number of relay client elements, configurable via build configuration.
 * If not defined, defaults to RELAY_CLIENT_ELEMENT_NOS_DEF.
 */
#ifndef CONFIG_RELAY_CLIENT_COUNT
#define CONFIG_RELAY_CLIENT_COUNT RELAY_CLIENT_ELEMENT_NOS_DEF
#endif

/**
 * @def RELAY_CLI_MODEL_SIG_CNT
 * @brief Number of SIG models in a relay model element.
 */

#define RELAY_CLI_MODEL_SIG_CNT 1 // No of SIG models in a relay model element

/**
 * @def RELAY_CLI_MODEL_VEN_CNT
 * @brief Number of Vendor models in a relay model element.
 */
#define RELAY_CLI_MODEL_VEN_CNT 0 // No of VEN models in a relay model element

/**
 * @def RELAY_CLI_MSG_SET
 * @brief Message type for setting relay client state.
 */
#define RELAY_CLI_MSG_SET 0

/**
 * @def RELAY_CLI_MSG_GET
 * @brief Message type for getting relay client state.
 */
#define RELAY_CLI_MSG_GET 1

/**
 * @def RELAY_CLI_MSG_ACK
 * @brief Acknowledgment message type.
 */
#define RELAY_CLI_MSG_ACK 1

/**
 * @def RELAY_CLI_MSG_NO_ACK
 * @brief Non-acknowledgment message type.
 */
#define RELAY_CLI_MSG_NO_ACK 0

/**
 * @def RELAY_CLIENT_ELEMENT_NOS_DEF
 * @brief Defines the number of relay client elements.
 *
 * This macro defines the number of relay client elements used in the MeshX application.
 */
#define RELAY_CLIENT_ELEMENT_NOS_DEF 3

/**
 * @brief Structure to hold the state of the relay client.
 */
typedef struct relay_client_state
{
    uint8_t on_off;      /**< Current On/Off state */
    uint8_t prev_on_off; /**< Previous On/Off state */
} relay_client_state_t;

/**
 * @brief Structure to hold the context of the relay client.
 */
typedef struct rel_cli_ctx
{
    uint8_t tid;                /**< Transaction ID */
    uint16_t app_id;            /**< Application ID */
    uint16_t pub_addr;          /**< Publish address */
    relay_client_state_t state; /**< State of the relay client */
} rel_cli_ctx_t;

/**
 * @brief Structure to hold the relay client message.
 */
typedef struct relay_client_msg
{
    uint8_t ack;         /**< Acknowledgment flag */
    uint8_t set_get;     /**< Set/Get flag */
    uint16_t element_id; /**< Element ID */
} relay_client_msg_t;

/**
 * @brief Structure to hold the context and configuration for the relay client element.
 */
typedef struct relay_client_element
{
    size_t element_cnt;                              /**< Number of elements */
    size_t element_id_end;                           /**< Ending ID of the element */
    size_t element_id_start;                         /**< Starting ID of the element */
    size_t element_model_init;                       /**< Initialization status of the element model */
    rel_cli_ctx_t *rel_cli_ctx;                      /**< Pointer to the relay client context */
    MESHX_MODEL_PUB *relay_cli_pub_list;    /**< Pointer to the list of relay client publication structures */
    esp_ble_mesh_client_t *relay_cli_onoff_gen_list; /**< Pointer to the list of relay client on/off generic structures */
    MESHX_MODEL **relay_cli_sig_model_list; /**< Pointer to the list of relay client SIG model structures */
} relay_client_elements_t;

/**
 * @brief Create Dynamic Relay Model Elements
 *
 * @param[in]       pdev            Pointer to device structure
 * @param[in]       element_cnt     Maximum number of relay models
 *
 * @return meshx_err_t
 */
meshx_err_t create_relay_client_elements(dev_struct_t *pdev, uint16_t element_cnt);

/**
 * @brief Sends a relay message over BLE mesh.
 *
 * This function sends a relay message to a specified element in the BLE mesh network.
 *
 * @param[in] pdev Pointer to the device structure.
 * @param[in] element_id The ID of the element to which the message is sent.
 * @param[in] set_get Indicates whether the message is a set (0) or get (1) operation.
 * @param[in] ack Indicates whether an acknowledgment is required (1) or not (0).
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_INVALID_ARG: Invalid argument
 *     - MESHX_FAIL: Sending message failed
 */
meshx_err_t ble_mesh_send_relay_msg(dev_struct_t *pdev, uint16_t element_id, uint8_t set_get, uint8_t ack);
