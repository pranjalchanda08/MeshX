/**
 * @file gpio_pin_map.h
 * @brief GPIO Pin Mapping for ESP32 WROOM DevKit BSP
 *
 * This file defines the logical to physical pin mapping for the ESP32 WROOM DevKit.
 * The ESP32 WROOM DevKit has 34 GPIO pins available for use (GPIO0-GPIO33).
 *
 * Common pin mapping for ESP32 WROOM DevKit:
 * - GPIO0:  Boot button, I2C SCL
 * - GPIO1:  TXD0, I2C SDA
 * - GPIO2:  ADC2_CH2, TOUCH2, HSPIWP, HS2_DATA0
 * - GPIO3:  RXD0, ADC2_CH3, TOUCH3, HSPIHD, HS2_DATA1
 * - GPIO4:  ADC2_CH0, TOUCH0, HSPIHD, HS2_DATA2
 * - GPIO5:  ADC2_CH1, TOUCH1, HSPIWP, HS2_DATA3
 * - GPIO6:  SD_CLK, SPICLK
 * - GPIO7:  SD_DATA0, SPIQ
 * - GPIO8:  SD_DATA1, SPID
 * - GPIO9:  SD_DATA2, SPIHD
 * - GPIO10: SD_DATA3, SPIWP
 * - GPIO11: SD_CMD, SPICS0
 * - GPIO12: ADC2_CH5, TOUCH5, HSPIQ, HS2_DATA4
 * - GPIO13: ADC2_CH4, TOUCH4, HSPID, HS2_DATA5
 * - GPIO14: ADC2_CH6, TOUCH6, HSPICLK, HS2_DATA6
 * - GPIO15: ADC2_CH7, TOUCH7, HSPICS0, HS2_DATA7
 * - GPIO16: UART2_RX, HS1_DATA4
 * - GPIO17: UART2_TX, HS1_DATA5
 * - GPIO18: VSPICLK
 * - GPIO19: VSPIQ
 * - GPIO21: VSPIHD
 * - GPIO22: VSPIWP
 * - GPIO23: VSPID
 * - GPIO25: DAC_1, ADC2_CH8
 * - GPIO26: DAC_2, ADC2_CH9
 * - GPIO27: ADC2_CH7, TOUCH7
 * - GPIO32: ADC1_CH4, TOUCH9
 * - GPIO33: ADC1_CH5, TOUCH8
 * - GPIO34: ADC1_CH6 (input only)
 * - GPIO35: ADC1_CH7 (input only)
 * - GPIO36: ADC1_CH0, SENSOR_VP (input only)
 * - GPIO37: ADC1_CH1, SENSOR_CAPP (input only)
 * - GPIO38: ADC1_CH2, SENSOR_CAPN (input only)
 * - GPIO39: ADC1_CH3, SENSOR_VN (input only)
 *
 * Note: GPIO34-39 are input-only pins.
 *
 * @author MeshX Team
 * @date 2024
 */

#ifndef __ESP32_DEVKITC_GPIO_PIN_MAP_H
#define __ESP32_DEVKITC_GPIO_PIN_MAP_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum number of GPIO pins supported on ESP32 WROOM DevKit
 */
#define ESP32_DEVKITC_GPIO_MAX_PINS 34

/**
 * @brief Invalid pin number
 */
#define ESP32_DEVKITC_GPIO_PIN_INVALID 0xFF

/**
 * @brief ESP32 WROOM DevKit pin names
 */
typedef enum {
    ESP32_DEVKITC_PIN_GPIO0 = 0,
    ESP32_DEVKITC_PIN_GPIO1,
    ESP32_DEVKITC_PIN_GPIO2,
    ESP32_DEVKITC_PIN_GPIO3,
    ESP32_DEVKITC_PIN_GPIO4,
    ESP32_DEVKITC_PIN_GPIO5,
    ESP32_DEVKITC_PIN_GPIO6,
    ESP32_DEVKITC_PIN_GPIO7,
    ESP32_DEVKITC_PIN_GPIO8,
    ESP32_DEVKITC_PIN_GPIO9,
    ESP32_DEVKITC_PIN_GPIO10,
    ESP32_DEVKITC_PIN_GPIO11,
    ESP32_DEVKITC_PIN_GPIO12,
    ESP32_DEVKITC_PIN_GPIO13,
    ESP32_DEVKITC_PIN_GPIO14,
    ESP32_DEVKITC_PIN_GPIO15,
    ESP32_DEVKITC_PIN_GPIO16,
    ESP32_DEVKITC_PIN_GPIO17,
    ESP32_DEVKITC_PIN_GPIO18,
    ESP32_DEVKITC_PIN_GPIO19,
    ESP32_DEVKITC_PIN_GPIO21,
    ESP32_DEVKITC_PIN_GPIO22,
    ESP32_DEVKITC_PIN_GPIO23,
    ESP32_DEVKITC_PIN_GPIO25,
    ESP32_DEVKITC_PIN_GPIO26,
    ESP32_DEVKITC_PIN_GPIO27,
    ESP32_DEVKITC_PIN_GPIO32,
    ESP32_DEVKITC_PIN_GPIO33,
    ESP32_DEVKITC_PIN_GPIO34,
    ESP32_DEVKITC_PIN_GPIO35,
    ESP32_DEVKITC_PIN_GPIO36,
    ESP32_DEVKITC_PIN_GPIO37,
    ESP32_DEVKITC_PIN_GPIO38,
    ESP32_DEVKITC_PIN_GPIO39,
    ESP32_DEVKITC_PIN_MAX
} esp32_devkitc_pin_name_t;

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
extern const uint8_t esp32_devkitc_physical_to_logical[];
extern const uint8_t esp32_devkitc_logical_to_physical[];
extern const char* esp32_devkitc_pin_names[];
extern const uint32_t esp32_devkitc_pin_capabilities[];

/**
 * @brief Check if physical pin is valid for ESP32 WROOM DevKit
 *
 * @param physical_pin Physical pin number
 * @return true if pin is valid
 */
bool gpio_is_physical_pin_valid(uint8_t physical_pin);

/**
 * @brief Check if logical pin is valid for ESP32 WROOM DevKit
 *
 * @param logical_pin Logical pin number
 * @return true if pin is valid
 */
bool gpio_is_logical_pin_valid(uint8_t logical_pin);

/**
 * @brief Map logical pin to physical pin for ESP32 WROOM DevKit
 *
 * @param logical_pin Logical pin number
 * @return uint8_t Physical pin number, ESP32_DEVKITC_GPIO_PIN_INVALID if mapping not found
 */
uint8_t gpio_map_logical_to_physical(uint8_t logical_pin);

/**
 * @brief Map physical pin to logical pin for ESP32 WROOM DevKit
 *
 * @param physical_pin Physical pin number
 * @return uint8_t Logical pin number, ESP32_DEVKITC_GPIO_PIN_INVALID if mapping not found
 */
uint8_t gpio_map_physical_to_logical(uint8_t physical_pin);

/**
 * @brief Get pin name for ESP32 WROOM DevKit
 *
 * @param logical_pin Logical pin number
 * @return const char* Pin name, NULL if not found
 */
const char* gpio_get_pin_name(uint8_t logical_pin);

/**
 * @brief Get pin capabilities for ESP32 WROOM DevKit
 *
 * @param physical_pin Physical pin number
 * @return uint32_t Bitmask of pin capabilities, 0 if pin invalid
 */
uint32_t gpio_get_pin_capabilities(uint8_t physical_pin);

#ifdef __cplusplus
}
#endif

#endif /* __ESP32_DEVKITC_GPIO_PIN_MAP_H */
