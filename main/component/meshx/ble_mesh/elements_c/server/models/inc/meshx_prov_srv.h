/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_prov_srv.h
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
#include "interface/ble_mesh/server/meshx_ble_mesh_prov_srv.h"

#if CONFIG_ENABLE_PROVISIONING

/**
 * @brief Initialize provisioning parameters.
 *
 * This function initializes the provisioning parameters by copying the UUID from the provided
 * server configuration and registering the provisioning callback.
 *
 * @param[in] p_dev Pointer to the device structure.
 * @param[in] prov_cfg Pointer to the provisioning parameters structure containing the UUID.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_FAIL: Failed to register provisioning callback
 */
meshx_err_t meshx_init_prov(dev_struct_t *p_dev, const meshx_prov_params_t *prov_cfg);

/**
 * @brief Register a callback function for provisioning events.
 *
 * This function registers a callback function that will be called when
 * certain provisioning events occur.
 *
 * @param[in] cb The callback function to register.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_FAIL: Failed to register the callback
 */
meshx_err_t meshx_prov_srv_reg_el_client_cb(prov_srv_cb_t cb);

/**
 * @brief Register a callback function for provisioning events.
 *
 * This function registers a callback function that will be called when
 * certain provisioning events occur.
 *
 * @param[in] cb The callback function to register.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_FAIL: Failed to register the callback
 */
meshx_err_t meshx_prov_srv_reg_el_server_cb(prov_srv_cb_t cb);

#endif /* CONFIG_ENABLE_PROVISIONING */
#endif /* __MESHX_PROV__ */
