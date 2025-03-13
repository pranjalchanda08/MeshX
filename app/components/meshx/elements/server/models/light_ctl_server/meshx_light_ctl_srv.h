/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_light_ctl_srv.h
 * @brief Header file for the Light CTL Server module.
 *
 * This file contains the function declarations and necessary includes for
 * initializing and managing the Light CTL Server in the BLE mesh network.
 *
 */

#ifndef __MESHX_LIGHT_CTL_SRV_H__
#define __MESHX_LIGHT_CTL_SRV_H__

#include "app_common.h"
#include <meshx_light_server.h>
#include "meshx_control_task.h"

/**
 * @brief Initialize the Light CTL Server.
 *
 * This function initializes the Light CTL Server, setting up necessary
 * configurations and preparing it to handle light control operations
 * within the BLE mesh network.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_FAIL: Initialization failed
 */
meshx_err_t meshx_light_ctl_server_init(void);
/**
 * @brief Send the Light CTL Status message to the client.
 *
 * This function sends the Light CTL Status message to the client with the
 * specified lightness and temperature values.
 *
 * @param model Pointer to the Light CTL Server model.
 * @param ctx Pointer to the BLE Mesh message context.
 * @param lightness Lightness value to send.
 * @param temperature Temperature value to send.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_FAIL: Failure
 */
meshx_err_t meshx_send_ctl_status(MESHX_MODEL *model, esp_ble_mesh_msg_ctx_t* ctx, uint16_t lightness, uint16_t temperature);

#endif /*__MESHX_LIGHT_CTL_SRV_H__*/
