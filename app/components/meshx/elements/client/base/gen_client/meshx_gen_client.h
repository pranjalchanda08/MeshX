/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file meshx_gen_client.h
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
#ifndef __MESHX_GEN_CLIENT_H_
#define __MESHX_GEN_CLIENT_H_

#include "meshx_err.h"
#include "meshx_control_task.h"
#include "interface/ble_mesh/client/meshx_ble_mesh_gen_cli.h"

/**
 * @brief Registers a callback function for a specific generic server model.
 *
 * This function associates a callback with the given model ID, allowing the server
 * to handle events or messages related to that model.
 *
 * @param[in] model_id The unique identifier of the generic server model.
 * @param[in] cb       The callback function to be registered for the model.
 *
 * @return meshx_err_t Returns an error code indicating the result of the registration.
 *                     Possible values include success or specific error codes.
 */
meshx_err_t meshx_gen_client_from_ble_reg_cb(uint32_t model_id, meshx_gen_client_cb_t cb);

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
meshx_err_t meshx_gen_client_init(void);

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
);
#endif /* __MESHX_GEN_CLIENT_H_ */
