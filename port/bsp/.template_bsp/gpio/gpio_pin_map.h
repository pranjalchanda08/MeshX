/**
 * @file gpio_pin_map.h
 * @brief GPIO Pin Mapping for Template BSP
 *
 * This file defines the logical to physical pin mapping for the template BSP.
 * Each BSP should provide its own pin mapping based on hardware capabilities.
 *
 * @author MeshX Team
 * @date 2024
 */

#ifndef __GPIO_PIN_MAP_H
#define __GPIO_PIN_MAP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum number of GPIO pins supported
 */
#define GPIO_MAX_PINS 32

/**
 * @brief Invalid pin number
 */
#define GPIO_PIN_INVALID 0xFF

/**
 * @brief Check if physical pin is valid
 *
 * @param physical_pin Physical pin number
 * @return true if pin is valid
 * @return false if pin is invalid
 */
bool gpio_is_physical_pin_valid(uint8_t physical_pin);

/**
 * @brief Check if logical pin is valid
 *
 * @param logical_pin Logical pin number
 * @return true if pin is valid
 * @return false if pin is invalid
 */
bool gpio_is_logical_pin_valid(uint8_t logical_pin);

/**
 * @brief Map logical pin to physical pin
 *
 * @param logical_pin Logical pin number (0-255)
 * @return uint8_t Physical pin number, GPIO_PIN_INVALID if mapping not found
 */
uint8_t gpio_map_logical_to_physical(uint8_t logical_pin);

/**
 * @brief Map physical pin to logical pin
 *
 * @param physical_pin Physical pin number
 * @return uint8_t Logical pin number, GPIO_PIN_INVALID if mapping not found
 */
uint8_t gpio_map_physical_to_logical(uint8_t physical_pin);

/**
 * @brief Get pin name
 *
 * @param logical_pin Logical pin number
 * @return const char* Pin name, NULL if not found
 */
const char* gpio_get_pin_name(uint8_t logical_pin);

/**
 * @brief Get pin capabilities
 *
 * @param physical_pin Physical pin number
 * @return uint32_t Bitmask of pin capabilities
 */
uint32_t gpio_get_pin_capabilities(uint8_t physical_pin);

// Pin capability flags
#define GPIO_CAP_INPUT          (1 << 0)  /**< Pin can be input */
#define GPIO_CAP_OUTPUT         (1 << 1)  /**< Pin can be output */
#define GPIO_CAP_PULL_UP        (1 << 2)  /**< Pin has pull-up resistor */
#define GPIO_CAP_PULL_DOWN      (1 << 3)  /**< Pin has pull-down resistor */
#define GPIO_CAP_OPEN_DRAIN     (1 << 4)  /**< Pin supports open-drain */
#define GPIO_CAP_PWM            (1 << 5)  /**< Pin supports PWM */
#define GPIO_CAP_ADC            (1 << 6)  /**< Pin supports ADC (future) */
#define GPIO_CAP_DAC            (1 << 7)  /**< Pin supports DAC (future) */
#define GPIO_CAP_INTERRUPT      (1 << 8)  /**< Pin supports interrupts */

#ifdef __cplusplus
}
#endif

#endif /* __GPIO_PIN_MAP_H */
