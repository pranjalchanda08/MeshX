/**
 * @file meshx_pwm.h
 * @brief MeshX PWM Interface
 *
 * This file defines the MeshX PWM interface for platform-abstracted PWM operations.
 *
 * @author MeshX Team
 * @date 2024
 */

#ifndef __MESHX_PWM_H
#define __MESHX_PWM_H

#include <stdint.h>
#include "../../inc/meshx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize PWM subsystem
 *
 * This function initializes the PWM subsystem based on YAML configuration.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_pwm_init(void);

/**
 * @brief Deinitialize PWM subsystem
 *
 * This function deinitializes the PWM subsystem and releases all resources.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_pwm_deinit(void);

/**
 * @brief Start PWM output on pin
 *
 * @param logical_pin Logical pin number (0-255)
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_pwm_start(uint8_t logical_pin);

/**
 * @brief Stop PWM output on pin
 *
 * @param logical_pin Logical pin number (0-255)
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_pwm_stop(uint8_t logical_pin);

/**
 * @brief Set PWM duty cycle
 *
 * @param logical_pin Logical pin number (0-255)
 * @param duty_cycle Duty cycle (0-100%)
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_pwm_set_duty_cycle(uint8_t logical_pin, uint8_t duty_cycle);

/**
 * @brief Get PWM duty cycle
 *
 * @param logical_pin Logical pin number (0-255)
 * @param[out] duty_cycle Pointer to store duty cycle
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_pwm_get_duty_cycle(uint8_t logical_pin, uint8_t *duty_cycle);

/**
 * @brief Set PWM frequency
 *
 * @param logical_pin Logical pin number (0-255)
 * @param frequency Frequency in Hz
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_pwm_set_frequency(uint8_t logical_pin, uint32_t frequency);

/**
 * @brief Get PWM frequency
 *
 * @param logical_pin Logical pin number (0-255)
 * @param[out] frequency Pointer to store frequency
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_pwm_get_frequency(uint8_t logical_pin, uint32_t *frequency);

/**
 * @brief Set PWM resolution
 *
 * @param logical_pin Logical pin number (0-255)
 * @param resolution Resolution in bits (typically 8-16)
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_pwm_set_resolution(uint8_t logical_pin, uint8_t resolution);

/**
 * @brief Get PWM resolution
 *
 * @param logical_pin Logical pin number (0-255)
 * @param[out] resolution Pointer to store resolution
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_pwm_get_resolution(uint8_t logical_pin, uint8_t *resolution);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_PWM_H */
