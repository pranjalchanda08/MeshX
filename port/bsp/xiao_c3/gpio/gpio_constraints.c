/**
 * @file gpio_constraints.c
 * @brief GPIO Constraints Implementation for Seeed XIAO ESP32-C3 BSP
 *
 * This file implements board-specific constraints and limitations for GPIO operations
 * on the Seeed XIAO ESP32-C3.
 *
 * @author MeshX Team
 * @date 2024
 */

#include "gpio_constraints.h"
#include "gpio_pin_map.h"
#include <string.h>
 
/**
 * @brief XIAO ESP32-C3 pin conflicts
 */
const xiao_c3_pin_conflict_t xiao_c3_pin_conflicts[] = {
    {
        .pin = 0,  // D1/GPIO0
        .conflict = "UART RX, I2C SCL, Boot mode pin",
        .recommendation = "Avoid using for critical GPIO if UART/I2C needed"
    },
    {
        .pin = 1,  // D0/GPIO1
        .conflict = "UART TX, I2C SDA",
        .recommendation = "Avoid using for critical GPIO if UART/I2C needed"
    },
    {
        .pin = 2,  // D2/GPIO2
        .conflict = "ADC1_CH2",
        .recommendation = "Can be used for analog input or GPIO"
    },
    {
        .pin = 3,  // D3/GPIO3
        .conflict = "ADC1_CH3",
        .recommendation = "Can be used for analog input or GPIO"
    },
    // Pins 4-10 are general purpose with ADC capability
};

/**
 * @brief Check if pin has conflicts
 */
bool xiao_c3_gpio_has_conflict(uint8_t physical_pin)
{
    for (size_t i = 0; i < XIAO_C3_PIN_CONFLICTS_COUNT; i++) {
        if (xiao_c3_pin_conflicts[i].pin == physical_pin) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Get pin conflict description
 */
const char* xiao_c3_gpio_get_conflict(uint8_t physical_pin)
{
    for (size_t i = 0; i < XIAO_C3_PIN_CONFLICTS_COUNT; i++) {
        if (xiao_c3_pin_conflicts[i].pin == physical_pin) {
            return xiao_c3_pin_conflicts[i].conflict;
        }
    }
    return NULL;
}

/**
 * @brief Get pin usage recommendation
 */
const char* xiao_c3_gpio_get_recommendation(uint8_t physical_pin)
{
    for (size_t i = 0; i < XIAO_C3_PIN_CONFLICTS_COUNT; i++) {
        if (xiao_c3_pin_conflicts[i].pin == physical_pin) {
            return xiao_c3_pin_conflicts[i].recommendation;
        }
    }
    return NULL;
}

/**
 * @brief Check if PWM frequency is within limits
 */
bool xiao_c3_pwm_is_frequency_valid(uint32_t frequency)
{
    return (frequency >= XIAO_C3_PWM_MIN_FREQUENCY &&
            frequency <= XIAO_C3_PWM_MAX_FREQUENCY);
}

/**
 * @brief Check if PWM duty cycle is valid
 */
bool xiao_c3_pwm_is_duty_cycle_valid(uint8_t duty_cycle)
{
    return (duty_cycle <= 100);  // 0-100% valid range
}

/**
 * @brief Check if PWM resolution is valid
 */
bool xiao_c3_pwm_is_resolution_valid(uint8_t resolution)
{
    return (resolution >= XIAO_C3_PWM_MIN_RESOLUTION &&
            resolution <= XIAO_C3_PWM_MAX_RESOLUTION);
}

/**
 * @brief Get recommended PWM frequency for given resolution
 *
 * Higher resolutions work better with lower frequencies due to
 * ESP32-C3 LEDC timer limitations.
 */
uint32_t xiao_c3_pwm_get_recommended_frequency(uint8_t resolution)
{
    // Recommended frequencies based on resolution
    // Higher resolution = lower maximum frequency
    switch (resolution) {
        case 1 ... 4:
            return 1000000;  // 1MHz for low resolution
        case 5 ... 8:
            return 500000;   // 500kHz for medium resolution
        case 9 ... 12:
            return 100000;   // 100kHz for high resolution
        case 13 ... 16:
            return 50000;    // 50kHz for very high resolution
        default:
            return 100000;   // Default 100kHz
    }
}

/**
 * @brief Get maximum number of PWM channels available
 *
 * ESP32-C3 has 6 LEDC channels total
 */
uint8_t xiao_c3_pwm_get_available_channels(void)
{
    return XIAO_C3_PWM_MAX_CHANNELS;
}

/**
 * @brief Check if interrupt is supported on pin
 *
 * All GPIO pins on ESP32-C3 support interrupts
 */
bool xiao_c3_gpio_is_interrupt_supported(uint8_t physical_pin)
{
    // Check if pin is valid and has interrupt capability
    uint32_t capabilities = gpio_get_pin_capabilities(physical_pin);
    return (capabilities & GPIO_CAP_INTERRUPT) != 0;
}

/**
 * @brief Get recommended interrupt task stack size
 */
uint16_t xiao_c3_gpio_get_recommended_interrupt_stack_size(void)
{
    return XIAO_C3_INTERRUPT_TASK_STACK_SIZE;
}

/**
 * @brief Get recommended interrupt task priority
 */
uint8_t xiao_c3_gpio_get_recommended_interrupt_priority(void)
{
    return XIAO_C3_INTERRUPT_TASK_PRIORITY;
}
