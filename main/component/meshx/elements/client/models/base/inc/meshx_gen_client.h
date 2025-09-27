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

#if CONFIG_ENABLE_GEN_CLIENT

/**
 * @brief Generic Client Model send parameters.
 *        This structure is used to pass parameters to the Generic Client Model
 *        for sending messages. It includes the element ID, model pointer,
 *        state parameters, opcode, destination address, network index, and
 *        application index.
 */
typedef struct meshx_gen_client_send_params
{
    meshx_ptr_t model;              /**< Model context associated with the message. */
    uint16_t opcode;                /**< Opcode associated with the message. */
    uint16_t addr;                  /**< Destination address associated with the message. */
    uint16_t net_idx;               /**< Network index associated with the message. */
    uint16_t app_idx;               /**< Application key index associated with the message. */
    meshx_gen_cli_set_t *state;     /**< State parameters associated with the message. */
}meshx_gen_client_send_params_t;

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
 * @brief Sends a Generic Client message over BLE Mesh.
 *
 * This function sends a message from a Generic Client model to a specified address
 * within the BLE Mesh network, using the provided opcode and parameters.
 *
 * @param[in] params   Pointer to the structure containing the message parameters to set.
 *
 * @return meshx_err_t  Result of the operation. Returns MESHX_OK on success or an error code on failure.
 */
meshx_err_t meshx_gen_cli_send_msg(meshx_gen_client_send_params_t *params);

#endif /* CONFIG_ENABLE_GEN_CLIENT */
#endif /* __MESHX_GEN_CLIENT_H_ */
