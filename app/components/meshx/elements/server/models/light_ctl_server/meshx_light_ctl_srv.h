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
#include "interface/ble_mesh/meshx_ble_mesh_cmn.h"

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

#endif /*__MESHX_LIGHT_CTL_SRV_H__*/
