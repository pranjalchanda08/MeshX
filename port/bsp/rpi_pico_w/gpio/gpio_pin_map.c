/**
 * @file gpio_pin_map.c
 * @brief GPIO Pin Mapping Implementation for Raspberry Pi Pico W (RP2040)
 */

#include "gpio_pin_map.h"
#include <stddef.h>

const uint8_t rpi_pico_w_physical_to_logical[RPI_PICO_W_GPIO_MAX_PINS] = {
    [0]  = 0,  [1]  = 1,  [2]  = 2,  [3]  = 3,  [4]  = 4,
    [5]  = 5,  [6]  = 6,  [7]  = 7,  [8]  = 8,  [9]  = 9,
    [10] = 10, [11] = 11, [12] = 12, [13] = 13, [14] = 14,
    [15] = 15, [16] = 16, [17] = 17, [18] = 18, [19] = 19,
    [20] = 20, [21] = 21, [22] = 22,
    [23] = RPI_PICO_W_GPIO_PIN_INVALID,
    [24] = RPI_PICO_W_GPIO_PIN_INVALID,
    [25] = RPI_PICO_W_GPIO_PIN_INVALID,
    [26] = 26, [27] = 27, [28] = 28,
    [29] = RPI_PICO_W_GPIO_PIN_INVALID,
};

const uint8_t rpi_pico_w_logical_to_physical[RPI_PICO_W_GPIO_MAX_PINS] = {
    [0]  = 0,  [1]  = 1,  [2]  = 2,  [3]  = 3,  [4]  = 4,
    [5]  = 5,  [6]  = 6,  [7]  = 7,  [8]  = 8,  [9]  = 9,
    [10] = 10, [11] = 11, [12] = 12, [13] = 13, [14] = 14,
    [15] = 15, [16] = 16, [17] = 17, [18] = 18, [19] = 19,
    [20] = 20, [21] = 21, [22] = 22,
    [23] = RPI_PICO_W_GPIO_PIN_INVALID,
    [24] = RPI_PICO_W_GPIO_PIN_INVALID,
    [25] = RPI_PICO_W_GPIO_PIN_INVALID,
    [26] = 26, [27] = 27, [28] = 28,
    [29] = RPI_PICO_W_GPIO_PIN_INVALID,
};

const char* rpi_pico_w_pin_names[RPI_PICO_W_GPIO_MAX_PINS] = {
    [0]  = "GP0",  [1]  = "GP1",  [2]  = "GP2",  [3]  = "GP3",  [4]  = "GP4",
    [5]  = "GP5",  [6]  = "GP6",  [7]  = "GP7",  [8]  = "GP8",  [9]  = "GP9",
    [10] = "GP10", [11] = "GP11", [12] = "GP12", [13] = "GP13", [14] = "GP14",
    [15] = "GP15", [16] = "GP16", [17] = "GP17", [18] = "GP18", [19] = "GP19",
    [20] = "GP20", [21] = "GP21", [22] = "GP22",
    [23] = "INTERNAL", [24] = "INTERNAL", [25] = "INTERNAL",
    [26] = "GP26_ADC0", [27] = "GP27_ADC1", [28] = "GP28_ADC2",
    [29] = "INTERNAL",
};

#define RP2040_STD_GPIO (GPIO_CAP_INPUT | GPIO_CAP_OUTPUT | GPIO_CAP_PULL_UP | GPIO_CAP_PULL_DOWN | GPIO_CAP_OPEN_DRAIN | GPIO_CAP_PWM | GPIO_CAP_INTERRUPT)
#define RP2040_ADC_GPIO (RP2040_STD_GPIO | GPIO_CAP_ADC)

const uint32_t rpi_pico_w_pin_capabilities[RPI_PICO_W_GPIO_MAX_PINS] = {
    [0]  = RP2040_STD_GPIO, [1]  = RP2040_STD_GPIO, [2]  = RP2040_STD_GPIO, [3]  = RP2040_STD_GPIO,
    [4]  = RP2040_STD_GPIO, [5]  = RP2040_STD_GPIO, [6]  = RP2040_STD_GPIO, [7]  = RP2040_STD_GPIO,
    [8]  = RP2040_STD_GPIO, [9]  = RP2040_STD_GPIO, [10] = RP2040_STD_GPIO, [11] = RP2040_STD_GPIO,
    [12] = RP2040_STD_GPIO, [13] = RP2040_STD_GPIO, [14] = RP2040_STD_GPIO, [15] = RP2040_STD_GPIO,
    [16] = RP2040_STD_GPIO, [17] = RP2040_STD_GPIO, [18] = RP2040_STD_GPIO, [19] = RP2040_STD_GPIO,
    [20] = RP2040_STD_GPIO, [21] = RP2040_STD_GPIO, [22] = RP2040_STD_GPIO,
    [23] = 0, [24] = 0, [25] = 0,
    [26] = RP2040_ADC_GPIO, [27] = RP2040_ADC_GPIO, [28] = RP2040_ADC_GPIO,
    [29] = 0,
};

bool gpio_is_physical_pin_valid(uint8_t physical_pin)
{
    return (physical_pin < RPI_PICO_W_GPIO_MAX_PINS &&
            rpi_pico_w_physical_to_logical[physical_pin] != RPI_PICO_W_GPIO_PIN_INVALID);
}

bool gpio_is_logical_pin_valid(uint8_t logical_pin)
{
    return (logical_pin < RPI_PICO_W_GPIO_MAX_PINS &&
            rpi_pico_w_logical_to_physical[logical_pin] != RPI_PICO_W_GPIO_PIN_INVALID);
}

uint8_t gpio_map_logical_to_physical(uint8_t logical_pin)
{
    if (gpio_is_logical_pin_valid(logical_pin)) {
        return rpi_pico_w_logical_to_physical[logical_pin];
    }
    return RPI_PICO_W_GPIO_PIN_INVALID;
}

uint8_t gpio_map_physical_to_logical(uint8_t physical_pin)
{
    if (gpio_is_physical_pin_valid(physical_pin)) {
        return rpi_pico_w_physical_to_logical[physical_pin];
    }
    return RPI_PICO_W_GPIO_PIN_INVALID;
}

const char* gpio_get_pin_name(uint8_t logical_pin)
{
    if (gpio_is_logical_pin_valid(logical_pin)) {
        uint8_t phys = rpi_pico_w_logical_to_physical[logical_pin];
        return rpi_pico_w_pin_names[phys];
    }
    return NULL;
}

uint32_t gpio_get_pin_capabilities(uint8_t physical_pin)
{
    if (gpio_is_physical_pin_valid(physical_pin)) {
        return rpi_pico_w_pin_capabilities[physical_pin];
    }
    return 0;
}
