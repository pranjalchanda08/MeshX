/**
 * @file gpio_constraints.h
 * @brief GPIO Constraints for ESP32 WROOM DevKit BSP
 *
 * This file defines board-specific constraints and limitations for GPIO operations
 * on the ESP32 WROOM DevKit.
 *
 * @author MeshX Team
 * @date 2024
 */

#ifndef __ESP32_DEVKITC_GPIO_CONSTRAINTS_H
#define __ESP32_DEVKITC_GPIO_CONSTRAINTS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum number of PWM channels available
 *
 * ESP32 has 16 LEDC channels (8 high-speed, 8 low-speed)
 */
#define ESP32_DEVKITC_PWM_MAX_CHANNELS 16

/**
 * @brief Maximum number of simultaneous PWM outputs
 *
 * Limited by available GPIO pins and PWM channels
 */
#define ESP32_DEVKITC_PWM_MAX_SIMULTANEOUS_OUTPUTS 16

/**
 * @brief PWM frequency limits (in Hz)
 */
#define ESP32_DEVKITC_PWM_MIN_FREQUENCY 1
#define ESP32_DEVKITC_PWM_MAX_FREQUENCY 40000000  // 40MHz theoretical max

/**
 * @brief PWM duty cycle resolution limits (in bits)
 */
#define ESP32_DEVKITC_PWM_MIN_RESOLUTION 1
#define ESP32_DEVKITC_PWM_MAX_RESOLUTION 20  // ESP32 supports up to 20-bit resolution

/**
 * @brief Maximum number of simultaneous interrupts
 *
 * ESP32 supports interrupts on all GPIO pins
 */
#define ESP32_DEVKITC_INTERRUPT_MAX_SIMULTANEOUS 34  // All 34 GPIO pins

/**
 * @brief Interrupt task stack size (in bytes)
 */
#define ESP32_DEVKITC_INTERRUPT_TASK_STACK_SIZE 2048

/**
 * @brief Interrupt task priority
 *
 * Higher number = higher priority (FreeRTOS convention)
 */
#define ESP32_DEVKITC_INTERRUPT_TASK_PRIORITY 5

/**
 * @brief GPIO drive strength options
 */
typedef enum {
    ESP32_DEVKITC_GPIO_DRIVE_STRENGTH_5MA = 0,   /**< 5mA drive strength */
    ESP32_DEVKITC_GPIO_DRIVE_STRENGTH_10MA,      /**< 10mA drive strength */
    ESP32_DEVKITC_GPIO_DRIVE_STRENGTH_20MA,      /**< 20mA drive strength */
    ESP32_DEVKITC_GPIO_DRIVE_STRENGTH_40MA,      /**< 40mA drive strength (max) */
    ESP32_DEVKITC_GPIO_DRIVE_STRENGTH_MAX
} esp32_devkitc_gpio_drive_strength_t;

/**
 * @brief GPIO sleep mode behavior
 */
typedef enum {
    ESP32_DEVKITC_GPIO_SLEEP_HOLD = 0,           /**< Hold pin state during sleep */
    ESP32_DEVKITC_GPIO_SLEEP_RELEASE,            /**< Release pin during sleep */
    ESP32_DEVKITC_GPIO_SLEEP_MAX
} esp32_devkitc_gpio_sleep_mode_t;

/**
 * @brief Pin usage conflicts
 *
 * These pins have special functions that may conflict with GPIO usage
 */
typedef struct {
    uint8_t pin;                    /**< Pin number */
    const char* conflict;           /**< Conflict description */
    const char* recommendation;     /**< Usage recommendation */
} esp32_devkitc_pin_conflict_t;

/**
 * @brief ESP32 WROOM DevKit pin conflicts
 */
/**
 * @brief Number of pin conflicts
 */
#define ESP32_DEVKITC_PIN_CONFLICTS_COUNT (sizeof(esp32_devkitc_pin_conflicts) / sizeof(esp32_devkitc_pin_conflicts[0]))

extern const esp32_devkitc_pin_conflict_t esp32_devkitc_pin_conflicts[];

/**
 * @brief Check if pin has conflicts
 *
 * @param physical_pin Physical pin number
 * @return true if pin has conflicts
 * @return false if pin has no conflicts
 */
bool esp32_devkitc_gpio_has_conflict(uint8_t physical_pin);

/**
 * @brief Get pin conflict description
 *
 * @param physical_pin Physical pin number
 * @return const char* Conflict description, NULL if no conflict
 */
const char* esp32_devkitc_gpio_get_conflict(uint8_t physical_pin);

/**
 * @brief Get pin usage recommendation
 *
 * @param physical_pin Physical pin number
 * @return const char* Usage recommendation, NULL if no conflict
 */
const char* esp32_devkitc_gpio_get_recommendation(uint8_t physical_pin);

/**
 * @brief Check if PWM frequency is within limits
 *
 * @param frequency Frequency in Hz
 * @return true if frequency is valid
 * @return false if frequency is invalid
 */
bool esp32_devkitc_pwm_is_frequency_valid(uint32_t frequency);

/**
 * @brief Check if PWM duty cycle is valid
 *
 * @param duty_cycle Duty cycle (0-100%)
 * @return true if duty cycle is valid
 * @return false if duty cycle is invalid
 */
bool esp32_devkitc_pwm_is_duty_cycle_valid(uint8_t duty_cycle);

/**
 * @brief Check if PWM resolution is valid
 *
 * @param resolution Resolution in bits
 * @return true if resolution is valid
 * @return false if resolution is invalid
 */
bool esp32_devkitc_pwm_is_resolution_valid(uint8_t resolution);

/**
 * @brief Get recommended PWM frequency for given resolution
 *
 * @param resolution Resolution in bits
 * @return uint32_t Recommended frequency in Hz
 */
uint32_t esp32_devkitc_pwm_get_recommended_frequency(uint8_t resolution);

/**
 * @brief Get maximum number of PWM channels available
 *
 * @return uint8_t Number of available PWM channels
 */
uint8_t esp32_devkitc_pwm_get_available_channels(void);

/**
 * @brief Check if interrupt is supported on pin
 *
 * @param physical_pin Physical pin number
 * @return true if interrupt is supported
 * @return false if interrupt is not supported
 */
bool esp32_devkitc_gpio_is_interrupt_supported(uint8_t physical_pin);

/**
 * @brief Get recommended interrupt task stack size
 *
 * @return uint16_t Recommended stack size in bytes
 */
uint16_t esp32_devkitc_gpio_get_recommended_interrupt_stack_size(void);

/**
 * @brief Get recommended interrupt task priority
 *
 * @return uint8_t Recommended task priority
 */
uint8_t esp32_devkitc_gpio_get_recommended_interrupt_priority(void);

#ifdef __cplusplus
}
#endif

#endif /* __ESP32_DEVKITC_GPIO_CONSTRAINTS_H */
