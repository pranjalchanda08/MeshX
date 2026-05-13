/**
 * @file gpio_constraints.h
 * @brief GPIO Constraints for Seeed XIAO ESP32-C3 BSP
 *
 * This file defines board-specific constraints and limitations for GPIO operations
 * on the Seeed XIAO ESP32-C3.
 *
 * @author MeshX Team
 * @date 2024
 */

#ifndef __XIAO_C3_GPIO_CONSTRAINTS_H
#define __XIAO_C3_GPIO_CONSTRAINTS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum number of PWM channels available
 *
 * ESP32-C3 has 6 LEDC channels (4 high-speed, 2 low-speed)
 */
#define XIAO_C3_PWM_MAX_CHANNELS 6

/**
 * @brief Maximum number of simultaneous PWM outputs
 *
 * Limited by available GPIO pins and PWM channels
 */
#define XIAO_C3_PWM_MAX_SIMULTANEOUS_OUTPUTS 6

/**
 * @brief PWM frequency limits (in Hz)
 */
#define XIAO_C3_PWM_MIN_FREQUENCY 1
#define XIAO_C3_PWM_MAX_FREQUENCY 40000000  // 40MHz theoretical max

/**
 * @brief PWM duty cycle resolution limits (in bits)
 */
#define XIAO_C3_PWM_MIN_RESOLUTION 1
#define XIAO_C3_PWM_MAX_RESOLUTION 16

/**
 * @brief Maximum number of simultaneous interrupts
 *
 * ESP32-C3 supports interrupts on all GPIO pins
 */
#define XIAO_C3_INTERRUPT_MAX_SIMULTANEOUS 11  // All 11 GPIO pins

/**
 * @brief Interrupt task stack size (in bytes)
 */
#define XIAO_C3_INTERRUPT_TASK_STACK_SIZE 2048

/**
 * @brief Interrupt task priority
 *
 * Higher number = higher priority (FreeRTOS convention)
 */
#define XIAO_C3_INTERRUPT_TASK_PRIORITY 5

/**
 * @brief GPIO drive strength options
 */
typedef enum {
    XIAO_C3_GPIO_DRIVE_STRENGTH_5MA = 0,   /**< 5mA drive strength */
    XIAO_C3_GPIO_DRIVE_STRENGTH_10MA,      /**< 10mA drive strength */
    XIAO_C3_GPIO_DRIVE_STRENGTH_20MA,      /**< 20mA drive strength */
    XIAO_C3_GPIO_DRIVE_STRENGTH_40MA,      /**< 40mA drive strength (max) */
    XIAO_C3_GPIO_DRIVE_STRENGTH_MAX
} xiao_c3_gpio_drive_strength_t;

/**
 * @brief GPIO sleep mode behavior
 */
typedef enum {
    XIAO_C3_GPIO_SLEEP_HOLD = 0,           /**< Hold pin state during sleep */
    XIAO_C3_GPIO_SLEEP_RELEASE,            /**< Release pin during sleep */
    XIAO_C3_GPIO_SLEEP_MAX
} xiao_c3_gpio_sleep_mode_t;

/**
 * @brief Pin usage conflicts
 *
 * These pins have special functions that may conflict with GPIO usage
 */
typedef struct {
    uint8_t pin;                    /**< Pin number */
    const char* conflict;           /**< Conflict description */
    const char* recommendation;     /**< Usage recommendation */
} xiao_c3_pin_conflict_t;

/**
 * @brief XIAO ESP32-C3 pin conflicts
 */
/**
 * @brief Number of pin conflicts
 */
#define XIAO_C3_PIN_CONFLICTS_COUNT (sizeof(xiao_c3_pin_conflicts) / sizeof(xiao_c3_pin_conflicts[0]))

extern const xiao_c3_pin_conflict_t xiao_c3_pin_conflicts[];

/**
 * @brief Check if pin has conflicts
 *
 * @param physical_pin Physical pin number
 * @return true if pin has conflicts
 * @return false if pin has no conflicts
 */
bool xiao_c3_gpio_has_conflict(uint8_t physical_pin);

/**
 * @brief Get pin conflict description
 *
 * @param physical_pin Physical pin number
 * @return const char* Conflict description, NULL if no conflict
 */
const char* xiao_c3_gpio_get_conflict(uint8_t physical_pin);

/**
 * @brief Get pin usage recommendation
 *
 * @param physical_pin Physical pin number
 * @return const char* Usage recommendation, NULL if no conflict
 */
const char* xiao_c3_gpio_get_recommendation(uint8_t physical_pin);

/**
 * @brief Check if PWM frequency is within limits
 *
 * @param frequency Frequency in Hz
 * @return true if frequency is valid
 * @return false if frequency is invalid
 */
bool xiao_c3_pwm_is_frequency_valid(uint32_t frequency);

/**
 * @brief Check if PWM duty cycle is valid
 *
 * @param duty_cycle Duty cycle (0-100%)
 * @return true if duty cycle is valid
 * @return false if duty cycle is invalid
 */
bool xiao_c3_pwm_is_duty_cycle_valid(uint8_t duty_cycle);

/**
 * @brief Check if PWM resolution is valid
 *
 * @param resolution Resolution in bits
 * @return true if resolution is valid
 * @return false if resolution is invalid
 */
bool xiao_c3_pwm_is_resolution_valid(uint8_t resolution);

/**
 * @brief Get recommended PWM frequency for given resolution
 *
 * @param resolution Resolution in bits
 * @return uint32_t Recommended frequency in Hz
 */
uint32_t xiao_c3_pwm_get_recommended_frequency(uint8_t resolution);

/**
 * @brief Get maximum number of PWM channels available
 *
 * @return uint8_t Number of available PWM channels
 */
uint8_t xiao_c3_pwm_get_available_channels(void);

/**
 * @brief Check if interrupt is supported on pin
 *
 * @param physical_pin Physical pin number
 * @return true if interrupt is supported
 * @return false if interrupt is not supported
 */
bool xiao_c3_gpio_is_interrupt_supported(uint8_t physical_pin);

/**
 * @brief Get recommended interrupt task stack size
 *
 * @return uint16_t Recommended stack size in bytes
 */
uint16_t xiao_c3_gpio_get_recommended_interrupt_stack_size(void);

/**
 * @brief Get recommended interrupt task priority
 *
 * @return uint8_t Recommended task priority
 */
uint8_t xiao_c3_gpio_get_recommended_interrupt_priority(void);

#ifdef __cplusplus
}
#endif

#endif /* __XIAO_C3_GPIO_CONSTRAINTS_H */
