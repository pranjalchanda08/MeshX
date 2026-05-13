/**
 * @file meshx_gpio.h
 * @brief MeshX GPIO Interface
 *
 * This file defines the MeshX GPIO interface for platform-abstracted GPIO operations.
 * The interface supports digital I/O, interrupts, and function-based API for extensibility.
 *
 * @author MeshX Team
 * @date 2024
 */

#ifndef __MESHX_GPIO_H
#define __MESHX_GPIO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../../inc/meshx_err.h"
#include "meshx_gpio_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief GPIO Pin Modes
 */
typedef enum {
    MESHX_GPIO_MODE_INPUT = 0,              /**< Input only mode */
    MESHX_GPIO_MODE_OUTPUT,                 /**< Output only mode */
    MESHX_GPIO_MODE_INPUT_OUTPUT,           /**< Input and output mode */
    MESHX_GPIO_MODE_OPEN_DRAIN,             /**< Open drain output mode */
    MESHX_GPIO_MODE_OPEN_DRAIN_INPUT_OUTPUT,/**< Open drain input/output mode */
    MESHX_GPIO_MODE_PWM_OUTPUT,             /**< PWM output mode */
    MESHX_GPIO_MODE_MAX                     /**< Maximum mode value */
} meshx_gpio_mode_t;

/**
 * @brief GPIO Pull Resistor Settings
 */
typedef enum {
    MESHX_GPIO_PULL_NONE = 0,               /**< No pull resistor */
    MESHX_GPIO_PULL_UP,                     /**< Pull-up resistor */
    MESHX_GPIO_PULL_DOWN,                   /**< Pull-down resistor */
    MESHX_GPIO_PULL_UP_DOWN,                /**< Both pull-up and pull-down */
    MESHX_GPIO_PULL_MAX                     /**< Maximum pull value */
} meshx_gpio_pull_t;

/**
 * @brief GPIO Drive Strength
 */
typedef enum {
    MESHX_GPIO_DRIVE_WEAK = 0,              /**< Weak drive strength */
    MESHX_GPIO_DRIVE_MEDIUM,                /**< Medium drive strength */
    MESHX_GPIO_DRIVE_STRONG,                /**< Strong drive strength */
    MESHX_GPIO_DRIVE_MAX_STRONG,            /**< Maximum strong drive strength */
    MESHX_GPIO_DRIVE_MAX                    /**< Maximum drive value */
} meshx_gpio_drive_t;

/**
 * @brief IO Function Types for function-based API
 */
typedef enum {
    MESHX_IO_FUNCTION_SET_LEVEL = 0,        /**< Set pin level function */
    MESHX_IO_FUNCTION_GET_LEVEL,            /**< Get pin level function */
    MESHX_IO_FUNCTION_TOGGLE,               /**< Toggle pin function */
    MESHX_IO_FUNCTION_SET_PWM_DUTY,         /**< Set PWM duty cycle function */
    MESHX_IO_FUNCTION_SET_PWM_FREQUENCY,    /**< Set PWM frequency function */
    MESHX_IO_FUNCTION_REGISTER_INTERRUPT,   /**< Register interrupt function */
    MESHX_IO_FUNCTION_CUSTOM,               /**< Custom function (for extensibility) */
    MESHX_IO_FUNCTION_MAX                   /**< Maximum function type */
} meshx_io_function_t;

/**
 * @brief GPIO Interrupt Callback Function
 *
 * @param logical_pin Logical pin number (0-255)
 * @param user_data User data passed during registration
 */
typedef void (*meshx_gpio_intr_cb_t)(uint8_t logical_pin, void *user_data);

/**
 * @brief Initialize GPIO subsystem
 *
 * This function initializes the GPIO subsystem and configures all pins
 * according to the compiled configuration.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_init(void);

/**
 * @brief Deinitialize GPIO subsystem
 *
 * This function deinitializes the GPIO subsystem and releases all resources.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_deinit(void);

/**
 * @brief Set GPIO pin level
 *
 * @param logical_pin Logical pin number (0-255)
 * @param level Pin level (0 = low, 1 = high)
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_set_level(uint8_t logical_pin, uint8_t level);

/**
 * @brief Get GPIO pin level
 *
 * @param logical_pin Logical pin number (0-255)
 * @param[out] level Pointer to store pin level
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_get_level(uint8_t logical_pin, uint8_t *level);

/**
 * @brief Toggle GPIO pin level
 *
 * @param logical_pin Logical pin number (0-255)
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_toggle(uint8_t logical_pin);

/**
 * @brief Execute IO function on GPIO pin
 *
 * This is the function-based API for extensible GPIO operations.
 *
 * @param logical_pin Logical pin number (0-255)
 * @param function IO function to execute
 * @param args Function arguments vector
 * @param arg_count Number of arguments in the vector
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_execute_function(uint8_t logical_pin,
                                        meshx_io_function_t function,
                                        const uint32_t *args,
                                        uint8_t arg_count);

/**
 * @brief Register GPIO interrupt handler
 *
 * @param logical_pin Logical pin number (0-255)
 * @param intr_type Interrupt trigger type
 * @param callback Interrupt callback function
 * @param user_data User data passed to callback
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_register_intr(uint8_t logical_pin,
                                     meshx_gpio_intr_type_t intr_type,
                                     meshx_gpio_intr_cb_t callback,
                                     void *user_data);

/**
 * @brief Unregister GPIO interrupt handler
 *
 * @param logical_pin Logical pin number (0-255)
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_unregister_intr(uint8_t logical_pin);

/**
 * @brief Enable/disable GPIO interrupt
 *
 * @param logical_pin Logical pin number (0-255)
 * @param enable true to enable, false to disable
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_intr_enable(uint8_t logical_pin, bool enable);

/**
 * @brief Set hosted mode for GPIO subsystem
 *
 * This function switches the GPIO subsystem between hosted and non-hosted modes.
 * In hosted mode, GPIO events are serialized and sent via UART transport.
 * In non-hosted mode, GPIO operations directly control hardware.
 *
 * @param mode Hosted mode setting (MESHX_GPIO_MODE_HOSTED or MESHX_GPIO_MODE_NON_HOSTED)
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_set_hosted_mode(meshx_gpio_hosted_mode_t mode);

/**
 * @brief Get current hosted mode
 *
 * @return meshx_gpio_hosted_mode_t Current hosted mode
 */
meshx_gpio_hosted_mode_t meshx_gpio_get_hosted_mode(void);

/**
 * @brief Check if GPIO subsystem is in hosted mode
 *
 * @return true if in hosted mode, false otherwise
 */
bool meshx_gpio_is_hosted_mode(void);

/**
 * @brief Register callback for GPIO hosted mode events
 *
 * This callback is invoked when GPIO events need to be sent to the host MCU
 * in hosted mode.
 *
 * @param callback Callback function for hosted mode events
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
typedef void (*meshx_gpio_hosted_event_cb_t)(const meshx_gpio_hosted_event_t *event);
meshx_err_t meshx_gpio_register_hosted_event_cb(meshx_gpio_hosted_event_cb_t callback);

/**
 * @brief Process GPIO interrupt event from host (hosted mode)
 *
 * This function is called when the host sends an interrupt notification
 * for a GPIO pin that was registered for interrupts.
 *
 * @param logical_pin Logical pin number
 * @param value Interrupt value (trigger type or pin state)
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_process_hosted_interrupt(uint8_t logical_pin, uint8_t value);

/**
 * @brief Check if GPIO subsystem is initialized
 *
 * @return true if initialized, false otherwise
 */
bool meshx_gpio_is_initialized(void);

/**
 * @brief Get number of configured GPIO pins
 *
 * @return uint8_t Number of configured pins
 */
uint8_t meshx_gpio_get_pin_count(void);

/**
 * @brief Get pin configuration
 *
 * @param logical_pin Logical pin number
 * @param[out] config Pointer to store pin configuration
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_get_pin_config(uint8_t logical_pin, meshx_gpio_pin_config_t *config);

/**
 * @brief Get pin runtime state
 *
 * @param logical_pin Logical pin number
 * @param[out] state Pointer to store pin state
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_get_pin_state(uint8_t logical_pin, meshx_gpio_pin_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_GPIO_H */
