/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file meshx_gen_server.c
 * @brief Implementation of the MeshX generic server model for BLE mesh nodes.
 *        This file contains functions for registering, deregistering, and
 *        initializing the generic server model.
 *
 * The MeshX generic server model provides an interface for handling BLE mesh
 * server operations, including callback registration and initialization.
 *
 * @author Pranjal Chanda
 *
 */
#include "stdlib.h"
#include "meshx_gen_server.h"

#define MESHX_SERVER_INIT_MAGIC_NO 0x1121
static uint16_t meshx_server_init = 0;

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
meshx_err_t meshx_gen_srv_reg_cb(uint32_t model_id, meshx_server_cb cb)
{
    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_FRM_BLE,
        model_id,
        (control_task_msg_handle_t)cb);
}

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
meshx_err_t meshx_gen_srv_dereg_cb(uint32_t model_id, meshx_server_cb cb)
{
    return control_task_msg_unsubscribe(
        CONTROL_TASK_MSG_CODE_FRM_BLE,
        model_id,
        (control_task_msg_handle_t)cb);
}

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
meshx_err_t meshx_gen_srv_init(void)
{
    if (meshx_server_init == MESHX_SERVER_INIT_MAGIC_NO)
        return MESHX_SUCCESS;
    meshx_server_init = MESHX_SERVER_INIT_MAGIC_NO;

    return meshx_plat_gen_srv_init();
}
