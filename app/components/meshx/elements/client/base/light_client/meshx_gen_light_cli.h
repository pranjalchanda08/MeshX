/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file meshx_gen_light_cli.h
 * @brief Implementation of the MeshX generic light client model for BLE mesh nodes.
 *        This file contains functions for registering, deregistering, and
 *        initializing the generic light client model.
 *
 * The MeshX generic light client model provides an interface for handling BLE mesh
 * light client operations, including callback registration and initialization.
 *
 * @author Pranjal Chanda
 *
 */

#ifndef __MESHX_GEN_LIGHT_CLI_H_
#define __MESHX_GEN_LIGHT_CLI_H_

#include "meshx_err.h"
#include "meshx_control_task.h"
#include "interface/ble_mesh/client/meshx_ble_mesh_light_cli.h"

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
meshx_err_t meshx_gen_light_cli_init(void);

/**
 * @brief Send a message using the generic client model.
 *
 * This function sends a message using the generic client model, allowing
 * interaction with the BLE mesh network.
 *
 * @param[in] model     Pointer to the model instance.
 * @param[in] state     Pointer to the state to be set.
 * @param[in] opcode    The operation code for the message.
 * @param[in] addr      The address to which the message is sent.
 * @param[in] net_idx   The network index for routing the message.
 * @param[in] app_idx   The application index for the message.
 *
 * @return
 *     - MESHX_SUCCESS: Message sent successfully.
 *     - Appropriate error code on failure.
 */
meshx_err_t meshx_gen_light_send_msg(
    meshx_ptr_t model,
    meshx_light_client_set_state_t *state,
    uint16_t opcode,
    uint16_t addr,
    uint16_t net_idx,
    uint16_t app_idx
);

#endif /* __MESHX_GEN_LIGHT_CLI_H_ */
