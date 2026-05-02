/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_config_server.h
 * @brief Header file for the meshxuction configuration server model.
 *
 * This file contains the definitions and function declarations for the
 * meshxuction configuration server model used in the ESP32 BLE Mesh Node.
 *
 * @author Pranjal Chanda
 */

#ifndef __MESHX_CONFIG_SERVER__
#define __MESHX_CONFIG_SERVER__

#include "interface/meshx_platform.h"
#include "interface/ble_mesh/server/meshx_ble_mesh_config_srv.h"
#include "meshx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_ENABLE_CONFIG_SERVER

/**
 * @brief Retrieves the configuration server model for the MeshX framework.
 *
 * @param[out] p_model Pointer to a variable where the address of the
 *                     configuration server model will be stored.
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
meshx_err_t meshx_get_config_srv_model(void* p_model);

/**
 * @brief Initialize the meshxuction configuration server.
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
meshx_err_t meshx_init_config_server(void);

/**
 * @brief Registers a configuration server callback for specific events.
 *
 * @param[in] cb               Callback function to register.
 * @param[in] config_evt_bmap  Bitmap of events the callback is interested in.
 * @return MESHX_SUCCESS on success, an error code otherwise.
 */
meshx_err_t meshx_config_server_cb_reg(config_srv_cb_t cb, uint32_t config_evt_bmap);

/**
 * @brief Retrieves the instance of the MeshX configuration server.
 *
 * @param[out] p_conf_srv Pointer to receive the configuration server instance.
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
meshx_err_t meshx_get_config_srv_instance(void** p_conf_srv);

#endif /* CONFIG_ENABLE_CONFIG_SERVER */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __MESHX_CONFIG_SERVER__ */
