/**
 * @file meshx_gpio_platform.h
 * @brief MeshX GPIO Platform Interface
 *
 * This file defines the platform-specific interface for GPIO implementations.
 * BSP implementations must provide these functions for GPIO operations.
 *
 * @author MeshX Team
 * @date 2024
 */

#ifndef __MESHX_GPIO_PLATFORM_H
#define __MESHX_GPIO_PLATFORM_H

#include "meshx_gpio.h"
#include "meshx_pwm.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Platform-specific GPIO initialization
 *
 * This function initializes the platform-specific GPIO hardware.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_platform_init(void);

/**
 * @brief Platform-specific GPIO deinitialization
 *
 * This function deinitializes the platform-specific GPIO hardware.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_platform_deinit(void);

/**
 * @brief Platform-specific pin level set
 *
 * @param physical_pin Physical pin number (platform-specific)
 * @param level Pin level (0 = low, 1 = high)
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_platform_set_level(uint8_t physical_pin, uint8_t level);

/**
 * @brief Platform-specific pin level get
 *
 * @param physical_pin Physical pin number (platform-specific)
 * @param[out] level Pointer to store pin level
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_platform_get_level(uint8_t physical_pin, uint8_t *level);

/**
 * @brief Platform-specific pin mode configuration
 *
 * @param physical_pin Physical pin number (platform-specific)
 * @param mode Pin mode (meshx_gpio_mode_t)
 * @param pull Pull resistor setting (meshx_gpio_pull_t)
 * @param drive Drive strength (meshx_gpio_drive_t)
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_platform_configure_pin(uint8_t physical_pin,
                                              meshx_gpio_mode_t mode,
                                              meshx_gpio_pull_t pull,
                                              meshx_gpio_drive_t drive);

/**
 * @brief Platform-specific interrupt registration
 *
 * @param physical_pin Physical pin number (platform-specific)
 * @param intr_type Interrupt trigger type
 * @param isr_handler Interrupt service routine handler
 * @param arg Argument passed to ISR handler
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_platform_register_intr(uint8_t physical_pin,
                                              meshx_gpio_intr_type_t intr_type,
                                              void (*isr_handler)(void*),
                                              void *arg);

/**
 * @brief Platform-specific interrupt unregistration
 *
 * @param physical_pin Physical pin number (platform-specific)
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_platform_unregister_intr(uint8_t physical_pin);

/**
 * @brief Platform-specific interrupt enable/disable
 *
 * @param physical_pin Physical pin number (platform-specific)
 * @param enable true to enable, false to disable
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_platform_intr_enable(uint8_t physical_pin, bool enable);

/**
 * @brief Platform-specific PWM initialization
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_pwm_platform_init(void);

/**
 * @brief Platform-specific PWM deinitialization
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_pwm_platform_deinit(void);

/**
 * @brief Platform-specific PWM start
 *
 * @param physical_pin Physical pin number (platform-specific)
 * @param frequency PWM frequency in Hz
 * @param duty_cycle Duty cycle (0-100%)
 * @param resolution PWM resolution in bits
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_pwm_platform_start(uint8_t physical_pin,
                                     uint32_t frequency,
                                     uint8_t duty_cycle,
                                     uint8_t resolution);

/**
 * @brief Platform-specific PWM stop
 *
 * @param physical_pin Physical pin number (platform-specific)
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_pwm_platform_stop(uint8_t physical_pin);

/**
 * @brief Platform-specific PWM duty cycle set
 *
 * @param physical_pin Physical pin number (platform-specific)
 * @param duty_cycle Duty cycle (0-100%)
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_pwm_platform_set_duty_cycle(uint8_t physical_pin, uint8_t duty_cycle);

/**
 * @brief Platform-specific PWM frequency set
 *
 * @param physical_pin Physical pin number (platform-specific)
 * @param frequency Frequency in Hz
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_pwm_platform_set_frequency(uint8_t physical_pin, uint32_t frequency);

/**
 * @brief Platform-specific IO function execution
 *
 * This is the function-based API for extensible IO operations.
 * BSP implementations must support the standard IO functions and
 * can optionally support custom functions.
 *
 * @param physical_pin Physical pin number (platform-specific)
 * @param function IO function to execute (meshx_io_function_t)
 * @param args Function arguments vector
 * @param arg_count Number of arguments in the vector
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_io_platform_execute_function(uint8_t physical_pin,
                                               meshx_io_function_t function,
                                               const uint32_t *args,
                                               uint8_t arg_count);

/**
 * @brief Platform-specific custom function registration
 *
 * BSP implementations can use this to register custom IO functions
 * that are not part of the standard IO function set.
 *
 * @param function_id Custom function ID
 * @param handler Function handler
 * @param user_data User data for the handler
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_io_platform_register_custom_function(uint16_t function_id,
                                                       meshx_err_t (*handler)(uint8_t physical_pin,
                                                                              const uint32_t *args,
                                                                              uint8_t arg_count,
                                                                              void *user_data),
                                                       void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_GPIO_PLATFORM_H */
