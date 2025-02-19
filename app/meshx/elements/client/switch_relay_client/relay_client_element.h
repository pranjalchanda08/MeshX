/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file relay_client_element.h
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
#include "prod_onoff_client.h"

#define RELAY_CLIENT_ELEMENT_NOS_DEF 3

#ifndef CONFIG_RELAY_CLIENT_COUNT
#define CONFIG_RELAY_CLIENT_COUNT RELAY_CLIENT_ELEMENT_NOS_DEF
#endif

#define RELAY_CLI_MODEL_SIG_CNT 1 // No of SIG models in a relay model element
#define RELAY_CLI_MODEL_VEN_CNT 0 // No of VEN models in a relay model element

#define RELAY_CLI_MSG_SET 0
#define RELAY_CLI_MSG_GET 1
#define RELAY_CLI_MSG_ACK 1
#define RELAY_CLI_MSG_NO_ACK 0

/**
 * @brief Structure to hold the context of the relay client.
 */
typedef struct rel_cli_ctx
{
    uint8_t tid;        /**< Transaction ID */
    uint8_t state;      /**< State of the relay client */
    uint16_t app_id;    /**< Application ID */
    uint16_t pub_addr;  /**< Publish address */
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
 * @brief Structure to hold the relay client elements.
 */
typedef struct relay_client_element
{
    size_t model_cnt;
    size_t element_id_end;
    size_t element_id_start;
    rel_cli_ctx_t *rel_cli_ctx;
    esp_ble_mesh_model_pub_t *relay_cli_pub_list;
    esp_ble_mesh_client_t *relay_cli_onoff_gen_list;
    esp_ble_mesh_model_t **relay_cli_sig_model_list;
} relay_client_elements_t;

/**
 * @brief Create Dynamic Relay Model Elements
 *
 * @param[in]       pdev    Pointer to device structure
 *
 * @return esp_err_t
 */
esp_err_t create_relay_client_elements(dev_struct_t *pdev);

/**
 * @brief Send Msg to relay node or group represented by the provisioned publish address
 *
 * @param[in] pdev          Pointer to dev_struct
 * @param[in] element_id    Element id of relay client
 * @param[in] set_get       Message type: Set -> 1 Get -> 0
 * @param[in] ack           Set with ack, 1/0
 * @return esp_err_t
 */
esp_err_t ble_mesh_send_relay_msg(dev_struct_t *pdev, uint16_t element_id, uint8_t set_get, uint8_t ack);
