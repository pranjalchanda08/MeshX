/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_light_server.c
 * @brief Implementation of the BLE Mesh Lighting Server for the ESP32.
 *
 * This file contains the implementation of the BLE Mesh Lighting Server,
 * including initialization, event handling, and callback registration.
 *
 * @author Pranjal Chanda
 */

#include "meshx_platform_ble_mesh.h"
#include "meshx_light_server.h"
#include "meshx_gen_server.h"

#define TAG __func__

#define MESHX_SERVER_INIT_MAGIC_NO 0x2483

static uint16_t meshx_lighting_server_init = 0;

/**
 * @brief Callback function to deregister a lighting server model.
 *
 * This function is called to deregister a lighting server model identified by the given model ID.
 *
 * @param[in] model_id  The ID of the model to be deregistered.
 * @param[in] cb        The callback function to be deregistered.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_INVALID_ARG: Invalid argument
 *     - MESHX_FAIL: Other failures
 */
meshx_err_t meshx_lighting_srv_dereg_cb(uint32_t model_id, meshx_lighting_server_cb cb)
{
    return control_task_msg_unsubscribe(CONTROL_TASK_MSG_CODE_FRM_BLE, model_id, (control_task_msg_handle_t)cb);
}
/**
 * @brief Register a callback function for the lighting server model.
 *
 * This function registers a callback function that will be called when
 * certain events occur in the lighting server model.
 *
 * @param[in] model_id  The ID of the lighting server model.
 * @param[in] cb        The callback function to register.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_INVALID_ARG: Invalid argument
 *    - MESHX_FAIL: Other failures
 */
meshx_err_t meshx_lighting_reg_cb(uint32_t model_id, meshx_lighting_server_cb cb)
{
    return control_task_msg_subscribe(CONTROL_TASK_MSG_CODE_FRM_BLE, model_id, (control_task_msg_handle_t)cb);
}

/**
 * @brief Initialize the meshxuction lighting server.
 *
 * This function sets up the necessary configurations and initializes the
 * meshxuction lighting server for the BLE mesh node.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_FAIL: Failed to initialize the lighting server
 */
meshx_err_t meshx_lighting_srv_init(void)
{
    if (meshx_lighting_server_init == MESHX_SERVER_INIT_MAGIC_NO)
        return MESHX_SUCCESS;
    meshx_lighting_server_init = MESHX_SERVER_INIT_MAGIC_NO;

    return meshx_plat_light_srv_init();
}
