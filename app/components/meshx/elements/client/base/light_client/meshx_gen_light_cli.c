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
#include "meshx_gen_light_cli.h"

#define MESHX_CLIENT_INIT_MAGIC_NO 0x4309
static uint16_t meshx_client_init = 0;

/**
 * @brief Register a callback function for the meshxuction client model.
 *
 * This function registers a callback function that will be called when
 * specific events related to the meshx function client model occur.
 *
 * @param[in] model_id  The ID of the model for which the callback is being registered.
 * @param[in] cb        The callback function to be registered.
 *
 * @return
 *     - MESHX_SUCCESS: Callback registered successfully.
 *     - MESHX_INVALID_ARG: Invalid arguments.
 *     - MESHX_FAIL: Failed to register the callback.
 */
meshx_err_t meshx_gen_light_cli_reg_cb(uint32_t model_id, meshx_gen_light_cli_cb_t cb)
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
meshx_err_t meshx_gen_light_cli_dereg_cb(uint32_t model_id, meshx_gen_light_cli_cb_t cb)
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
meshx_err_t meshx_gen_light_cli_init(void)
{
    if (meshx_client_init == MESHX_CLIENT_INIT_MAGIC_NO)
        return MESHX_SUCCESS;
    meshx_client_init = MESHX_CLIENT_INIT_MAGIC_NO;

    return meshx_plat_gen_light_cli_init();
}
