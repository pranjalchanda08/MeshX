/**
 * Copyright (c) 2024 - 2026 MeshX
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file meshx_gpio_zephyr.c
 * @brief Zephyr GPIO Platform Implementation
 */

#include "interface/gpio/meshx_gpio_platform.h"
#include "interface/logging/meshx_log.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <string.h>

#define MODULE_ID_PLATFORM_GPIO MODULE_ID_COMMON
#define ZEPHYR_GPIO_MAX_PINS 32

typedef struct {
    bool registered;
    void (*isr_handler)(void *);
    void *arg;
    struct gpio_callback cb_data;
} zephyr_gpio_intr_state_t;

static zephyr_gpio_intr_state_t g_intr_states[ZEPHYR_GPIO_MAX_PINS];
static const struct device *g_gpio_dev = NULL;

static void zephyr_gpio_cb_handler(const struct device *port, struct gpio_callback *cb, gpio_port_pins_t pins)
{
    ARG_UNUSED(port);
    for (uint8_t pin = 0; pin < ZEPHYR_GPIO_MAX_PINS; pin++) {
        if ((pins & BIT(pin)) && g_intr_states[pin].registered && g_intr_states[pin].isr_handler) {
            g_intr_states[pin].isr_handler(g_intr_states[pin].arg);
        }
    }
}

meshx_err_t meshx_gpio_platform_init(void)
{
#if DT_NODE_EXISTS(DT_NODELABEL(gpio0))
    g_gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    if (!device_is_ready(g_gpio_dev)) {
        MESHX_LOGE(MODULE_ID_PLATFORM_GPIO, "GPIO device gpio0 not ready");
        return MESHX_ERR_PLAT;
    }
    memset(g_intr_states, 0, sizeof(g_intr_states));
    return MESHX_SUCCESS;
#else
    MESHX_LOGW(MODULE_ID_PLATFORM_GPIO, "No gpio0 device node defined in devicetree");
    return MESHX_SUCCESS;
#endif
}

meshx_err_t meshx_gpio_platform_deinit(void)
{
    g_gpio_dev = NULL;
    return MESHX_SUCCESS;
}

meshx_err_t meshx_gpio_platform_set_level(uint8_t physical_pin, uint8_t level)
{
    if (!g_gpio_dev || physical_pin >= ZEPHYR_GPIO_MAX_PINS) {
        return MESHX_INVALID_ARG;
    }
    int rc = gpio_pin_set_raw(g_gpio_dev, physical_pin, level ? 1 : 0);
    return (rc == 0) ? MESHX_SUCCESS : MESHX_ERR_PLAT;
}

meshx_err_t meshx_gpio_platform_get_level(uint8_t physical_pin, uint8_t *level)
{
    if (!g_gpio_dev || physical_pin >= ZEPHYR_GPIO_MAX_PINS || !level) {
        return MESHX_INVALID_ARG;
    }
    int val = gpio_pin_get_raw(g_gpio_dev, physical_pin);
    if (val < 0) {
        return MESHX_ERR_PLAT;
    }
    *level = (uint8_t)val;
    return MESHX_SUCCESS;
}

meshx_err_t meshx_gpio_platform_configure_pin(uint8_t physical_pin,
                                              meshx_gpio_mode_t mode,
                                              meshx_gpio_pull_t pull,
                                              meshx_gpio_drive_t drive)
{
    ARG_UNUSED(drive);
    if (!g_gpio_dev || physical_pin >= ZEPHYR_GPIO_MAX_PINS) {
        return MESHX_INVALID_ARG;
    }

    gpio_flags_t flags = 0;
    switch (mode) {
        case MESHX_GPIO_MODE_INPUT:
            flags |= GPIO_INPUT;
            break;
        case MESHX_GPIO_MODE_OUTPUT:
            flags |= GPIO_OUTPUT;
            break;
        case MESHX_GPIO_MODE_INPUT_OUTPUT:
            flags |= (GPIO_INPUT | GPIO_OUTPUT);
            break;
        case MESHX_GPIO_MODE_OPEN_DRAIN:
            flags |= (GPIO_OUTPUT | GPIO_OPEN_DRAIN);
            break;
        case MESHX_GPIO_MODE_OPEN_DRAIN_INPUT_OUTPUT:
            flags |= (GPIO_INPUT | GPIO_OUTPUT | GPIO_OPEN_DRAIN);
            break;
        default:
            flags |= GPIO_DISCONNECTED;
            break;
    }

    switch (pull) {
        case MESHX_GPIO_PULL_UP:
            flags |= GPIO_PULL_UP;
            break;
        case MESHX_GPIO_PULL_DOWN:
            flags |= GPIO_PULL_DOWN;
            break;
        default:
            break;
    }

    int rc = gpio_pin_configure(g_gpio_dev, physical_pin, flags);
    return (rc == 0) ? MESHX_SUCCESS : MESHX_ERR_PLAT;
}

meshx_err_t meshx_gpio_platform_register_intr(uint8_t physical_pin,
                                              meshx_gpio_intr_type_t intr_type,
                                              void (*isr_handler)(void*),
                                              void *arg)
{
    if (!g_gpio_dev || physical_pin >= ZEPHYR_GPIO_MAX_PINS) {
        return MESHX_INVALID_ARG;
    }

    g_intr_states[physical_pin].registered = true;
    g_intr_states[physical_pin].isr_handler = isr_handler;
    g_intr_states[physical_pin].arg = arg;

    gpio_flags_t trig_flags = GPIO_INT_DISABLE;
    switch (intr_type) {
        case MESHX_GPIO_INTR_POSITIVE_EDGE:
            trig_flags = GPIO_INT_EDGE_RISING;
            break;
        case MESHX_GPIO_INTR_NEGATIVE_EDGE:
            trig_flags = GPIO_INT_EDGE_FALLING;
            break;
        case MESHX_GPIO_INTR_ANY_EDGE:
            trig_flags = GPIO_INT_EDGE_BOTH;
            break;
        case MESHX_GPIO_INTR_LOW_LEVEL:
            trig_flags = GPIO_INT_LEVEL_LOW;
            break;
        case MESHX_GPIO_INTR_HIGH_LEVEL:
            trig_flags = GPIO_INT_LEVEL_HIGH;
            break;
        default:
            trig_flags = GPIO_INT_DISABLE;
            break;
    }

    gpio_init_callback(&g_intr_states[physical_pin].cb_data, zephyr_gpio_cb_handler, BIT(physical_pin));
    gpio_add_callback(g_gpio_dev, &g_intr_states[physical_pin].cb_data);
    gpio_pin_interrupt_configure(g_gpio_dev, physical_pin, trig_flags);

    return MESHX_SUCCESS;
}

meshx_err_t meshx_gpio_platform_unregister_intr(uint8_t physical_pin)
{
    if (!g_gpio_dev || physical_pin >= ZEPHYR_GPIO_MAX_PINS) {
        return MESHX_INVALID_ARG;
    }

    gpio_pin_interrupt_configure(g_gpio_dev, physical_pin, GPIO_INT_DISABLE);
    gpio_remove_callback(g_gpio_dev, &g_intr_states[physical_pin].cb_data);
    g_intr_states[physical_pin].registered = false;
    g_intr_states[physical_pin].isr_handler = NULL;

    return MESHX_SUCCESS;
}

meshx_err_t meshx_gpio_platform_intr_enable(uint8_t physical_pin, bool enable)
{
    if (!g_gpio_dev || physical_pin >= ZEPHYR_GPIO_MAX_PINS) {
        return MESHX_INVALID_ARG;
    }

    return gpio_pin_interrupt_configure(g_gpio_dev, physical_pin,
                                        enable ? GPIO_INT_EDGE_BOTH : GPIO_INT_DISABLE) == 0
               ? MESHX_SUCCESS : MESHX_ERR_PLAT;
}

meshx_err_t meshx_io_platform_execute_function(uint8_t physical_pin,
                                               meshx_io_function_t function,
                                               const uint32_t *args,
                                               uint8_t arg_count)
{
    if (physical_pin >= ZEPHYR_GPIO_MAX_PINS) {
        return MESHX_ERR_GPIO_INVALID_PIN;
    }

    switch (function) {
        case MESHX_IO_FUNCTION_SET_LEVEL:
            if (arg_count < 1 || args == NULL) {
                return MESHX_INVALID_ARG;
            }
            return meshx_gpio_platform_set_level(physical_pin, (uint8_t)args[0]);

        case MESHX_IO_FUNCTION_GET_LEVEL:
            if (arg_count < 1 || args == NULL) {
                return MESHX_INVALID_ARG;
            }
            return MESHX_ERR_GPIO_INVALID_MODE;

        case MESHX_IO_FUNCTION_TOGGLE:
            {
                uint8_t current_level = 0;
                meshx_err_t err = meshx_gpio_platform_get_level(physical_pin, &current_level);
                if (err != MESHX_SUCCESS) {
                    return err;
                }
                return meshx_gpio_platform_set_level(physical_pin, !current_level);
            }

        case MESHX_IO_FUNCTION_SET_PWM_DUTY:
        case MESHX_IO_FUNCTION_SET_PWM_FREQUENCY:
        case MESHX_IO_FUNCTION_REGISTER_INTERRUPT:
#if CONFIG_ENABLE_UNIT_TEST
            return MESHX_SUCCESS;
#else
            return MESHX_ERR_GPIO_INVALID_MODE;
#endif

        case MESHX_IO_FUNCTION_CUSTOM:
            return MESHX_NOT_SUPPORTED;

        default:
            return MESHX_NOT_SUPPORTED;
    }
}

meshx_err_t meshx_io_platform_register_custom_function(uint16_t function_id,
                                                       meshx_err_t (*handler)(uint8_t physical_pin,
                                                                              const uint32_t *args,
                                                                              uint8_t arg_count,
                                                                              void *user_data),
                                                       void *user_data)
{
    ARG_UNUSED(function_id);
    ARG_UNUSED(handler);
    ARG_UNUSED(user_data);
    return MESHX_NOT_SUPPORTED;
}

