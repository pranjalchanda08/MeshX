/**
 * @file gpio_pin_map.h
 * @brief GPIO Pin Mapping for Raspberry Pi Pico W BSP (RP2040)
 *
 * This file defines the logical to physical pin mapping for the Raspberry Pi Pico W.
 * The RP2040 has 26 breakout GPIO pins available on the 40-pin header.
 *
 * Pin mapping reference:
 * - GP0 - GP22: Digital GPIOs (PWM, SPI, I2C, UART)
 * - GP26: ADC0 / GPIO26
 * - GP27: ADC1 / GPIO27
 * - GP28: ADC2 / GPIO28
 *
 * @author MeshX Team
 * @date 2026
 */

#ifndef __RPI_PICO_W_GPIO_PIN_MAP_H
#define __RPI_PICO_W_GPIO_PIN_MAP_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum number of GPIO pins on RP2040
 */
#define RPI_PICO_W_GPIO_MAX_PINS 30

/**
 * @brief Invalid pin number
 */
#define RPI_PICO_W_GPIO_PIN_INVALID 0xFF

// Pin capability flags
#define GPIO_CAP_INPUT          (1 << 0)  /**< Pin can be input */
#define GPIO_CAP_OUTPUT         (1 << 1)  /**< Pin can be output */
#define GPIO_CAP_PULL_UP        (1 << 2)  /**< Pin has pull-up resistor */
#define GPIO_CAP_PULL_DOWN      (1 << 3)  /**< Pin has pull-down resistor */
#define GPIO_CAP_OPEN_DRAIN     (1 << 4)  /**< Pin supports open-drain */
#define GPIO_CAP_PWM            (1 << 5)  /**< Pin supports PWM */
#define GPIO_CAP_ADC            (1 << 6)  /**< Pin supports ADC */
#define GPIO_CAP_DAC            (1 << 7)  /**< Pin supports DAC */
#define GPIO_CAP_INTERRUPT      (1 << 8)  /**< Pin supports interrupts */

// External array declarations
extern const uint8_t rpi_pico_w_physical_to_logical[];
extern const uint8_t rpi_pico_w_logical_to_physical[];
extern const char* rpi_pico_w_pin_names[];
extern const uint32_t rpi_pico_w_pin_capabilities[];

bool gpio_is_physical_pin_valid(uint8_t physical_pin);
bool gpio_is_logical_pin_valid(uint8_t logical_pin);
uint8_t gpio_map_logical_to_physical(uint8_t logical_pin);
uint8_t gpio_map_physical_to_logical(uint8_t physical_pin);
const char* gpio_get_pin_name(uint8_t logical_pin);
uint32_t gpio_get_pin_capabilities(uint8_t physical_pin);

#ifdef __cplusplus
}
#endif

#endif /* __RPI_PICO_W_GPIO_PIN_MAP_H */
