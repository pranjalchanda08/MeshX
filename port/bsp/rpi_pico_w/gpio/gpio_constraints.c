/**
 * @file gpio_constraints.c
 * @brief GPIO Constraints Implementation for Raspberry Pi Pico W (RP2040)
 */

#include "gpio_constraints.h"
#include "gpio_pin_map.h"
#include <string.h>

const rpi_pico_w_pin_conflict_t rpi_pico_w_pin_conflicts[] = {
    {
        .pin = 0,
        .conflict = "Default UART0 TX",
        .recommendation = "Can be used as GPIO if UART0 console is redirected to USB CDC"
    },
    {
        .pin = 1,
        .conflict = "Default UART0 RX",
        .recommendation = "Can be used as GPIO if UART0 console is redirected to USB CDC"
    },
    {
        .pin = 23,
        .conflict = "Internal power SMPS mode pin",
        .recommendation = "Not accessible on breakout header"
    },
    {
        .pin = 24,
        .conflict = "Internal VBUS sense",
        .recommendation = "Not accessible on breakout header"
    },
    {
        .pin = 25,
        .conflict = "Connected to CYW43439 SPI on Pico W",
        .recommendation = "Do not use on Pico W"
    },
    {
        .pin = 29,
        .conflict = "Internal VSYS ADC sense",
        .recommendation = "Not accessible on breakout header"
    },
};

#define RPI_PICO_W_CONFLICTS_COUNT (sizeof(rpi_pico_w_pin_conflicts) / sizeof(rpi_pico_w_pin_conflicts[0]))

bool rpi_pico_w_gpio_has_conflict(uint8_t physical_pin)
{
    for (size_t i = 0; i < RPI_PICO_W_CONFLICTS_COUNT; i++) {
        if (rpi_pico_w_pin_conflicts[i].pin == physical_pin) {
            return true;
        }
    }
    return false;
}

const char* rpi_pico_w_gpio_get_conflict(uint8_t physical_pin)
{
    for (size_t i = 0; i < RPI_PICO_W_CONFLICTS_COUNT; i++) {
        if (rpi_pico_w_pin_conflicts[i].pin == physical_pin) {
            return rpi_pico_w_pin_conflicts[i].conflict;
        }
    }
    return NULL;
}

const char* rpi_pico_w_gpio_get_recommendation(uint8_t physical_pin)
{
    for (size_t i = 0; i < RPI_PICO_W_CONFLICTS_COUNT; i++) {
        if (rpi_pico_w_pin_conflicts[i].pin == physical_pin) {
            return rpi_pico_w_pin_conflicts[i].recommendation;
        }
    }
    return NULL;
}

bool rpi_pico_w_pwm_is_frequency_valid(uint32_t frequency)
{
    return (frequency >= RPI_PICO_W_PWM_MIN_FREQUENCY &&
            frequency <= RPI_PICO_W_PWM_MAX_FREQUENCY);
}

bool rpi_pico_w_pwm_is_duty_cycle_valid(uint8_t duty_cycle)
{
    return (duty_cycle <= 100);
}

bool rpi_pico_w_pwm_is_resolution_valid(uint8_t resolution)
{
    return (resolution >= RPI_PICO_W_PWM_MIN_RESOLUTION &&
            resolution <= RPI_PICO_W_PWM_MAX_RESOLUTION);
}

uint32_t rpi_pico_w_pwm_get_recommended_frequency(uint8_t resolution)
{
    switch (resolution) {
        case 1 ... 8:
            return 1000000;
        case 9 ... 12:
            return 250000;
        case 13 ... 16:
            return 50000;
        default:
            return 100000;
    }
}

uint8_t rpi_pico_w_pwm_get_available_channels(void)
{
    return RPI_PICO_W_PWM_MAX_CHANNELS;
}

bool rpi_pico_w_gpio_is_interrupt_supported(uint8_t physical_pin)
{
    uint32_t capabilities = gpio_get_pin_capabilities(physical_pin);
    return (capabilities & GPIO_CAP_INTERRUPT) != 0;
}

uint16_t rpi_pico_w_gpio_get_recommended_interrupt_stack_size(void)
{
    return RPI_PICO_W_INTERRUPT_TASK_STACK_SIZE;
}

uint8_t rpi_pico_w_gpio_get_recommended_interrupt_priority(void)
{
    return RPI_PICO_W_INTERRUPT_TASK_PRIORITY;
}
