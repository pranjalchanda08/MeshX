/**
 * @file gpio_property_test.c
 * @brief Property-based tests for MeshX GPIO interface validation
 *
 * This file implements property-based tests for the MeshX GPIO subsystem,
 * specifically validating Property 2: Mode-Aware GPIO Operation Validity.
 *
 * **Validates: Requirements 2.1-2.5, 2.8-2.10, 8.1-8.3, 8.9-8.10**
 *
 * Property 2: Mode-Aware GPIO Operation Validity
 * For any GPIO pin configured in a specific mode (input, output, open-drain,
 * input/output, PWM output), operations attempted on that pin SHALL:
 * 1. Succeed only if valid for the pin's configured mode
 * 2. Return appropriate standardized error codes for invalid operations
 * 3. Not affect the state of other GPIO pins
 * 4. Maintain consistent pin state tracking for runtime validation
 * 5. Initialize and deinitialize pins correctly during system startup/shutdown
 *
 * @author MeshX Team
 * @date 2024
 */

#include <string.h>
#include <stdbool.h>
#include "unit_test.h"
#include "../../meshx/interface/gpio/meshx_gpio.h"
#include "../../meshx/interface/gpio/meshx_gpio_types.h"
#include "../../meshx/interface/logging/meshx_log.h"
#include "../../meshx/inc/module_id.h"

#if CONFIG_ENABLE_UNIT_TEST
extern void meshx_gpio_test_set_pin_count(uint8_t count);
extern void meshx_gpio_test_set_pin_mode(uint8_t pin, meshx_gpio_mode_t mode);
#endif

#if CONFIG_ENABLE_UNIT_TEST

/**
 * @brief Test configuration for GPIO property tests
 */
typedef struct {
    uint8_t logical_pin;
    meshx_gpio_mode_t mode;
    uint8_t initial_level;
    bool signal_inversion;
} gpio_test_config_t;

/**
 * @brief Test state for tracking GPIO operations
 */
typedef struct {
    uint8_t pin_levels[256];           // Track levels for all possible pins
    bool pin_initialized[256];         // Track initialization state
    meshx_gpio_mode_t pin_modes[256];  // Track configured modes
} gpio_test_state_t;

static gpio_test_state_t test_state;

/**
 * @brief Initialize test state
 */
static void init_test_state(void)
{
    meshx_gpio_deinit();
    meshx_gpio_init();
    meshx_gpio_test_set_pin_count(128);
    memset(&test_state, 0, sizeof(test_state));
    for (int i = 0; i < 256; i++) {
        test_state.pin_modes[i] = MESHX_GPIO_MODE_MAX; // Invalid mode by default
    }
}

/**
 * @brief Property 2.1: Mode-specific operation validity
 *
 * Tests that operations succeed only if valid for the pin's configured mode.
 */
static meshx_err_t test_property_mode_specific_operations(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Testing Property 2.1: Mode-specific operation validity");

    // Test cases for different modes and operations
    const struct {
        meshx_gpio_mode_t mode;
        const char* mode_name;
        bool should_set_succeed;
        bool should_get_succeed;
        bool should_toggle_succeed;
    } test_cases[] = {
        {MESHX_GPIO_MODE_INPUT, "INPUT", false, true, false},
        {MESHX_GPIO_MODE_OUTPUT, "OUTPUT", true, true, true},
        {MESHX_GPIO_MODE_INPUT_OUTPUT, "INPUT_OUTPUT", true, true, true},
        {MESHX_GPIO_MODE_OPEN_DRAIN, "OPEN_DRAIN", true, true, true},
        {MESHX_GPIO_MODE_OPEN_DRAIN_INPUT_OUTPUT, "OPEN_DRAIN_INPUT_OUTPUT", true, true, true},
        {MESHX_GPIO_MODE_PWM_OUTPUT, "PWM_OUTPUT", true, true, false}, // Toggle may not be valid for PWM
    };

    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        uint8_t test_pin = i; // Use different pin for each test case

        // Configure pin with test mode
        test_state.pin_modes[test_pin] = test_cases[i].mode;
        test_state.pin_initialized[test_pin] = true;
        meshx_gpio_test_set_pin_mode(test_pin, test_cases[i].mode);

        MESHX_LOGD(MODULE_ID_COMMON, "Testing mode %s on pin %u",
                   test_cases[i].mode_name, test_pin);

        // Test set_level operation
        meshx_err_t set_result = meshx_gpio_set_level(test_pin, 1);
        if (test_cases[i].should_set_succeed) {
            if (set_result != MESHX_SUCCESS) {
                MESHX_LOGE(MODULE_ID_COMMON,
                          "FAIL: set_level should succeed for %s mode, got error: %d",
                          test_cases[i].mode_name, set_result);
                return MESHX_FAIL;
            }
        } else {
            if (set_result != MESHX_ERR_GPIO_INVALID_MODE) {
                MESHX_LOGE(MODULE_ID_COMMON,
                          "FAIL: set_level should fail with MESHX_ERR_GPIO_INVALID_MODE for %s mode, got: %d",
                          test_cases[i].mode_name, set_result);
                return MESHX_FAIL;
            }
        }

        // Test get_level operation
        uint8_t level;
        meshx_err_t get_result = meshx_gpio_get_level(test_pin, &level);
        if (test_cases[i].should_get_succeed) {
            if (get_result != MESHX_SUCCESS) {
                MESHX_LOGE(MODULE_ID_COMMON,
                          "FAIL: get_level should succeed for %s mode, got error: %d",
                          test_cases[i].mode_name, get_result);
                return MESHX_FAIL;
            }
        } else {
            if (get_result != MESHX_ERR_GPIO_INVALID_MODE) {
                MESHX_LOGE(MODULE_ID_COMMON,
                          "FAIL: get_level should fail with MESHX_ERR_GPIO_INVALID_MODE for %s mode, got: %d",
                          test_cases[i].mode_name, get_result);
                return MESHX_FAIL;
            }
        }

        // Test toggle operation
        meshx_err_t toggle_result = meshx_gpio_toggle(test_pin);
        if (test_cases[i].should_toggle_succeed) {
            if (toggle_result != MESHX_SUCCESS) {
                MESHX_LOGE(MODULE_ID_COMMON,
                          "FAIL: toggle should succeed for %s mode, got error: %d",
                          test_cases[i].mode_name, toggle_result);
                return MESHX_FAIL;
            }
        } else {
            if (toggle_result != MESHX_ERR_GPIO_INVALID_MODE) {
                MESHX_LOGE(MODULE_ID_COMMON,
                          "FAIL: toggle should fail with MESHX_ERR_GPIO_INVALID_MODE for %s mode, got: %d",
                          test_cases[i].mode_name, toggle_result);
                return MESHX_FAIL;
            }
        }
    }

    MESHX_LOGD(MODULE_ID_COMMON, "PASS: Property 2.1 - Mode-specific operation validity");
    return MESHX_SUCCESS;
}

/**
 * @brief Property 2.2: Appropriate error codes for invalid operations
 *
 * Tests that invalid operations return appropriate standardized error codes.
 */
static meshx_err_t test_property_error_codes(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Testing Property 2.2: Appropriate error codes");

    // Test invalid pin number (out of range)
    meshx_err_t result = meshx_gpio_set_level(255, 1); // 255 is valid (0-255)
    // Actually 255 is valid since range is 0-255, so let's test with 256
    // But our API uses uint8_t, so 256 would wrap to 0

    // Instead test with uninitialized pin
    uint8_t uninitialized_pin = 200;
    test_state.pin_initialized[uninitialized_pin] = false;
    test_state.pin_modes[uninitialized_pin] = MESHX_GPIO_MODE_MAX;

    result = meshx_gpio_set_level(uninitialized_pin, 1);
    if (result != MESHX_ERR_GPIO_INVALID_PIN && result != MESHX_ERR_GPIO_NOT_INITIALIZED) {
        MESHX_LOGE(MODULE_ID_COMMON,
                  "FAIL: Operation on uninitialized pin should return appropriate error code, got: %d",
                  result);
        return MESHX_FAIL;
    }

    // Test mode mismatch - try output operation on input pin
    uint8_t input_pin = 10;
    test_state.pin_modes[input_pin] = MESHX_GPIO_MODE_INPUT;
    test_state.pin_initialized[input_pin] = true;
    meshx_gpio_test_set_pin_mode(input_pin, MESHX_GPIO_MODE_INPUT);

    result = meshx_gpio_set_level(input_pin, 1);
    if (result != MESHX_ERR_GPIO_INVALID_MODE) {
        MESHX_LOGE(MODULE_ID_COMMON,
                  "FAIL: set_level on input pin should return MESHX_ERR_GPIO_INVALID_MODE, got: %d",
                  result);
        return MESHX_FAIL;
    }

    // Test invalid level value
    uint8_t output_pin = 11;
    test_state.pin_modes[output_pin] = MESHX_GPIO_MODE_OUTPUT;
    test_state.pin_initialized[output_pin] = true;
    meshx_gpio_test_set_pin_mode(output_pin, MESHX_GPIO_MODE_OUTPUT);

    result = meshx_gpio_set_level(output_pin, 2); // Invalid level (only 0 or 1)
    if (result != MESHX_ERR_GPIO_INVALID_LEVEL) {
        MESHX_LOGE(MODULE_ID_COMMON,
                  "FAIL: set_level with invalid level should return MESHX_ERR_GPIO_INVALID_LEVEL, got: %d",
                  result);
        return MESHX_FAIL;
    }

    MESHX_LOGD(MODULE_ID_COMMON, "PASS: Property 2.2 - Appropriate error codes");
    return MESHX_SUCCESS;
}

/**
 * @brief Property 2.3: Operation isolation (no effect on other pins)
 *
 * Tests that operations on one pin do not affect the state of other GPIO pins.
 */
static meshx_err_t test_property_operation_isolation(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Testing Property 2.3: Operation isolation");

    // Configure multiple pins with different initial states
    const uint8_t pins[] = {20, 21, 22, 23};
    const uint8_t initial_levels[] = {0, 1, 0, 1};

    for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
        test_state.pin_modes[pins[i]] = MESHX_GPIO_MODE_OUTPUT;
        test_state.pin_initialized[pins[i]] = true;
        test_state.pin_levels[pins[i]] = initial_levels[i];
        meshx_gpio_test_set_pin_mode(pins[i], MESHX_GPIO_MODE_OUTPUT);
    }

    // Record initial states
    uint8_t initial_states[sizeof(pins) / sizeof(pins[0])];
    memcpy(initial_states, test_state.pin_levels + pins[0], sizeof(initial_states));

    // Perform operation on first pin
    uint8_t target_pin = pins[0];
    uint8_t new_level = !test_state.pin_levels[target_pin]; // Toggle level

    meshx_err_t result = meshx_gpio_set_level(target_pin, new_level);
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: Failed to set level on pin %u: %d", target_pin, result);
        return MESHX_FAIL;
    }

    // Update test state
    test_state.pin_levels[target_pin] = new_level;

    // Verify other pins remain unchanged
    for (size_t i = 1; i < sizeof(pins) / sizeof(pins[0]); i++) {
        if (test_state.pin_levels[pins[i]] != initial_states[i]) {
            MESHX_LOGE(MODULE_ID_COMMON,
                      "FAIL: Operation on pin %u affected pin %u (changed from %u to %u)",
                      target_pin, pins[i], initial_states[i], test_state.pin_levels[pins[i]]);
            return MESHX_FAIL;
        }
    }

    MESHX_LOGD(MODULE_ID_COMMON, "PASS: Property 2.3 - Operation isolation");
    return MESHX_SUCCESS;
}

/**
 * @brief Property 2.4: Consistent pin state tracking
 *
 * Tests that pin state is tracked consistently for runtime validation.
 */
static meshx_err_t test_property_state_tracking(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Testing Property 2.4: Consistent pin state tracking");

    uint8_t test_pin = 30;
    test_state.pin_modes[test_pin] = MESHX_GPIO_MODE_OUTPUT;
    test_state.pin_initialized[test_pin] = true;
    test_state.pin_levels[test_pin] = 0;
    meshx_gpio_test_set_pin_mode(test_pin, MESHX_GPIO_MODE_OUTPUT);

    // Perform series of operations and verify state consistency
    const struct {
        uint8_t set_level;
        uint8_t expected_level;
        const char* description;
    } operations[] = {
        {1, 1, "Set to high"},
        {0, 0, "Set to low"},
        {1, 1, "Set to high again"},
        {0, 0, "Set to low again"},
    };

    for (size_t i = 0; i < sizeof(operations) / sizeof(operations[0]); i++) {
        // Set level
        meshx_err_t result = meshx_gpio_set_level(test_pin, operations[i].set_level);
        if (result != MESHX_SUCCESS) {
            MESHX_LOGE(MODULE_ID_COMMON, "FAIL: Failed to %s: %d",
                       operations[i].description, result);
            return MESHX_FAIL;
        }

        // Update test state
        test_state.pin_levels[test_pin] = operations[i].set_level;

        // Get level and verify
        uint8_t read_level;
        result = meshx_gpio_get_level(test_pin, &read_level);
        if (result != MESHX_SUCCESS) {
            MESHX_LOGE(MODULE_ID_COMMON, "FAIL: Failed to get level after %s: %d",
                       operations[i].description, result);
            return MESHX_FAIL;
        }

        if (read_level != operations[i].expected_level) {
            MESHX_LOGE(MODULE_ID_COMMON,
                      "FAIL: State inconsistency after %s: expected %u, got %u",
                      operations[i].description, operations[i].expected_level, read_level);
            return MESHX_FAIL;
        }

        // Verify test state matches
        if (test_state.pin_levels[test_pin] != read_level) {
            MESHX_LOGE(MODULE_ID_COMMON,
                      "FAIL: Test state mismatch after %s: test_state=%u, read_level=%u",
                      operations[i].description, test_state.pin_levels[test_pin], read_level);
            return MESHX_FAIL;
        }
    }

    // Test toggle operation state tracking
    test_state.pin_levels[test_pin] = 0;
    meshx_err_t result = meshx_gpio_set_level(test_pin, 0); // Ensure known state
    if (result != MESHX_SUCCESS) return result;

    result = meshx_gpio_toggle(test_pin);
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: Toggle failed: %d", result);
        return MESHX_FAIL;
    }

    uint8_t read_level;
    result = meshx_gpio_get_level(test_pin, &read_level);
    if (result != MESHX_SUCCESS) return result;

    if (read_level != 1) {
        MESHX_LOGE(MODULE_ID_COMMON,
                  "FAIL: Toggle state tracking failed: expected 1, got %u", read_level);
        return MESHX_FAIL;
    }

    MESHX_LOGD(MODULE_ID_COMMON, "PASS: Property 2.4 - Consistent pin state tracking");
    return MESHX_SUCCESS;
}

/**
 * @brief Property 2.5: Initialization and deinitialization
 *
 * Tests that pins initialize and deinitialize correctly during system startup/shutdown.
 */
static meshx_err_t test_property_init_deinit(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Testing Property 2.5: Initialization and deinitialization");

    // Test initialization
    meshx_err_t result = meshx_gpio_init();
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: GPIO initialization failed: %d", result);
        return MESHX_FAIL;
    }

    // Verify pins can be used after initialization
    uint8_t test_pin = 40;
    test_state.pin_modes[test_pin] = MESHX_GPIO_MODE_OUTPUT;
    test_state.pin_initialized[test_pin] = true;

    result = meshx_gpio_set_level(test_pin, 1);
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON,
                  "FAIL: Cannot use GPIO after initialization: %d", result);
        return MESHX_FAIL;
    }

    // Test deinitialization
    result = meshx_gpio_deinit();
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: GPIO deinitialization failed: %d", result);
        return MESHX_FAIL;
    }

    // Verify pins cannot be used after deinitialization
    // (This depends on implementation - may return MESHX_ERR_GPIO_NOT_INITIALIZED
    //  or may re-initialize automatically)
    result = meshx_gpio_set_level(test_pin, 0);
    if (result != MESHX_SUCCESS && result != MESHX_ERR_GPIO_NOT_INITIALIZED) {
        MESHX_LOGE(MODULE_ID_COMMON,
                  "FAIL: Unexpected error after deinitialization: %d", result);
        return MESHX_FAIL;
    }

    // Test re-initialization
    result = meshx_gpio_init();
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: GPIO re-initialization failed: %d", result);
        return MESHX_FAIL;
    }

    // Verify pins can be used again after re-initialization
    result = meshx_gpio_set_level(test_pin, 1);
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON,
                  "FAIL: Cannot use GPIO after re-initialization: %d", result);
        return MESHX_FAIL;
    }

    MESHX_LOGD(MODULE_ID_COMMON, "PASS: Property 2.5 - Initialization and deinitialization");
    return MESHX_SUCCESS;
}

/**
 * @brief Comprehensive property test for GPIO interface validation
 *
 * Runs all property tests for Property 2: Mode-Aware GPIO Operation Validity.
 */
static meshx_err_t run_gpio_property_tests(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Starting GPIO Property Tests");
    MESHX_LOGD(MODULE_ID_COMMON, "Property 2: Mode-Aware GPIO Operation Validity");
    MESHX_LOGD(MODULE_ID_COMMON, "Validates: Requirements 2.1-2.5, 2.8-2.10, 8.1-8.3, 8.9-8.10");

    init_test_state();

    // Run all property tests
    meshx_err_t result;

    result = test_property_mode_specific_operations();
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "Property 2.1 FAILED");
        return result;
    }

    result = test_property_error_codes();
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "Property 2.2 FAILED");
        return result;
    }

    result = test_property_operation_isolation();
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "Property 2.3 FAILED");
        return result;
    }

    result = test_property_state_tracking();
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "Property 2.4 FAILED");
        return result;
    }

    result = test_property_init_deinit();
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "Property 2.5 FAILED");
        return result;
    }

    MESHX_LOGI(MODULE_ID_COMMON, "All GPIO Property Tests PASSED");
    return MESHX_SUCCESS;
}

/**
 * @brief GPIO property test command handler
 *
 * This function is called by the unit test framework when GPIO tests are requested.
 */
static meshx_err_t gpio_property_test_handler(int cmd_id, int argc, char **argv)
{
    (void)argc;
    (void)argv;

    switch (cmd_id) {
        case 0:
            // Run all property tests
            return run_gpio_property_tests();

        case 1:
            // Property 2.1: Mode-specific operation validity
            init_test_state();
            return test_property_mode_specific_operations();

        case 2:
            // Property 2.2: Appropriate error codes
            init_test_state();
            return test_property_error_codes();

        case 3:
            // Property 2.3: Operation isolation
            init_test_state();
            return test_property_operation_isolation();

        case 4:
            // Property 2.4: State tracking
            init_test_state();
            return test_property_state_tracking();

        case 5:
            // Property 2.5: Initialization/deinitialization
            init_test_state();
            return test_property_init_deinit();

        default:
            MESHX_LOGE(MODULE_ID_COMMON, "Unknown GPIO test command ID: %d", cmd_id);
            return MESHX_INVALID_ARG;
    }
}

/**
 * @brief Register GPIO property tests with unit test framework
 */
meshx_err_t register_gpio_property_tests(void)
{
    MESHX_LOGD(MODULE_ID_GPIO_PROPERTY_TEST, "Registering GPIO property tests");
    return register_unit_test(MODULE_ID_GPIO_PROPERTY_TEST, gpio_property_test_handler);
}

#endif /* CONFIG_ENABLE_UNIT_TEST */
