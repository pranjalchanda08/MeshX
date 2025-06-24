/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file meshx_gen_client.c
 * @brief Implementation of the MeshX generic client model for BLE mesh nodes.
 *        This file contains functions for registering, deregistering, and
 *        initializing the generic client model.
 *
 * The MeshX generic client model provides an interface for handling BLE mesh
 * client operations, including callback registration and initialization.
 *
 * @author Pranjal Chanda
 *
 */
#include "stdlib.h"
#include "meshx_gen_client.h"

#define MESHX_SERVER_INIT_MAGIC_NO 0x1121
static uint16_t meshx_client_init = 0;

/**
 * @brief Register a callback function for the meshxuction client model.
 *
 * This function registers a callback function that will be called when
 * specific events related to the meshxuction client model occur.
 *
 * @param[in] model_id  The ID of the model for which the callback is being registered.
 * @param[in] cb        The callback function to be registered.
 *
 * @return
 *     - MESHX_SUCCESS: Callback registered successfully.
 *     - MESHX_INVALID_ARG: Invalid arguments.
 *     - MESHX_FAIL: Failed to register the callback.
 */
meshx_err_t meshx_gen_cli_reg_cb(uint32_t model_id, meshx_gen_cli_cb_t cb)
{
    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_TO_BLE,
        model_id,
        (control_task_msg_handle_t)cb);
}

/**
 * @brief Callback function to deregister a generic client model.
 *
 * This function is called to deregister a generic client model identified by the given model ID.
 *
 * @param[in] model_id  The ID of the model to be deregistered.
 * @param[in] cb        The callback function to be deregistered.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_INVALID_ARG: Invalid argument
 *     - MESHX_FAIL: Other failures
 */
meshx_err_t meshx_gen_cli_dereg_cb(uint32_t model_id, meshx_gen_cli_cb_t cb)
{
    return control_task_msg_unsubscribe(
        CONTROL_TASK_MSG_CODE_TO_BLE,
        model_id,
        (control_task_msg_handle_t)cb);
}

/**
 * @brief Initialize the meshxuction generic client.
 *
 * This function sets up the necessary configurations and initializes the
 * meshxuction generic client for the BLE mesh node.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_FAIL: Failed to initialize the client
 */
meshx_err_t meshx_gen_cli_init(void)
{
    if (meshx_client_init == MESHX_SERVER_INIT_MAGIC_NO)
        return MESHX_SUCCESS;
    meshx_client_init = MESHX_SERVER_INIT_MAGIC_NO;

    return meshx_plat_gen_cli_init();
}

/**
 * @brief Sends a generic client message in the MeshX framework.
 *
 * This function constructs and sends a message from a generic client model to a specified address
 * within the mesh network. It uses the provided model context, state parameters, opcode, and addressing
 * information to format the message appropriately.
 *
 * @param[in] model     The model context or memory handle associated with the client.
 * @param[in] state     Pointer to the structure containing the state to be set or queried.
 * @param[in] opcode    The opcode representing the type of generic client message to send.
 * @param[in] addr      The destination address within the mesh network.
 * @param[in] net_idx   The network index identifying the subnet to use for sending the message.
 * @param[in] app_idx   The application key index used for encrypting the message.
 *
 * @return meshx_err_t Returns an error code indicating the result of the operation.
 */
meshx_err_t meshx_gen_cli_send_msg(
    meshx_ptr_t model,
    meshx_gen_cli_set_t *state,
    uint16_t opcode,
    uint16_t addr,
    uint16_t net_idx,
    uint16_t app_idx
)
{
    if (!model || !state)
    {
        return MESHX_INVALID_ARG;
    }

    return meshx_plat_gen_cli_send_msg(
        model, state, opcode, addr, net_idx, app_idx
    );
}
