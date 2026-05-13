/**
 * @file gpio_pin_map.h
 * @brief GPIO Pin Mapping for WeAct ESP32-C3 BSP
 *
 * This file defines the logical to physical pin mapping for the WeAct ESP32-C3.
 * The WeAct ESP32-C3 has 22 GPIO pins available for use.
 *
 * Common pin mapping for WeAct ESP32-C3 boards:
 * - GPIO0:  Boot button, I2C SCL
 * - GPIO1:  TXD0, I2C SDA
 * - GPIO2:  RXD0, ADC1_CH2
 * - GPIO3:  CTS0, ADC1_CH3
 * - GPIO4:  RTS0, ADC1_CH4
 * - GPIO5:  GPIO, ADC1_CH5
 * - GPIO6:  GPIO, ADC1_CH6
 * - GPIO7:  GPIO, ADC1_CH7
 * - GPIO8:  GPIO, ADC1_CH8
 * - GPIO9:  GPIO, ADC1_CH9
 * - GPIO10: GPIO, ADC1_CH10
 * - GPIO11: GPIO, ADC2_CH0
 * - GPIO12: GPIO, ADC2_CH1
 * - GPIO13: GPIO, ADC2_CH2
 * - GPIO14: GPIO, ADC2_CH3
 * - GPIO15: GPIO, ADC2_CH4
 * - GPIO16: GPIO, ADC2_CH5
 * - GPIO17: GPIO, ADC2_CH6
 * - GPIO18: GPIO, ADC2_CH7
 * - GPIO19: GPIO, ADC2_CH8
 * - GPIO20: GPIO, ADC2_CH9
 * - GPIO21: GPIO, ADC2_CH10
 *
 * @author MeshX Team
 * @date 2024
 */

#ifndef __WEACT_C3_GPIO_PIN_MAP_H
#define __WEACT_C3_GPIO_PIN_MAP_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum number of GPIO pins supported on WeAct ESP32-C3
 */
#define WEACT_C3_GPIO_MAX_PINS 22

/**
 * @brief Invalid pin number
 */
#define WEACT_C3_GPIO_PIN_INVALID 0xFF

/**
 * @brief WeAct ESP32-C3 pin names
 */
typedef enum {
    WEACT_C3_PIN_GPIO0 = 0,
    WEACT_C3_PIN_GPIO1,
    WEACT_C3_PIN_GPIO2,
    WEACT_C3_PIN_GPIO3,
    WEACT_C3_PIN_GPIO4,
    WEACT_C3_PIN_GPIO5,
    WEACT_C3_PIN_GPIO6,
    WEACT_C3_PIN_GPIO7,
    WEACT_C3_PIN_GPIO8,
    WEACT_C3_PIN_GPIO9,
    WEACT_C3_PIN_GPIO10,
    WEACT_C3_PIN_GPIO11,
    WEACT_C3_PIN_GPIO12,
    WEACT_C3_PIN_GPIO13,
    WEACT_C3_PIN_GPIO14,
    WEACT_C3_PIN_GPIO15,
    WEACT_C3_PIN_GPIO16,
    WEACT_C3_PIN_GPIO17,
    WEACT_C3_PIN_GPIO18,
    WEACT_C3_PIN_GPIO19,
    WEACT_C3_PIN_GPIO20,
    WEACT_C3_PIN_GPIO21,
    WEACT_C3_PIN_MAX
} weact_c3_pin_name_t;

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

// External array declarations
extern const uint8_t weact_c3_physical_to_logical[];
extern const uint8_t weact_c3_logical_to_physical[];
extern const char* weact_c3_pin_names[];
extern const uint32_t weact_c3_pin_capabilities[];

/**
 * @brief Check if physical pin is valid for WeAct ESP32-C3
 *
 * @param physical_pin Physical pin number
 * @return true if pin is valid
 */
bool gpio_is_physical_pin_valid(uint8_t physical_pin);

/**
 * @brief Check if logical pin is valid for WeAct ESP32-C3
 *
 * @param logical_pin Logical pin number
 * @return true if pin is valid
 */
bool gpio_is_logical_pin_valid(uint8_t logical_pin);

/**
 * @brief Map logical pin to physical pin for WeAct ESP32-C3
 *
 * @param logical_pin Logical pin number
 * @return uint8_t Physical pin number, WEACT_C3_GPIO_PIN_INVALID if mapping not found
 */
uint8_t gpio_map_logical_to_physical(uint8_t logical_pin);

/**
 * @brief Map physical pin to logical pin for WeAct ESP32-C3
 *
 * @param physical_pin Physical pin number
 * @return uint8_t Logical pin number, WEACT_C3_GPIO_PIN_INVALID if mapping not found
 */
uint8_t gpio_map_physical_to_logical(uint8_t physical_pin);

/**
 * @brief Get pin name for WeAct ESP32-C3
 *
 * @param logical_pin Logical pin number
 * @return const char* Pin name, NULL if not found
 */
const char* gpio_get_pin_name(uint8_t logical_pin);

/**
 * @brief Get pin capabilities for WeAct ESP32-C3
 *
 * @param physical_pin Physical pin number
 * @return uint32_t Bitmask of pin capabilities, 0 if pin invalid
 */
uint32_t gpio_get_pin_capabilities(uint8_t physical_pin);

#ifdef __cplusplus
}
#endif

#endif /* __WEACT_C3_GPIO_PIN_MAP_H */
