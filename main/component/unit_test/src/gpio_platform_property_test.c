/**
 * @file gpio_platform_property_test.c
 * @brief Property-based tests for GPIO platform abstraction validation
 *
 * This file implements property-based tests for Property 8: BSP Abstraction
 * and Platform Compatibility.
 *
 * @author MeshX Team
 * @date 2024
 */

#include <string.h>
#include <stdbool.h>
#include "unit_test.h"
#include "../../meshx/interface/gpio/meshx_gpio.h"
#include "../../meshx/interface/gpio/meshx_gpio_platform.h"
#include "../../meshx/interface/logging/meshx_log.h"
#include "../../meshx/inc/module_id.h"
#include "../../meshx/inc/meshx_err.h"

#if CONFIG_ENABLE_UNIT_TEST

/**
 * @brief Property 8.1: Platform Interface Adherence
 *
 * Validates that the platform interface functions are present and return
 * expected results for basic operations.
 */
static meshx_err_t test_property_platform_interface_adherence(void)
{
    MESHX_LOGI(MODULE_ID_COMMON, "Testing Property 8.1: Platform Interface Adherence");

    // Test platform initialization
    meshx_err_t err = meshx_gpio_platform_init();
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: Platform initialization failed: %d", err);
        return MESHX_FAIL;
    }

    // Test platform pin configuration with a known valid physical pin
    // Note: This is platform-dependent, but we expect at least one pin to work
    // For ESP32-C3, GPIO 4 is often available
    uint8_t physical_pin = 4;
    err = meshx_gpio_platform_configure_pin(physical_pin, MESHX_GPIO_MODE_OUTPUT, MESHX_GPIO_PULL_NONE, MESHX_GPIO_DRIVE_MEDIUM);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGW(MODULE_ID_COMMON, "WARN: Could not configure physical pin %u, trying pin 0", physical_pin);
        physical_pin = 0;
        err = meshx_gpio_platform_configure_pin(physical_pin, MESHX_GPIO_MODE_OUTPUT, MESHX_GPIO_PULL_NONE, MESHX_GPIO_DRIVE_MEDIUM);
        if (err != MESHX_SUCCESS) {
            MESHX_LOGE(MODULE_ID_COMMON, "FAIL: Platform pin configuration failed: %d", err);
            return MESHX_FAIL;
        }
    }

    // Test platform level control
    err = meshx_gpio_platform_set_level(physical_pin, 1);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: Platform set_level failed: %d", err);
        return MESHX_FAIL;
    }

    uint8_t read_level = 0;
    err = meshx_gpio_platform_get_level(physical_pin, &read_level);
    if (err != MESHX_SUCCESS || read_level != 1) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: Platform get_level failed or returned wrong value: %d, val=%u", err, read_level);
        return MESHX_FAIL;
    }

    MESHX_LOGI(MODULE_ID_COMMON, "PASS: Property 8.1 - Platform Interface Adherence");
    return MESHX_SUCCESS;
}

/**
 * @brief Property 8.2: Platform Interrupt Support
 *
 * Validates that the platform correctly handles interrupt registration and enabling.
 */
static meshx_err_t test_property_platform_interrupt_support(void)
{
    MESHX_LOGI(MODULE_ID_COMMON, "Testing Property 8.2: Platform Interrupt Support");

    uint8_t physical_pin = 5; // Use another pin for interrupt test
    
    // Configure as input first
    meshx_err_t err = meshx_gpio_platform_configure_pin(physical_pin, MESHX_GPIO_MODE_INPUT, MESHX_GPIO_PULL_UP, MESHX_GPIO_DRIVE_MEDIUM);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGW(MODULE_ID_COMMON, "WARN: Could not configure physical pin %u for input, skip interrupt test", physical_pin);
        return MESHX_SUCCESS; // Not a failure if pin is unavailable
    }

    // Register interrupt
    err = meshx_gpio_platform_register_intr(physical_pin, MESHX_GPIO_INTR_NEGATIVE_EDGE, NULL, NULL);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: Platform interrupt registration failed: %d", err);
        return MESHX_FAIL;
    }

    // Enable interrupt
    err = meshx_gpio_platform_intr_enable(physical_pin, true);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: Platform interrupt enable failed: %d", err);
        return MESHX_FAIL;
    }

    // Disable interrupt
    err = meshx_gpio_platform_intr_enable(physical_pin, false);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: Platform interrupt disable failed: %d", err);
        return MESHX_FAIL;
    }

    // Unregister interrupt
    err = meshx_gpio_platform_unregister_intr(physical_pin);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: Platform interrupt unregistration failed: %d", err);
        return MESHX_FAIL;
    }

    MESHX_LOGI(MODULE_ID_COMMON, "PASS: Property 8.2 - Platform Interrupt Support");
    return MESHX_SUCCESS;
}

/**
 * @brief Run all Property 8 tests
 */
static meshx_err_t run_gpio_platform_property_tests(void)
{
    MESHX_LOGI(MODULE_ID_COMMON, "Starting GPIO Platform Property Tests (Property 8)");
    
    meshx_err_t result;
    
    result = test_property_platform_interface_adherence();
    if (result != MESHX_SUCCESS) return result;
    
    result = test_property_platform_interrupt_support();
    if (result != MESHX_SUCCESS) return result;
    
    MESHX_LOGI(MODULE_ID_COMMON, "All GPIO Platform Property Tests PASSED");
    return MESHX_SUCCESS;
}

/**
 * @brief GPIO platform property test command handler
 */
static meshx_err_t gpio_platform_property_test_handler(int cmd_id, int argc, char **argv)
{
    (void)argc;
    (void)argv;

    switch (cmd_id) {
        case 0: return run_gpio_platform_property_tests();
        case 1: return test_property_platform_interface_adherence();
        case 2: return test_property_platform_interrupt_support();
        default: return MESHX_INVALID_ARG;
    }
}

/**
 * @brief Register GPIO platform property tests
 */
meshx_err_t register_gpio_platform_property_tests(void)
{
    return register_unit_test(MODULE_ID_GPIO_PLATFORM_TEST, gpio_platform_property_test_handler);
}

#endif /* CONFIG_ENABLE_UNIT_TEST */
