/**
 * @file meshx_io_platform.h
 * @brief MeshX IO Platform Interface
 *
 * This file defines the platform-specific interface for IO implementations.
 *
 * @author MeshX Team
 * @date 2024
 */

#ifndef __MESHX_IO_PLATFORM_H
#define __MESHX_IO_PLATFORM_H

#include "meshx_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize platform IO subsystem
 *
 * @return meshx_err_t Error code
 */
meshx_err_t meshx_io_platform_init(void);

/**
 * @brief Deinitialize platform IO subsystem
 *
 * @return meshx_err_t Error code
 */
meshx_err_t meshx_io_platform_deinit(void);

/**
 * @brief Create platform-specific GPIO instance
 *
 * @param logical_pin Logical pin number
 * @param name Pin name
 * @param mode GPIO mode
 * @param pull Pull resistor setting
 * @param drive Drive strength
 * @param initial_level Initial output level
 * @param signal_inversion Signal inversion flag
 * @return void* Platform-specific GPIO instance handle
 */
void* meshx_io_platform_create_gpio(uint8_t logical_pin,
                                    const char* name,
                                    uint8_t mode,
                                    uint8_t pull,
                                    uint8_t drive,
                                    uint8_t initial_level,
                                    bool signal_inversion);

/**
 * @brief Create platform-specific PWM instance
 *
 * @param logical_pin Logical pin number
 * @param name Pin name
 * @param frequency PWM frequency
 * @param duty_cycle Initial duty cycle
 * @param resolution PWM resolution
 * @param channel Hardware channel
 * @return void* Platform-specific PWM instance handle
 */
void* meshx_io_platform_create_pwm(uint8_t logical_pin,
                                   const char* name,
                                   uint32_t frequency,
                                   uint8_t duty_cycle,
                                   uint8_t resolution,
                                   uint8_t channel);

/**
 * @brief Destroy platform-specific IO instance
 *
 * @param handle Platform-specific IO instance handle
 */
void meshx_io_platform_destroy(void* handle);

/**
 * @brief Execute platform-specific IO function
 *
 * @param handle Platform-specific IO instance handle
 * @param function IO function to execute
 * @param args Function arguments
 * @param arg_count Number of arguments
 * @return meshx_err_t Error code
 */
meshx_err_t meshx_io_platform_execute(void* handle,
                                      uint8_t function,
                                      const uint32_t* args,
                                      uint8_t arg_count);

/**
 * @brief Set hosted mode
 *
 * @param mode Hosted mode (0 = non-hosted, 1 = hosted)
 * @return meshx_err_t Error code
 */
meshx_err_t meshx_io_platform_set_hosted_mode(uint8_t mode);

/**
 * @brief Get current hosted mode
 *
 * @return uint8_t Current hosted mode
 */
uint8_t meshx_io_platform_get_hosted_mode(void);

/**
 * @brief Send hosted mode event
 *
 * @param event_type Event type
 * @param logical_pin Logical pin number
 * @param value Event value
 * @return meshx_err_t Error code
 */
meshx_err_t meshx_io_platform_send_hosted_event(uint8_t event_type,
                                                uint8_t logical_pin,
                                                uint8_t value);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_IO_PLATFORM_H */
