/**
 * @file gpio_pin_map.h
 * @brief GPIO Pin Mapping for Seeed XIAO ESP32-C3 BSP
 *
 * This file defines the logical to physical pin mapping for the Seeed XIAO ESP32-C3.
 * The XIAO ESP32-C3 has 11 GPIO pins available for use.
 *
 * Pin mapping reference:
 * - D0: GPIO1  (UART TX, I2C SDA)
 * - D1: GPIO0  (UART RX, I2C SCL)
 * - D2: GPIO2  (GPIO, ADC1_CH2)
 * - D3: GPIO3  (GPIO, ADC1_CH3)
 * - D4: GPIO4  (GPIO, ADC1_CH4)
 * - D5: GPIO5  (GPIO, ADC1_CH5)
 * - D6: GPIO6  (GPIO, ADC1_CH6)
 * - D7: GPIO7  (GPIO, ADC1_CH7)
 * - D8: GPIO8  (GPIO, ADC1_CH8)
 * - D9: GPIO9  (GPIO, ADC1_CH9)
 * - D10: GPIO10 (GPIO, ADC1_CH10)
 *
 * Note: GPIO0 and GPIO1 are also used for UART by default.
 *
 * @author MeshX Team
 * @date 2024
 */

#ifndef __XIAO_C3_GPIO_PIN_MAP_H
#define __XIAO_C3_GPIO_PIN_MAP_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum number of GPIO pins supported on XIAO ESP32-C3
 */
#define XIAO_C3_GPIO_MAX_PINS 11

/**
 * @brief Invalid pin number
 */
#define XIAO_C3_GPIO_PIN_INVALID 0xFF

/**
 * @brief XIAO ESP32-C3 pin names
 */
typedef enum {
    XIAO_C3_PIN_D0 = 0,
    XIAO_C3_PIN_D1,
    XIAO_C3_PIN_D2,
    XIAO_C3_PIN_D3,
    XIAO_C3_PIN_D4,
    XIAO_C3_PIN_D5,
    XIAO_C3_PIN_D6,
    XIAO_C3_PIN_D7,
    XIAO_C3_PIN_D8,
    XIAO_C3_PIN_D9,
    XIAO_C3_PIN_D10,
    XIAO_C3_PIN_MAX
} xiao_c3_pin_name_t;

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
extern const uint8_t xiao_c3_physical_to_logical[];
extern const uint8_t xiao_c3_logical_to_physical[];
extern const char* xiao_c3_pin_names[];
extern const uint32_t xiao_c3_pin_capabilities[];

/**
 * @brief Check if physical pin is valid for XIAO ESP32-C3
 *
 * @param physical_pin Physical pin number (ESP32-C3 GPIO number)
 * @return true if pin is valid
 */
bool gpio_is_physical_pin_valid(uint8_t physical_pin);

/**
 * @brief Check if logical pin is valid for XIAO ESP32-C3
 *
 * @param logical_pin Logical pin number (0-255)
 * @return true if pin is valid
 */
bool gpio_is_logical_pin_valid(uint8_t logical_pin);

/**
 * @brief Map logical pin to physical pin for XIAO ESP32-C3
 *
 * @param logical_pin Logical pin number (0-255)
 * @return uint8_t Physical pin number, XIAO_C3_GPIO_PIN_INVALID if mapping not found
 */
uint8_t gpio_map_logical_to_physical(uint8_t logical_pin);

/**
 * @brief Map physical pin to logical pin for XIAO ESP32-C3
 *
 * @param physical_pin Physical pin number (ESP32-C3 GPIO number)
 * @return uint8_t Logical pin number, XIAO_C3_GPIO_PIN_INVALID if mapping not found
 */
uint8_t gpio_map_physical_to_logical(uint8_t physical_pin);

/**
 * @brief Get pin name for XIAO ESP32-C3
 *
 * @param logical_pin Logical pin number
 * @return const char* Pin name, NULL if not found
 */
const char* gpio_get_pin_name(uint8_t logical_pin);

/**
 * @brief Get pin capabilities for XIAO ESP32-C3
 *
 * @param physical_pin Physical pin number
 * @return uint32_t Bitmask of pin capabilities, 0 if pin invalid
 */
uint32_t gpio_get_pin_capabilities(uint8_t physical_pin);

#ifdef __cplusplus
}
#endif

#endif /* __XIAO_C3_GPIO_PIN_MAP_H */
