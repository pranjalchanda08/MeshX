/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_light_server.h
 * @brief Header file for the production lighting server.
 *
 * This file contains the declarations and definitions for the production
 * lighting server, including callback registration and initialization functions.
 *
 * @author Pranjal Chanda
 */

#ifndef __MESHX_LIGHT_SERVER_H__
#define __MESHX_LIGHT_SERVER_H__

#include "sys/queue.h"
#include "meshx_control_task.h"
#include "interface/ble_mesh/server/meshx_ble_mesh_light_srv.h"

typedef control_task_msg_handle_t meshx_lighting_server_cb;

/**
 * @brief Sends a message to the BLE subsystem via the control task.
 *
 * This function publishes a message to the BLE layer with the specified event and parameters.
 *
 * @param[in] evt          The event to be sent to the BLE layer.
 * @param[in] params       Pointer to the parameters associated with the event.
 *
 * @return
 *     - MESHX_SUCCESS: Message sent successfully.
 *     - MESHX_FAIL: Failed to send the message.
 */
meshx_err_t meshx_gen_light_srv_send_msg_to_ble(
    control_task_msg_evt_to_ble_t evt,
    const meshx_lighting_server_cb_param_t *params);

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
meshx_err_t meshx_lighting_reg_cb(uint32_t model_id, meshx_lighting_server_cb cb);

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
meshx_err_t meshx_lighting_srv_dereg_cb(uint32_t model_id, meshx_lighting_server_cb cb);
/**
 * @brief Initialize the production lighting server.
 *
 * @return MESHX_SUCCESS on success, or an appropriate error code on failure.
 */
meshx_err_t meshx_lighting_srv_init(void);

#endif /* __MESHX_LIGHT_SERVER_H__ */
