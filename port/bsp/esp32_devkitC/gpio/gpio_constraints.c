/**
 * @file gpio_constraints.c
 * @brief GPIO Constraints Implementation for ESP32 WROOM DevKit BSP
 *
 * This file implements board-specific constraints and limitations for GPIO operations
 * on the ESP32 WROOM DevKit.
 *
 * @author MeshX Team
 * @date 2024
 */

#include "gpio_constraints.h"
#include "gpio_pin_map.h"
#include <string.h>
 
/**
 * @brief ESP32 WROOM DevKit pin conflicts
 */
const esp32_devkitc_pin_conflict_t esp32_devkitc_pin_conflicts[] = {
    {
        .pin = 0,  // GPIO0
        .conflict = "Boot button, I2C SCL, Strapping pin",
        .recommendation = "Can affect boot mode, use with caution"
    },
    {
        .pin = 1,  // GPIO1
        .conflict = "UART TX, I2C SDA",
        .recommendation = "Avoid using if UART/I2C communication needed"
    },
    {
        .pin = 2,  // GPIO2
        .conflict = "Strapping pin, ADC2_CH2, TOUCH2",
        .recommendation = "Can affect boot mode, use with caution"
    },
    {
        .pin = 3,  // GPIO3
        .conflict = "UART RX, ADC2_CH3, TOUCH3",
        .recommendation = "Avoid using if UART communication needed"
    },
    {
        .pin = 6,  // GPIO6
        .conflict = "SPI flash, not recommended for GPIO",
        .recommendation = "Do not use - connected to flash memory"
    },
    {
        .pin = 7,  // GPIO7
        .conflict = "SPI flash, not recommended for GPIO",
        .recommendation = "Do not use - connected to flash memory"
    },
    {
        .pin = 8,  // GPIO8
        .conflict = "SPI flash, not recommended for GPIO",
        .recommendation = "Do not use - connected to flash memory"
    },
    {
        .pin = 9,  // GPIO9
        .conflict = "SPI flash, not recommended for GPIO",
        .recommendation = "Do not use - connected to flash memory"
    },
    {
        .pin = 10, // GPIO10
        .conflict = "SPI flash, not recommended for GPIO",
        .recommendation = "Do not use - connected to flash memory"
    },
    {
        .pin = 11, // GPIO11
        .conflict = "SPI flash, not recommended for GPIO",
        .recommendation = "Do not use - connected to flash memory"
    },
    {
        .pin = 34, // GPIO34
        .conflict = "Input only, no output capability",
        .recommendation = "Use only for input operations"
    },
    {
        .pin = 35, // GPIO35
        .conflict = "Input only, no output capability",
        .recommendation = "Use only for input operations"
    },
    {
        .pin = 36, // GPIO36
        .conflict = "Input only, no output capability",
        .recommendation = "Use only for input operations"
    },
    {
        .pin = 37, // GPIO37
        .conflict = "Input only, no output capability",
        .recommendation = "Use only for input operations"
    },
    {
        .pin = 38, // GPIO38
        .conflict = "Input only, no output capability",
        .recommendation = "Use only for input operations"
    },
    {
        .pin = 39, // GPIO39
        .conflict = "Input only, no output capability",
        .recommendation = "Use only for input operations"
    },
};

/**
 * @brief Check if pin has conflicts
 */
bool esp32_devkitc_gpio_has_conflict(uint8_t physical_pin)
{
    for (size_t i = 0; i < ESP32_DEVKITC_PIN_CONFLICTS_COUNT; i++) {
        if (esp32_devkitc_pin_conflicts[i].pin == physical_pin) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Get pin conflict description
 */
const char* esp32_devkitc_gpio_get_conflict(uint8_t physical_pin)
{
    for (size_t i = 0; i < ESP32_DEVKITC_PIN_CONFLICTS_COUNT; i++) {
        if (esp32_devkitc_pin_conflicts[i].pin == physical_pin) {
            return esp32_devkitc_pin_conflicts[i].conflict;
        }
    }
    return NULL;
}

/**
 * @brief Get pin usage recommendation
 */
const char* esp32_devkitc_gpio_get_recommendation(uint8_t physical_pin)
{
    for (size_t i = 0; i < ESP32_DEVKITC_PIN_CONFLICTS_COUNT; i++) {
        if (esp32_devkitc_pin_conflicts[i].pin == physical_pin) {
            return esp32_devkitc_pin_conflicts[i].recommendation;
        }
    }
    return NULL;
}

/**
 * @brief Check if PWM frequency is within limits
 */
bool esp32_devkitc_pwm_is_frequency_valid(uint32_t frequency)
{
    return (frequency >= ESP32_DEVKITC_PWM_MIN_FREQUENCY &&
            frequency <= ESP32_DEVKITC_PWM_MAX_FREQUENCY);
}

/**
 * @brief Check if PWM duty cycle is valid
 */
bool esp32_devkitc_pwm_is_duty_cycle_valid(uint8_t duty_cycle)
{
    return (duty_cycle <= 100);  // 0-100% valid range
}

/**
 * @brief Check if PWM resolution is valid
 */
bool esp32_devkitc_pwm_is_resolution_valid(uint8_t resolution)
{
    return (resolution >= ESP32_DEVKITC_PWM_MIN_RESOLUTION &&
            resolution <= ESP32_DEVKITC_PWM_MAX_RESOLUTION);
}

/**
 * @brief Get recommended PWM frequency for given resolution
 *
 * Higher resolutions work better with lower frequencies due to
 * ESP32 LEDC timer limitations.
 */
uint32_t esp32_devkitc_pwm_get_recommended_frequency(uint8_t resolution)
{
    // Recommended frequencies based on resolution
    // Higher resolution = lower maximum frequency
    switch (resolution) {
        case 1 ... 8:
            return 1000000;  // 1MHz for low resolution
        case 9 ... 12:
            return 500000;   // 500kHz for medium resolution
        case 13 ... 16:
            return 100000;   // 100kHz for high resolution
        case 17 ... 20:
            return 50000;    // 50kHz for very high resolution
        default:
            return 100000;   // Default 100kHz
    }
}

/**
 * @brief Get maximum number of PWM channels available
 *
 * ESP32 has 16 LEDC channels total
 */
uint8_t esp32_devkitc_pwm_get_available_channels(void)
{
    return ESP32_DEVKITC_PWM_MAX_CHANNELS;
}

/**
 * @brief Check if interrupt is supported on pin
 *
 * All GPIO pins on ESP32 support interrupts
 */
bool esp32_devkitc_gpio_is_interrupt_supported(uint8_t physical_pin)
{
    // Check if pin is valid and has interrupt capability
    uint32_t capabilities = gpio_get_pin_capabilities(physical_pin);
    return (capabilities & GPIO_CAP_INTERRUPT) != 0;
}

/**
 * @brief Get recommended interrupt task stack size
 */
uint16_t esp32_devkitc_gpio_get_recommended_interrupt_stack_size(void)
{
    return ESP32_DEVKITC_INTERRUPT_TASK_STACK_SIZE;
}

/**
 * @brief Get recommended interrupt task priority
 */
uint8_t esp32_devkitc_gpio_get_recommended_interrupt_priority(void)
{
    return ESP32_DEVKITC_INTERRUPT_TASK_PRIORITY;
}
