/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_onoff_client.h
 * @brief Header file for the On/Off client model in BLE mesh.
 *
 * This file contains the definitions and function prototypes for the On/Off client model.
 */

#ifndef __MESHX_ONOFF_CLIENT_H__
#define __MESHX_ONOFF_CLIENT_H__

#include "app_common.h"
#include "meshx_gen_client.h"
#include "meshx_control_task.h"

#if CONFIG_ENABLE_GEN_ONOFF_CLIENT

#define MESHX_GEN_ON_OFF_CLI_MSG_SET 0
#define MESHX_GEN_ON_OFF_CLI_MSG_GET 1
#define MESHX_GEN_ON_OFF_CLI_MSG_ACK 1
#define MESHX_GEN_ON_OFF_CLI_MSG_NO_ACK 0

/**
 * @brief Structure to hold the state of the relay client.
 */
typedef struct relay_client_state
{
    uint8_t on_off;      /**< Current On/Off state */
} meshx_on_off_cli_state_t;

/**
 * @brief Structure representing the MeshX On/Off Client Model.
 *
 * This structure is used to define the On/Off client model in the MeshX framework.
 * It contains pointers to various components required for the On/Off client functionality.
 */
typedef struct meshx_onoff_client_model
{
    meshx_ptr_t meshx_onoff_client_sig_model;     /**< Pointer to the On/Off client SIG model. */
    meshx_ptr_t meshx_onoff_client_pub;           /**< Pointer to the list of relay client publication structures. */
    meshx_ptr_t meshx_onoff_client_gen_cli;       /**< Pointer to the list of relay client On/Off generic structures. */
} meshx_onoff_client_model_t;

/**
 * @brief Structure to hold the On/Off Server to element message.
 */
typedef struct meshx_on_off_cli_el_msg
{
    uint8_t err_code;           /**< Error code */
    meshx_model_t model;        /**< Generic OnOff Server model */
    meshx_ctx_t ctx;            /**< Context of the message */
    uint8_t on_off_state;       /**< The present value of Generic OnOff state */
}meshx_on_off_cli_el_msg_t;

/**
 * @brief Creates and initializes a Generic OnOff Client model instance.
 *
 * This function allocates and sets up a Generic OnOff Client model, associating it with the provided
 * SIG model context.
 *
 * @param[out] p_model      Pointer to a pointer where the created model instance will be stored.
 * @param[in]  p_sig_model  Pointer to the SIG model context to associate with the client model.
 *
 * @return meshx_err_t      Returns an error code indicating the result of the operation.
 *                         - MESHX_OK on success
 *                         - Appropriate error code otherwise
 */
meshx_err_t meshx_on_off_client_create(meshx_onoff_client_model_t **p_model, void *p_sig_model);

/**
 * @brief Delete the On/Off client model instance.
 *
 * This function deletes an instance of the On/Off client model, freeing
 * associated resources and setting the model pointer to NULL.
 *
 * @param[in,out] p_model Double pointer to the On/Off client model instance to be deleted.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully deleted the model.
 *     - MESHX_INVALID_ARG: Invalid argument provided.
 */
meshx_err_t meshx_on_off_client_delete(meshx_onoff_client_model_t **p_model);

/**
 * @brief Initialize the On/Off client model.
 *
 * This function initializes the On/Off client model for the BLE mesh node.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_FAIL: Failure
 */
meshx_err_t meshx_on_off_client_init(void);

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
meshx_err_t meshx_onoff_client_send_msg(
        meshx_onoff_client_model_t *model,
        uint16_t opcode, uint16_t addr,
        uint16_t net_idx, uint16_t app_idx,
        uint8_t state, uint8_t tid
);

/**
 * @brief Handle state changes for the Generic OnOff Client.
 *
 * This function processes state change events for the Generic OnOff Client,
 * updating the previous and next states based on the received message parameters.
 *
 * @param[in]       param           Pointer to the message structure containing the state change parameters.
 * @param[in,out]   p_prev_state    Pointer to the previous state structure.
 * @param[in,out]   p_next_state    Pointer to the next state structure.
 *
 * @return meshx_err_t Returns an error code indicating the result of the handler execution.
 *                     - MESHX_SUCCESS if a state change occurred
 *                     - MESHX_INVALID_STATE if no state change occurred
 *                     - Appropriate error code otherwise
 */
meshx_err_t meshx_gen_on_off_state_change_handle(
    meshx_on_off_cli_el_msg_t *param,
    meshx_on_off_cli_state_t *p_prev_state,
    meshx_on_off_cli_state_t *p_next_state
);

#endif /* CONFIG_ENABLE_GEN_ONOFF_CLIENT */
#endif /* __MESHX_ONOFF_CLIENT_H__ */
