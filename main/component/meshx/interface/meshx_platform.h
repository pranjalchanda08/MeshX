/**
 * @file meshx_platform.h
 * @brief Platform abstraction layer for MeshX.
 *
 * This header provides initialization functions for the MeshX platform
 * and its Bluetooth subsystem.
 */

#ifndef __MESHX_PLATFORM_H__
#define __MESHX_PLATFORM_H__

#include "meshx_err.h"
#include "meshx_platform_ble_mesh.h"
#include "interface/ble_mesh/meshx_ble_mesh_cmn_def.h"

/**
 * @brief Initializes the MeshX platform.
 *
 * This function sets up the necessary hardware and software components
 * required for the MeshX platform to function correctly.
 *
 * @return meshx_err_t Returns MESHX_OK on success, or an appropriate error code.
 */
meshx_err_t meshx_platform_init(void);

/**
 * @brief Resets the MeshX platform.
 * This function performs a system reset, restarting the platform.
 */

__attribute__((noreturn)) void meshx_platform_reset(void);

#endif /* __MESHX_PLATFORM_H__ */
