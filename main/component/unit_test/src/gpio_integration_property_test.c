/**
 * @file gpio_integration_property_test.c
 * @brief Property-based tests for GPIO element integration validation
 *
 * This file implements property-based tests for Property 6: Element-IO
 * Integration Consistency.
 *
 * @author MeshX Team
 * @date 2024
 */

#include <string.h>
#include <stdbool.h>
#include "unit_test.h"
#include "../../meshx/interface/gpio/meshx_gpio.h"
#include "../../meshx/io/interface/meshx_io_bridge.h"
#include "../../meshx/interface/logging/meshx_log.h"
#include "../../meshx/inc/module_id.h"
#include "../../meshx/inc/meshx_err.h"

#if CONFIG_ENABLE_UNIT_TEST

/**
 * @brief Property 6.1: IO Bridge Function Dispatch
 *
 * Validates that the IO bridge correctly dispatches commands to the 
 * underlying GPIO subsystem and maintains consistency.
 */
static meshx_err_t test_property_io_bridge_dispatch(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Testing Property 6.1: IO Bridge Function Dispatch");

    // Initialize subsystems
    meshx_err_t err = meshx_gpio_init();
    if (err != MESHX_SUCCESS) return MESHX_FAIL;

    // Use logical pin 0 which is RELAY_1 in prod_profile.yml
    uint8_t logical_pin = 0;
    
    // Create IO instance for logical pin 0
    meshx_io_config_t config = {
        .logical_pin = logical_pin,
        .io_type = 0, // GPIO
        .name = "INTEGRATION_TEST_PIN",
        .config.gpio = {
            .mode = MESHX_GPIO_MODE_OUTPUT,
            .pull = MESHX_GPIO_PULL_NONE,
            .drive = MESHX_GPIO_DRIVE_MEDIUM,
            .initial_level = 0,
            .signal_inversion = false,
        }
    };

    meshx_io_handle_t handle = meshx_io_create(&config);
    if (handle == NULL) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: IO instance creation failed");
        return MESHX_FAIL;
    }

    // Set level via IO interface
    err = meshx_io_set_level(handle, 1);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: IO set_level failed: %d", err);
        meshx_io_destroy(handle);
        return MESHX_FAIL;
    }

    // Verify level via GPIO subsystem direct API
    uint8_t direct_level = 0;
    err = meshx_gpio_get_level(logical_pin, &direct_level);
    if (err != MESHX_SUCCESS || direct_level != 1) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: GPIO direct level mismatch: %d, val=%u", err, direct_level);
        meshx_io_destroy(handle);
        return MESHX_FAIL;
    }

    // Toggle via IO interface
    err = meshx_io_toggle(handle);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: IO toggle failed: %d", err);
        meshx_io_destroy(handle);
        return MESHX_FAIL;
    }

    // Verify level again
    err = meshx_gpio_get_level(logical_pin, &direct_level);
    if (err != MESHX_SUCCESS || direct_level != 0) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: GPIO direct level mismatch after toggle: %d, val=%u", err, direct_level);
        meshx_io_destroy(handle);
        return MESHX_FAIL;
    }

    meshx_io_destroy(handle);
    MESHX_LOGI(MODULE_ID_COMMON, "PASS: Property 6.1 - IO Bridge Function Dispatch");
    return MESHX_SUCCESS;
}

/**
 * @brief Run all Property 6 tests
 */
static meshx_err_t run_gpio_integration_property_tests(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Starting GPIO Integration Property Tests (Property 6)");
    
    meshx_err_t result;
    
    result = test_property_io_bridge_dispatch();
    if (result != MESHX_SUCCESS) return result;
    
    MESHX_LOGI(MODULE_ID_COMMON, "All GPIO Integration Property Tests PASSED");
    return MESHX_SUCCESS;
}

/**
 * @brief GPIO integration property test command handler
 */
static meshx_err_t gpio_integration_property_test_handler(int cmd_id, int argc, char **argv)
{
    (void)argc;
    (void)argv;

    switch (cmd_id) {
        case 0: return run_gpio_integration_property_tests();
        case 1: return test_property_io_bridge_dispatch();
        default: return MESHX_INVALID_ARG;
    }
}

/**
 * @brief Register GPIO integration property tests
 */
meshx_err_t register_gpio_integration_property_tests(void)
{
    return register_unit_test(MODULE_ID_GPIO_ELEMENT_TEST, gpio_integration_property_test_handler);
}

#endif /* CONFIG_ENABLE_UNIT_TEST */
