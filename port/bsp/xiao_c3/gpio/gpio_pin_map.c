/**
 * @file gpio_pin_map.c
 * @brief GPIO Pin Mapping Implementation for Seeed XIAO ESP32-C3 BSP
 */
 
#include "gpio_pin_map.h"
#include <stddef.h>
 
/**
 * @brief XIAO ESP32-C3 physical to logical pin mapping
 */
const uint8_t xiao_c3_physical_to_logical[] = {
    [1]  = XIAO_C3_PIN_D0,   // GPIO1 -> D0
    [0]  = XIAO_C3_PIN_D1,   // GPIO0 -> D1
    [2]  = XIAO_C3_PIN_D2,   // GPIO2 -> D2
    [3]  = XIAO_C3_PIN_D3,   // GPIO3 -> D3
    [4]  = XIAO_C3_PIN_D4,   // GPIO4 -> D4
    [5]  = XIAO_C3_PIN_D5,   // GPIO5 -> D5
    [6]  = XIAO_C3_PIN_D6,   // GPIO6 -> D6
    [7]  = XIAO_C3_PIN_D7,   // GPIO7 -> D7
    [8]  = XIAO_C3_PIN_D8,   // GPIO8 -> D8
    [9]  = XIAO_C3_PIN_D9,   // GPIO9 -> D9
    [10] = XIAO_C3_PIN_D10,  // GPIO10 -> D10
};
 
/**
 * @brief XIAO ESP32-C3 logical to physical pin mapping
 */
const uint8_t xiao_c3_logical_to_physical[] = {
    [XIAO_C3_PIN_D0]  = 1,   // D0 -> GPIO1
    [XIAO_C3_PIN_D1]  = 0,   // D1 -> GPIO0
    [XIAO_C3_PIN_D2]  = 2,   // D2 -> GPIO2
    [XIAO_C3_PIN_D3]  = 3,   // D3 -> GPIO3
    [XIAO_C3_PIN_D4]  = 4,   // D4 -> GPIO4
    [XIAO_C3_PIN_D5]  = 5,   // D5 -> GPIO5
    [XIAO_C3_PIN_D6]  = 6,   // D6 -> GPIO6
    [XIAO_C3_PIN_D7]  = 7,   // D7 -> GPIO7
    [XIAO_C3_PIN_D8]  = 8,   // D8 -> GPIO8
    [XIAO_C3_PIN_D9]  = 9,   // D9 -> GPIO9
    [XIAO_C3_PIN_D10] = 10,  // D10 -> GPIO10
};
 
/**
 * @brief XIAO ESP32-C3 pin names as strings
 */
const char* xiao_c3_pin_names[] = {
    [XIAO_C3_PIN_D0]  = "D0",
    [XIAO_C3_PIN_D1]  = "D1",
    [XIAO_C3_PIN_D2]  = "D2",
    [XIAO_C3_PIN_D3]  = "D3",
    [XIAO_C3_PIN_D4]  = "D4",
    [XIAO_C3_PIN_D5]  = "D5",
    [XIAO_C3_PIN_D6]  = "D6",
    [XIAO_C3_PIN_D7]  = "D7",
    [XIAO_C3_PIN_D8]  = "D8",
    [XIAO_C3_PIN_D9]  = "D9",
    [XIAO_C3_PIN_D10] = "D10",
};
 
/**
 * @brief XIAO ESP32-C3 pin capabilities
 */
const uint32_t xiao_c3_pin_capabilities[] = {
    [XIAO_C3_PIN_D0]  = GPIO_CAP_INPUT | GPIO_CAP_OUTPUT | GPIO_CAP_PULL_UP | GPIO_CAP_PULL_DOWN | GPIO_CAP_OPEN_DRAIN | GPIO_CAP_PWM | GPIO_CAP_INTERRUPT,
    [XIAO_C3_PIN_D1]  = GPIO_CAP_INPUT | GPIO_CAP_OUTPUT | GPIO_CAP_PULL_UP | GPIO_CAP_PULL_DOWN | GPIO_CAP_OPEN_DRAIN | GPIO_CAP_PWM | GPIO_CAP_INTERRUPT,
    [XIAO_C3_PIN_D2]  = GPIO_CAP_INPUT | GPIO_CAP_OUTPUT | GPIO_CAP_PULL_UP | GPIO_CAP_PULL_DOWN | GPIO_CAP_OPEN_DRAIN | GPIO_CAP_PWM | GPIO_CAP_ADC | GPIO_CAP_INTERRUPT,
    [XIAO_C3_PIN_D3]  = GPIO_CAP_INPUT | GPIO_CAP_OUTPUT | GPIO_CAP_PULL_UP | GPIO_CAP_PULL_DOWN | GPIO_CAP_OPEN_DRAIN | GPIO_CAP_PWM | GPIO_CAP_ADC | GPIO_CAP_INTERRUPT,
    [XIAO_C3_PIN_D4]  = GPIO_CAP_INPUT | GPIO_CAP_OUTPUT | GPIO_CAP_PULL_UP | GPIO_CAP_PULL_DOWN | GPIO_CAP_OPEN_DRAIN | GPIO_CAP_PWM | GPIO_CAP_ADC | GPIO_CAP_INTERRUPT,
    [XIAO_C3_PIN_D5]  = GPIO_CAP_INPUT | GPIO_CAP_OUTPUT | GPIO_CAP_PULL_UP | GPIO_CAP_PULL_DOWN | GPIO_CAP_OPEN_DRAIN | GPIO_CAP_PWM | GPIO_CAP_ADC | GPIO_CAP_INTERRUPT,
    [XIAO_C3_PIN_D6]  = GPIO_CAP_INPUT | GPIO_CAP_OUTPUT | GPIO_CAP_PULL_UP | GPIO_CAP_PULL_DOWN | GPIO_CAP_OPEN_DRAIN | GPIO_CAP_PWM | GPIO_CAP_ADC | GPIO_CAP_INTERRUPT,
    [XIAO_C3_PIN_D7]  = GPIO_CAP_INPUT | GPIO_CAP_OUTPUT | GPIO_CAP_PULL_UP | GPIO_CAP_PULL_DOWN | GPIO_CAP_OPEN_DRAIN | GPIO_CAP_PWM | GPIO_CAP_ADC | GPIO_CAP_INTERRUPT,
    [XIAO_C3_PIN_D8]  = GPIO_CAP_INPUT | GPIO_CAP_OUTPUT | GPIO_CAP_PULL_UP | GPIO_CAP_PULL_DOWN | GPIO_CAP_OPEN_DRAIN | GPIO_CAP_PWM | GPIO_CAP_ADC | GPIO_CAP_INTERRUPT,
    [XIAO_C3_PIN_D9]  = GPIO_CAP_INPUT | GPIO_CAP_OUTPUT | GPIO_CAP_PULL_UP | GPIO_CAP_PULL_DOWN | GPIO_CAP_OPEN_DRAIN | GPIO_CAP_PWM | GPIO_CAP_ADC | GPIO_CAP_INTERRUPT,
    [XIAO_C3_PIN_D10] = GPIO_CAP_INPUT | GPIO_CAP_OUTPUT | GPIO_CAP_PULL_UP | GPIO_CAP_PULL_DOWN | GPIO_CAP_OPEN_DRAIN | GPIO_CAP_PWM | GPIO_CAP_ADC | GPIO_CAP_INTERRUPT,
};
 
bool gpio_is_physical_pin_valid(uint8_t physical_pin)
{
    return (physical_pin < sizeof(xiao_c3_physical_to_logical) / sizeof(xiao_c3_physical_to_logical[0]) &&
            xiao_c3_physical_to_logical[physical_pin] != XIAO_C3_GPIO_PIN_INVALID);
}
 
bool gpio_is_logical_pin_valid(uint8_t logical_pin)
{
    return (logical_pin < XIAO_C3_PIN_MAX &&
            xiao_c3_logical_to_physical[logical_pin] != XIAO_C3_GPIO_PIN_INVALID);
}
 
uint8_t gpio_map_logical_to_physical(uint8_t logical_pin)
{
    if (logical_pin < XIAO_C3_PIN_MAX) {
        return xiao_c3_logical_to_physical[logical_pin];
    }
    return XIAO_C3_GPIO_PIN_INVALID;
}
 
uint8_t gpio_map_physical_to_logical(uint8_t physical_pin)
{
    if (physical_pin < sizeof(xiao_c3_physical_to_logical) / sizeof(xiao_c3_physical_to_logical[0])) {
        return xiao_c3_physical_to_logical[physical_pin];
    }
    return XIAO_C3_GPIO_PIN_INVALID;
}
 
const char* gpio_get_pin_name(uint8_t logical_pin)
{
    if (logical_pin < XIAO_C3_PIN_MAX) {
        return xiao_c3_pin_names[logical_pin];
    }
    return NULL;
}
 
uint32_t gpio_get_pin_capabilities(uint8_t physical_pin)
{
    uint8_t logical_pin = gpio_map_physical_to_logical(physical_pin);
    if (logical_pin < XIAO_C3_PIN_MAX) {
        return xiao_c3_pin_capabilities[logical_pin];
    }
    return 0;
}
