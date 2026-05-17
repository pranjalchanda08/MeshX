/**
 * @file meshx_platform.h
 * @brief Platform abstraction layer for MeshX.
 *
 * This header provides initialization functions for the MeshX platform
 * and its Bluetooth subsystem.
 */

#ifndef __MESHX_PLATFORM_H__
#define __MESHX_PLATFORM_H__

#include <stdbool.h>
#include <stdint.h>
#include "meshx_err.h"
#include "meshx_platform_ble_mesh.h"
#include "interface/ble_mesh/meshx_ble_mesh_cmn_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Console channel interface types.
 */
typedef enum {
    MESHX_PLATFORM_CONSOLE_CHANNEL_UART,     /**< Standard physical logging UART */
    MESHX_PLATFORM_CONSOLE_CHANNEL_USB_CDC   /**< Native USB-to-UART or JTAG/CDC Bridge */
} meshx_platform_console_channel_t;

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

/**
 * @brief Writes data to the console interface.
 *
 * @param[in] data Pointer to the data buffer.
 * @param[in] len Length of the data to write.
 */
void meshx_platform_console_write(const char *data, uint16_t len);

/**
 * @brief Reads data from the console interface.
 *
 * @param[out] data Pointer to the data buffer.
 * @param[in] len Length of the data to read.
 * @return int32_t Number of bytes read, or negative error code.
 */
int32_t meshx_platform_console_read(uint8_t *data, uint16_t len);

/**
 * @brief Initializes the console interface.
 *
 * @return meshx_err_t Returns MESHX_OK on success, or an appropriate error code.
 */
meshx_err_t meshx_platform_console_init(void);

/**
 * @brief Expose console detection capabilities.
 * @return Active console channel type configuration.
 */
meshx_platform_console_channel_t meshx_platform_get_console_channel(void);

/**
 * @brief Get the current dynamic MXSP routing target.
 * @return True if MXSP is currently multiplexed over console channel, False if routed to UART1.
 */
bool meshx_platform_get_mxsp_use_console(void);

/**
 * @brief Set the dynamic MXSP routing target.
 * @param[in] enable Set to true to route MXSP over active console log channel.
 */
void meshx_platform_set_mxsp_use_console(bool enable);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_PLATFORM_H__ */
