/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_gen_server.h
 * @brief Header file for the generic server model in the BLE mesh node application.
 *
 * This file contains the function declarations and data structures for registering,
 * deregistering, and initializing the generic server model callbacks in the BLE mesh node application.
 *
 *
 */

#ifndef __MESHX_GEN_SERVER_H__
#define __MESHX_GEN_SERVER_H__

#include "interface/ble_mesh/server/meshx_ble_mesh_gen_srv.h"

/**
 * @brief Register a callback function for the meshxuction server model.
 *
 * This function registers a callback function that will be called when
 * specific events related to the meshxuction server model occur.
 *
 * @param[in] model_id  The ID of the model for which the callback is being registered.
 * @param[in] cb        The callback function to be registered.
 *
 * @return
 *     - MESHX_SUCCESS: Callback registered successfully.
 *     - MESHX_INVALID_ARG: Invalid arguments.
 *     - MESHX_FAIL: Failed to register the callback.
 */
meshx_err_t meshx_gen_srv_reg_cb(uint32_t model_id, meshx_server_cb cb);

/**
 * @brief Callback function to deregister a generic server model.
 *
 * This function is called to deregister a generic server model identified by the given model ID.
 *
 * @param[in] model_id  The ID of the model to be deregistered.
 * @param[in] cb        The callback function to be deregistered.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_INVALID_ARG: Invalid argument
 *     - MESHX_FAIL: Other failures
 */
meshx_err_t meshx_gen_srv_dereg_cb(uint32_t model_id, meshx_server_cb cb);

/**
 * @brief Initialize the meshxuction generic server.
 *
 * This function sets up the necessary configurations and initializes the
 * meshxuction generic server for the BLE mesh node.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_FAIL: Failed to initialize the server
 */
meshx_err_t meshx_gen_srv_init(void);

#endif /* __MESHX_GEN_SERVER_H__ */
