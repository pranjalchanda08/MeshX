/**
 * @file meshx_platform.h
 * @brief Platform abstraction layer for MeshX.
 *
 * This header provides initialization functions for the MeshX platform
 * and its Bluetooth subsystem.
 */

#ifndef __MESHX_PLATFORM_H_
#define __MESHX_PLATFORM_H_

#include "meshx_err.h"
#include "meshx_platform_ble_mesh.h"

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
 * @brief Initializes the Bluetooth subsystem of the MeshX platform.
 *
 * This function sets up the Bluetooth-related components necessary for
 * MeshX operation, such as BLE Mesh provisioning and communication.
 *
 * @return meshx_err_t Returns MESHX_OK on success, or an appropriate error code.
 */
meshx_err_t meshx_platform_bt_init(void);

#endif /* __MESHX_PLATFORM_H_ */
