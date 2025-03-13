/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_provisioning_server.h
 * @brief Header file for provisioning related definitions and functions.
 *
 * This file contains the definitions and function declarations for provisioning
 * in the BLE mesh node application.
 */

#ifndef __MESHX_PROV__
#define __MESHX_PROV__

#include "meshx_platform.h"

#define MESHX_PROV_INSTANCE g_meshx_prov

/**
 * @brief Structure to hold provisioning parameters.
 */
typedef struct prov_params
{
    uint8_t uuid[16]; /**< UUID for the provisioning device */
} prov_params_t;

extern MESHX_PROV g_meshx_prov;

/**
 * @brief Initialize provisioning parameters.
 *
 * This function initializes the provisioning parameters by copying the UUID from the provided
 * server configuration and registering the provisioning callback.
 *
 * @param[in] svr_cfg Pointer to the provisioning parameters structure containing the UUID.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_FAIL: Failed to register provisioning callback
 */
meshx_err_t meshx_init_prov(const prov_params_t * svr_cfg);

#endif /* __MESHX_PROV__ */
