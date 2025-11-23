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
 * @author Pranjal Chanda
 */

#ifndef __RELAY_CLIENT_MODEL_H__
#define __RELAY_CLIENT_MODEL_H__

#include "meshx_common.h"
#include "meshx_onoff_client.h"

#if CONFIG_RELAY_CLIENT_COUNT > 0

/**
 * @def RELAY_CLI_MODEL_SIG_CNT
 * @brief Number of SIG models in a relay model element.
 */
#define RELAY_CLI_MODEL_SIG_CNT RELAY_CLI_SIG_MAX_ID // No of SIG models in a relay model element
/**
 * @def RELAY_CLI_MODEL_VEN_CNT
 * @brief Number of Vendor models in a relay model element.
 */
#define RELAY_CLI_MODEL_VEN_CNT 0 // No of VEN models in a relay model element

typedef enum
{
    RELAY_CLI_SIG_ONOFF_MODEL_ID,   /**< On/Off model ID */
    RELAY_CLI_SIG_MAX_ID,           /**< Maximum model ID */
} relay_cli_sig_id_t;

/**
 * @brief Structure to hold the relay client message.
 */
typedef struct meshx_gen_on_off_cli_msg
{
    uint8_t ack;         /**< Acknowledgment flag */
    uint8_t set_get;     /**< Set/Get flag */
    uint8_t reserved;    /**< Reserved for future use */
    uint16_t element_id; /**< Element ID */
} meshx_gen_on_off_cli_msg_t;

/**
 * @brief Structure to hold the context of the relay client.
 */
typedef struct meshx_relay_client_model_ctx
{
    uint8_t tid;                /**< Transaction ID */
    uint16_t app_id;            /**< Application ID */
    uint16_t pub_addr;          /**< Publish address */
    meshx_on_off_cli_state_t state; /**< State of the relay client */
    meshx_on_off_cli_state_t prev_state; /**< Previous state of the relay client */
} meshx_relay_client_model_ctx_t;

/**
 * @brief Structure to hold the context and configuration for the relay client element.
 */
typedef struct relay_client_element
{
    size_t element_model_init;                                     /**< Initialization status of the element model */
    meshx_relay_client_model_ctx_t *cli_ctx;                       /**< Pointer to the relay client context */
    meshx_onoff_client_model_t *onoff_cli_model;                   /**< Pointer to the list of relay client on/off generic structures */
    MESHX_MODEL relay_cli_sig_model_list[RELAY_CLI_MODEL_SIG_CNT]; /**< Pointer to the list of relay client SIG model structures */
} relay_client_elements_t;

typedef struct relay_client_element_ctrl
{
    size_t element_cnt;               /**< Number of elements */
    size_t element_id_end;            /**< Ending ID of the element */
    size_t element_id_start;          /**< Starting ID of the element */
    relay_client_elements_t *el_list; /**< Pointer to the list of relay client elements */
} relay_client_element_ctrl_t;

/**
 * @brief Retrieves the current state of the relay element.
 *
 * This function constructs a generic On/Off client message to request the current
 * state of a relay element identified by the given element ID. It then publishes
 * this message to the control task for BLE communication.
 *
 * @param[in] el_id The element ID of the relay whose state is to be retrieved.
 * @return meshx_err_t Returns the result of the message publish operation.
 */
meshx_err_t meshx_relay_el_get_state(uint16_t el_id);

/**
 * @brief Set the On/Off state for a specific element in the BLE mesh network.
 *
 * This function sets the On/Off state of the specified element, optionally waiting for an acknowledgment.
 *
 * @param[in] el_id The ID of the element for which to set the On/Off state.
 * @param[in] ack   Whether to wait for an acknowledgment (true) or not (false).
 *
 * @return
 *     - MESHX_SUCCESS: Successfully set the state.
 *     - MESHX_INVALID_ARG: Invalid element ID or parameters.
 *     - MESHX_FAIL: Failed to set the state.
 */
meshx_err_t meshx_relay_el_set_state(uint16_t el_id, bool ack);

/**
 * @brief Create Dynamic Relay Model Elements
 *
 * @param[in]       pdev            Pointer to device structure
 * @param[in]       element_cnt     Maximum number of relay models
 *
 * @return meshx_err_t
 */
meshx_err_t create_relay_client_elements(dev_struct_t *pdev, uint16_t element_cnt);

#endif /* CONFIG_RELAY_CLIENT_COUNT > 0 */
#endif /*__RELAY_CLIENT_MODEL_H__*/
