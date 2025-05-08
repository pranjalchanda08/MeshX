/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_provisioning_server.h
 * @brief Header file for provisioning related definitions and functions.
 *
 * This file contains the definitions and function declarations for provisioning
 * in the BLE mesh node application.
 *
 * @author Pranjal Chanda
 */

#ifndef __MESHX_PROV__
#define __MESHX_PROV__

#include "interface/meshx_platform.h"
#include "interface/ble_mesh/meshx_ble_mesh_prov_srv.h"

/**
 * @brief Initialize provisioning parameters.
 *
 * This function initializes the provisioning parameters by copying the UUID from the provided
 * server configuration and registering the provisioning callback.
 *
 * @param[in] prov_cfg Pointer to the provisioning parameters structure containing the UUID.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_FAIL: Failed to register provisioning callback
 */
meshx_err_t meshx_init_prov(const meshx_prov_params_t * prov_cfg);

/**
 * @brief Get the provisioning parameters.
 *
 * This function returns a pointer to the global provisioning parameters.
 *
 * @return Pointer to the global provisioning parameters.
 */
MESHX_PROV *meshx_get_prov(void);

#endif /* __MESHX_PROV__ */
