/*
 * GPIO constraints implementation for template BSP
 *
 * This is a template file. When creating a new BSP from this template,
 * implement the actual GPIO constraints for your board here.
 *
 * The functions in this file define GPIO pin constraints and validation
 * for the specific board.
 */

#include "gpio_constraints.h"
#include "gpio_pin_map.h"

// Template implementation - to be filled in for actual BSP
bool gpio_is_valid_for_output(uint8_t gpio_num)
{
    // TODO: Implement actual GPIO validation for your board
    // Check if the GPIO pin is valid for output mode
    return true;
}

bool gpio_is_valid_for_input(uint8_t gpio_num)
{
    // TODO: Implement actual GPIO validation for your board
    // Check if the GPIO pin is valid for input mode
    return true;
}

bool gpio_is_valid_for_pwm(uint8_t gpio_num)
{
    // TODO: Implement actual GPIO validation for your board
    // Check if the GPIO pin supports PWM
    return true;
}

const gpio_constraint_t* gpio_get_constraints(uint8_t gpio_num)
{
    // TODO: Return actual constraints for the GPIO pin
    static gpio_constraint_t default_constraint = {
        .valid_for_output = true,
        .valid_for_input = true,
        .valid_for_pwm = false,
        .pullup_available = true,
        .pulldown_available = true,
        .open_drain_supported = true
    };

    return &default_constraint;
}
