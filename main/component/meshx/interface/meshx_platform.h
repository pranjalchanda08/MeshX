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
 
#ifdef __cplusplus
extern "C" {
#endif

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

/**
 * @brief Writes data to the serial interface.
 *
 * @param[in] data Pointer to the data buffer.
 * @param[in] len Length of the data to write.
 */
void meshx_platform_serial_write(const uint8_t *data, uint16_t len);

/**
 * @brief Initializes the serial interface for communication.
 * 
 * @return meshx_err_t Returns MESHX_OK on success, or an appropriate error code.
 */
meshx_err_t meshx_platform_serial_init(void);

/**
 * @brief Reads data from the serial interface.
 * 
 * @param[out] data Pointer to the data buffer.
 * @param[in] len Length of the data to read.
 * @return int32_t Number of bytes read, or negative error code.
 */
int32_t meshx_platform_serial_read(uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif
 
#endif /* __MESHX_PLATFORM_H__ */
