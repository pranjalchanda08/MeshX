/**
 * @file gpio_constraints.h
 * @brief GPIO Constraints for Template BSP
 *
 * This file defines hardware constraints and limitations for GPIO on the template BSP.
 * Each BSP should provide its own constraints based on hardware capabilities.
 *
 * @author MeshX Team
 * @date 2024
 */

#ifndef __GPIO_CONSTRAINTS_H
#define __GPIO_CONSTRAINTS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum PWM frequency (Hz)
 */
#define GPIO_PWM_MAX_FREQUENCY 40000

/**
 * @brief Minimum PWM frequency (Hz)
 */
#define GPIO_PWM_MIN_FREQUENCY 1

/**
 * @brief Maximum PWM resolution (bits)
 */
#define GPIO_PWM_MAX_RESOLUTION 16

/**
 * @brief Minimum PWM resolution (bits)
 */
#define GPIO_PWM_MIN_RESOLUTION 1

/**
 * @brief Maximum number of PWM channels
 */
#define GPIO_PWM_MAX_CHANNELS 8

/**
 * @brief Maximum interrupt priority
 */
#define GPIO_INTR_MAX_PRIORITY 10

/**
 * @brief Minimum interrupt priority
 */
#define GPIO_INTR_MIN_PRIORITY 1

/**
 * @brief Maximum interrupt stack size (bytes)
 */
#define GPIO_INTR_MAX_STACK_SIZE 8192

/**
 * @brief Minimum interrupt stack size (bytes)
 */
#define GPIO_INTR_MIN_STACK_SIZE 1024

/**
 * @brief Check if PWM frequency is valid
 *
 * @param frequency PWM frequency in Hz
 * @return true if frequency is valid
 * @return false if frequency is invalid
 */
bool gpio_is_pwm_frequency_valid(uint32_t frequency);

/**
 * @brief Check if PWM resolution is valid
 *
 * @param resolution PWM resolution in bits
 * @return true if resolution is valid
 * @return false if resolution is invalid
 */
bool gpio_is_pwm_resolution_valid(uint8_t resolution);

/**
 * @brief Check if PWM duty cycle is valid
 *
 * @param duty_cycle PWM duty cycle (0-100%)
 * @return true if duty cycle is valid
 * @return false if duty cycle is invalid
 */
bool gpio_is_pwm_duty_cycle_valid(uint8_t duty_cycle);

/**
 * @brief Check if PWM channel is available
 *
 * @param channel PWM channel number
 * @return true if channel is available
 * @return false if channel is not available
 */
bool gpio_is_pwm_channel_available(uint8_t channel);

/**
 * @brief Allocate PWM channel
 *
 * @param channel PWM channel number to allocate
 * @return true if allocation successful
 * @return false if allocation failed (channel already allocated)
 */
bool gpio_allocate_pwm_channel(uint8_t channel);

/**
 * @brief Release PWM channel
 *
 * @param channel PWM channel number to release
 */
void gpio_release_pwm_channel(uint8_t channel);

/**
 * @brief Check if interrupt priority is valid
 *
 * @param priority Interrupt priority
 * @return true if priority is valid
 * @return false if priority is invalid
 */
bool gpio_is_intr_priority_valid(uint8_t priority);

/**
 * @brief Check if interrupt stack size is valid
 *
 * @param stack_size Interrupt stack size in bytes
 * @return true if stack size is valid
 * @return false if stack size is invalid
 */
bool gpio_is_intr_stack_size_valid(uint16_t stack_size);

/**
 * @brief Get maximum drive strength for pin
 *
 * @param physical_pin Physical pin number
 * @return uint8_t Maximum drive strength (0-3)
 */
uint8_t gpio_get_max_drive_strength(uint8_t physical_pin);

/**
 * @brief Check if pull resistor configuration is valid for pin
 *
 * @param physical_pin Physical pin number
 * @param pull Pull resistor setting (0-3)
 * @return true if pull configuration is valid
 * @return false if pull configuration is invalid
 */
bool gpio_is_pull_config_valid(uint8_t physical_pin, uint8_t pull);

/**
 * @brief Check if mode is supported for pin
 *
 * @param physical_pin Physical pin number
 * @param mode GPIO mode (0-5)
 * @return true if mode is supported
 * @return false if mode is not supported
 */
bool gpio_is_mode_supported(uint8_t physical_pin, uint8_t mode);

#ifdef __cplusplus
}
#endif

#endif /* __GPIO_CONSTRAINTS_H */
