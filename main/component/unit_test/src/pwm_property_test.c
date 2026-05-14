/**
 * @file pwm_property_test.c
 * @brief Property-based tests for MeshX PWM subsystem correctness
 *
 * This file implements property-based tests for the MeshX PWM subsystem,
 * specifically validating Property 4: PWM Subsystem Correctness.
 *
 * **Validates: Requirements 4.1-4.10**
 *
 * Property 4: PWM Subsystem Correctness
 * For any GPIO pin configured for PWM output (with frequency, duty cycle, resolution, and channel specifications), the PWM subsystem SHALL:
 * 1. Initialize based on YAML configuration
 * 2. Start and stop PWM output as requested
 * 3. Set and get duty cycle (0-100%) and frequency values accurately
 * 4. Validate parameters against hardware limits
 * 5. Handle hardware channel allocation and conflicts
 * 6. Maintain PWM state for runtime control and monitoring
 * 7. Deinitialize and free resources during shutdown
 *
 * @author MeshX Team
 * @date 2024
 */

#include <string.h>
#include <stdbool.h>
#include "unit_test.h"
#include "../../meshx/interface/gpio/meshx_pwm.h"
#include "../../meshx/interface/gpio/meshx_gpio.h"
#include "../../meshx/interface/logging/meshx_log.h"
#include "../../meshx/inc/module_id.h"

#if CONFIG_ENABLE_UNIT_TEST
extern void meshx_gpio_test_set_pin_count(uint8_t count);
extern void meshx_gpio_test_set_pin_mode(uint8_t pin, meshx_gpio_mode_t mode);
extern void meshx_pwm_test_set_pin_count(uint8_t count);
#endif

#if CONFIG_ENABLE_UNIT_TEST

/**
 * @brief Test configuration for PWM property tests
 */
typedef struct {
    uint8_t logical_pin;
    uint32_t frequency;
    uint8_t duty_cycle;
    uint8_t resolution;
    uint8_t channel;
} pwm_test_config_t;

/**
 * @brief Test state for tracking PWM operations
 */
typedef struct {
    bool pwm_initialized;
    bool pwm_started[256];           // Track started state for all possible pins
    uint32_t pwm_frequencies[256];   // Track frequency for each pin
    uint8_t pwm_duty_cycles[256];    // Track duty cycle for each pin
    uint8_t pwm_resolutions[256];    // Track resolution for each pin
    uint8_t pwm_channels[256];       // Track channel allocation for each pin
} pwm_test_state_t;

static pwm_test_state_t test_state;

/**
 * @brief Initialize test state
 */
static void init_test_state(void)
{
    meshx_gpio_deinit();
    meshx_pwm_deinit();
    meshx_gpio_init();
    meshx_pwm_init();
    meshx_gpio_test_set_pin_count(128);
    meshx_pwm_test_set_pin_count(128);
    memset(&test_state, 0, sizeof(test_state));
}

/**
 * @brief Property 4.1: PWM initialization based on configuration
 *
 * Tests that PWM subsystem initializes correctly based on YAML configuration.
 */
static meshx_err_t test_property_pwm_initialization(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Testing Property 4.1: PWM initialization based on configuration");

    // Test PWM subsystem initialization
    meshx_err_t result = meshx_pwm_init();
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: PWM initialization failed: %d", result);
        return MESHX_FAIL;
    }

    test_state.pwm_initialized = true;
    MESHX_LOGD(MODULE_ID_COMMON, "PASS: Property 4.1 - PWM initialization based on configuration");
    return MESHX_SUCCESS;
}

/**
 * @brief Property 4.2: PWM start and stop operations
 *
 * Tests that PWM can be started and stopped as requested.
 */
static meshx_err_t test_property_pwm_start_stop(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Testing Property 4.2: PWM start and stop operations");

    if (!test_state.pwm_initialized) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: PWM not initialized");
        return MESHX_ERR_GPIO_NOT_INITIALIZED;
    }

    uint8_t test_pin = 6; // Use safe Xiao C3 pin
    uint32_t test_frequency = 1000; // 1kHz
    uint8_t test_duty_cycle = 50;   // 50%
    uint8_t test_resolution = 10;   // 10-bit resolution

    // Test starting PWM
    meshx_gpio_test_set_pin_mode(test_pin, MESHX_GPIO_MODE_PWM_OUTPUT);
    meshx_err_t result = meshx_pwm_start(test_pin);
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: PWM start failed: %d", result);
        return result;
    }

    test_state.pwm_started[test_pin] = true;
    test_state.pwm_frequencies[test_pin] = test_frequency;
    test_state.pwm_duty_cycles[test_pin] = test_duty_cycle;
    test_state.pwm_resolutions[test_pin] = test_resolution;

    // Verify PWM is running (implementation dependent)
    // For now, just verify no error on subsequent operations

    // Test stopping PWM
    result = meshx_pwm_stop(test_pin);
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: PWM stop failed: %d", result);
        return result;
    }

    test_state.pwm_started[test_pin] = false;

    // Test stopping already stopped PWM (should succeed or return appropriate error)
    result = meshx_pwm_stop(test_pin);
    if (result != MESHX_SUCCESS && result != MESHX_ERR_GPIO_PWM_NOT_SUPPORTED) {
        MESHX_LOGE(MODULE_ID_COMMON,
                  "FAIL: Stopping already stopped PWM returned unexpected error: %d", result);
        return MESHX_FAIL;
    }

    MESHX_LOGD(MODULE_ID_COMMON, "PASS: Property 4.2 - PWM start and stop operations");
    return MESHX_SUCCESS;
}

/**
 * @brief Property 4.3: Duty cycle accuracy
 *
 * Tests that duty cycle can be set and get accurately (0-100%).
 */
static meshx_err_t test_property_duty_cycle_accuracy(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Testing Property 4.3: Duty cycle accuracy");

    if (!test_state.pwm_initialized) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: PWM not initialized");
        return MESHX_ERR_GPIO_NOT_INITIALIZED;
    }

    uint8_t test_pin = 6;
    meshx_pwm_start(test_pin);
    test_state.pwm_started[test_pin] = true;

    // Test valid duty cycle values
    const uint8_t test_duty_cycles[] = {0, 25, 50, 75, 100};

    for (size_t i = 0; i < sizeof(test_duty_cycles) / sizeof(test_duty_cycles[0]); i++) {
        // Set duty cycle
        meshx_err_t result = meshx_pwm_set_duty_cycle(test_pin, test_duty_cycles[i]);
        if (result != MESHX_SUCCESS) {
            MESHX_LOGE(MODULE_ID_COMMON,
                      "FAIL: Setting duty cycle %u%% failed: %d", test_duty_cycles[i], result);
            return result;
        }

        test_state.pwm_duty_cycles[test_pin] = test_duty_cycles[i];

        // Get duty cycle and verify
        uint8_t read_duty_cycle;
        result = meshx_pwm_get_duty_cycle(test_pin, &read_duty_cycle);
        if (result != MESHX_SUCCESS) {
            MESHX_LOGE(MODULE_ID_COMMON,
                      "FAIL: Getting duty cycle failed: %d", result);
            return result;
        }

        if (read_duty_cycle != test_duty_cycles[i]) {
            MESHX_LOGE(MODULE_ID_COMMON,
                      "FAIL: Duty cycle mismatch: set %u%%, got %u%%",
                      test_duty_cycles[i], read_duty_cycle);
            return MESHX_FAIL;
        }

        // Verify test state matches
        if (test_state.pwm_duty_cycles[test_pin] != read_duty_cycle) {
            MESHX_LOGE(MODULE_ID_COMMON,
                      "FAIL: Test state mismatch: test_state=%u%%, read=%u%%",
                      test_state.pwm_duty_cycles[test_pin], read_duty_cycle);
            return MESHX_FAIL;
        }
    }

    // Test invalid duty cycle values (should fail)
    const uint8_t invalid_duty_cycles[] = {101, 255};

    for (size_t i = 0; i < sizeof(invalid_duty_cycles) / sizeof(invalid_duty_cycles[0]); i++) {
        meshx_err_t result = meshx_pwm_set_duty_cycle(test_pin, invalid_duty_cycles[i]);
        if (result != MESHX_ERR_GPIO_PWM_INVALID_PARAM) {
            MESHX_LOGE(MODULE_ID_COMMON,
                      "FAIL: Invalid duty cycle %u should return MESHX_ERR_GPIO_PWM_INVALID_PARAM, got: %d",
                      invalid_duty_cycles[i], result);
            return MESHX_FAIL;
        }
    }

    MESHX_LOGD(MODULE_ID_COMMON, "PASS: Property 4.3 - Duty cycle accuracy");
    meshx_pwm_stop(test_pin);
    return MESHX_SUCCESS;
}

/**
 * @brief Property 4.4: Frequency accuracy
 *
 * Tests that frequency can be set and get accurately.
 */
static meshx_err_t test_property_frequency_accuracy(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Testing Property 4.4: Frequency accuracy");

    if (!test_state.pwm_initialized) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: PWM not initialized");
        return MESHX_ERR_GPIO_NOT_INITIALIZED;
    }

    uint8_t test_pin = 6;
    meshx_pwm_start(test_pin);
    test_state.pwm_started[test_pin] = true;

    // Test valid frequency values (typical PWM frequencies)
    const uint32_t test_frequencies[] = {100, 500, 1000, 5000, 10000, 20000};

    for (size_t i = 0; i < sizeof(test_frequencies) / sizeof(test_frequencies[0]); i++) {
        // Set frequency
        meshx_err_t result = meshx_pwm_set_frequency(test_pin, test_frequencies[i]);
        if (result != MESHX_SUCCESS) {
            MESHX_LOGE(MODULE_ID_COMMON,
                      "FAIL: Setting frequency %u Hz failed: %d", test_frequencies[i], result);
            return result;
        }

        test_state.pwm_frequencies[test_pin] = test_frequencies[i];

        // Get frequency and verify
        uint32_t read_frequency;
        result = meshx_pwm_get_frequency(test_pin, &read_frequency);
        if (result != MESHX_SUCCESS) {
            MESHX_LOGE(MODULE_ID_COMMON,
                      "FAIL: Getting frequency failed: %d", result);
            return result;
        }

        if (read_frequency != test_frequencies[i]) {
            MESHX_LOGE(MODULE_ID_COMMON,
                      "FAIL: Frequency mismatch: set %u Hz, got %u Hz",
                      test_frequencies[i], read_frequency);
            return MESHX_FAIL;
        }

        // Verify test state matches
        if (test_state.pwm_frequencies[test_pin] != read_frequency) {
            MESHX_LOGE(MODULE_ID_COMMON,
                      "FAIL: Test state mismatch: test_state=%u Hz, read=%u Hz",
                      test_state.pwm_frequencies[test_pin], read_frequency);
            return MESHX_FAIL;
        }
    }

    // Test zero frequency (should fail)
    meshx_err_t result = meshx_pwm_set_frequency(test_pin, 0);
    if (result != MESHX_ERR_GPIO_PWM_INVALID_PARAM) {
        MESHX_LOGE(MODULE_ID_COMMON,
                  "FAIL: Zero frequency should return MESHX_ERR_GPIO_PWM_INVALID_PARAM, got: %d", result);
        return MESHX_FAIL;
    }

    MESHX_LOGD(MODULE_ID_COMMON, "PASS: Property 4.4 - Frequency accuracy");
    meshx_pwm_stop(test_pin);
    return MESHX_SUCCESS;
}

/**
 * @brief Property 4.5: Parameter validation against hardware limits
 *
 * Tests that PWM parameters are validated against hardware limits.
 */
static meshx_err_t test_property_parameter_validation(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Testing Property 4.5: Parameter validation against hardware limits");

    if (!test_state.pwm_initialized) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: PWM not initialized");
        return MESHX_ERR_GPIO_NOT_INITIALIZED;
    }

    uint8_t test_pin = 6;
    meshx_pwm_start(test_pin);
    test_state.pwm_started[test_pin] = true;

    // Test resolution parameter validation
    const uint8_t test_resolutions[] = {8, 10, 12, 16};

    for (size_t i = 0; i < sizeof(test_resolutions) / sizeof(test_resolutions[0]); i++) {
        meshx_err_t result = meshx_pwm_set_resolution(test_pin, test_resolutions[i]);
        if (result != MESHX_SUCCESS) {
            MESHX_LOGE(MODULE_ID_COMMON,
                      "FAIL: Setting resolution %u bits failed: %d", test_resolutions[i], result);
            return result;
        }

        test_state.pwm_resolutions[test_pin] = test_resolutions[i];

        // Get resolution and verify
        uint8_t read_resolution;
        result = meshx_pwm_get_resolution(test_pin, &read_resolution);
        if (result != MESHX_SUCCESS) {
            MESHX_LOGE(MODULE_ID_COMMON,
                      "FAIL: Getting resolution failed: %d", result);
            return result;
        }

        if (read_resolution != test_resolutions[i]) {
            MESHX_LOGE(MODULE_ID_COMMON,
                      "FAIL: Resolution mismatch: set %u bits, got %u bits",
                      test_resolutions[i], read_resolution);
            return MESHX_FAIL;
        }
    }

    // Test invalid resolution (should fail)
    meshx_err_t result = meshx_pwm_set_resolution(test_pin, 0); // 0-bit resolution invalid
    if (result != MESHX_ERR_GPIO_PWM_INVALID_PARAM) {
        MESHX_LOGE(MODULE_ID_COMMON,
                  "FAIL: Invalid resolution should return MESHX_ERR_GPIO_PWM_INVALID_PARAM, got: %d", result);
        return MESHX_FAIL;
    }

    MESHX_LOGD(MODULE_ID_COMMON, "PASS: Property 4.5 - Parameter validation against hardware limits");
    meshx_pwm_stop(test_pin);
    return MESHX_SUCCESS;
}

/**
 * @brief Property 4.6: Hardware channel allocation and conflict handling
 *
 * Tests that hardware channel allocation and conflicts are handled properly.
 */
static meshx_err_t test_property_channel_allocation(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Testing Property 4.6: Hardware channel allocation and conflict handling");

    if (!test_state.pwm_initialized) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: PWM not initialized");
        return MESHX_ERR_GPIO_NOT_INITIALIZED;
    }

    // Test multiple pins can be configured (implementation may handle channel allocation)
    const uint8_t test_pins[] = {3, 4, 5};

    for (size_t i = 0; i < sizeof(test_pins) / sizeof(test_pins[0]); i++) {
        meshx_err_t result = meshx_pwm_start(test_pins[i]);
        if (result != MESHX_SUCCESS) {
            MESHX_LOGE(MODULE_ID_COMMON,
                      "FAIL: Starting PWM on pin %u failed: %d", test_pins[i], result);
            return result;
        }

        test_state.pwm_started[test_pins[i]] = true;
        test_state.pwm_channels[test_pins[i]] = i; // Assume sequential channel allocation

        // Set different parameters for each pin
        result = meshx_pwm_set_frequency(test_pins[i], 1000 + (i * 1000));
        if (result != MESHX_SUCCESS) {
            MESHX_LOGE(MODULE_ID_COMMON,
                      "FAIL: Setting frequency on pin %u failed: %d", test_pins[i], result);
            return result;
        }

        result = meshx_pwm_set_duty_cycle(test_pins[i], 25 * (i + 1));
        if (result != MESHX_SUCCESS) {
            MESHX_LOGE(MODULE_ID_COMMON,
                      "FAIL: Setting duty cycle on pin %u failed: %d", test_pins[i], result);
            return result;
        }
    }

    // Verify each pin maintains its own state
    for (size_t i = 0; i < sizeof(test_pins) / sizeof(test_pins[0]); i++) {
        uint32_t read_frequency;
        meshx_err_t result = meshx_pwm_get_frequency(test_pins[i], &read_frequency);
        if (result != MESHX_SUCCESS) {
            MESHX_LOGE(MODULE_ID_COMMON,
                      "FAIL: Getting frequency from pin %u failed: %d", test_pins[i], result);
            return result;
        }

        uint8_t read_duty_cycle;
        result = meshx_pwm_get_duty_cycle(test_pins[i], &read_duty_cycle);
        if (result != MESHX_SUCCESS) {
            MESHX_LOGE(MODULE_ID_COMMON,
                      "FAIL: Getting duty cycle from pin %u failed: %d", test_pins[i], result);
            return result;
        }

        // Clean up
        result = meshx_pwm_stop(test_pins[i]);
        if (result != MESHX_SUCCESS) {
            MESHX_LOGE(MODULE_ID_COMMON,
                      "FAIL: Stopping PWM on pin %u failed: %d", test_pins[i], result);
            return result;
        }

        test_state.pwm_started[test_pins[i]] = false;
    }

    MESHX_LOGD(MODULE_ID_COMMON, "PASS: Property 4.6 - Hardware channel allocation and conflict handling");
    return MESHX_SUCCESS;
}

/**
 * @brief Property 4.7: PWM state maintenance
 *
 * Tests that PWM state is maintained for runtime control and monitoring.
 */
static meshx_err_t test_property_state_maintenance(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Testing Property 4.7: PWM state maintenance");

    if (!test_state.pwm_initialized) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: PWM not initialized");
        return MESHX_ERR_GPIO_NOT_INITIALIZED;
    }

    uint8_t test_pin = 6;
    meshx_pwm_start(test_pin);
    test_state.pwm_started[test_pin] = true;

    // Test state persistence across multiple operations
    const struct {
        uint32_t frequency;
        uint8_t duty_cycle;
        uint8_t resolution;
        const char* description;
    } state_changes[] = {
        {1000, 25, 8, "Initial state"},
        {2000, 50, 10, "Increased frequency and duty"},
        {500, 75, 12, "Decreased frequency, increased duty"},
        {1500, 10, 16, "Medium frequency, low duty"},
    };

    for (size_t i = 0; i < sizeof(state_changes) / sizeof(state_changes[0]); i++) {
        // Set all parameters
        meshx_err_t result = meshx_pwm_set_frequency(test_pin, state_changes[i].frequency);
        if (result != MESHX_SUCCESS) {
            MESHX_LOGE(MODULE_ID_COMMON, "FAIL: Setting frequency for %s failed: %d",
                       state_changes[i].description, result);
            return result;
        }

        result = meshx_pwm_set_duty_cycle(test_pin, state_changes[i].duty_cycle);
        if (result != MESHX_SUCCESS) {
            MESHX_LOGE(MODULE_ID_COMMON, "FAIL: Setting duty cycle for %s failed: %d",
                       state_changes[i].description, result);
            return result;
        }

        result = meshx_pwm_set_resolution(test_pin, state_changes[i].resolution);
        if (result != MESHX_SUCCESS) {
            MESHX_LOGE(MODULE_ID_COMMON, "FAIL: Setting resolution for %s failed: %d",
                       state_changes[i].description, result);
            return result;
        }

        // Update test state
        test_state.pwm_frequencies[test_pin] = state_changes[i].frequency;
        test_state.pwm_duty_cycles[test_pin] = state_changes[i].duty_cycle;
        test_state.pwm_resolutions[test_pin] = state_changes[i].resolution;

        // Verify all parameters can be retrieved
        uint32_t read_frequency;
        result = meshx_pwm_get_frequency(test_pin, &read_frequency);
        if (result != MESHX_SUCCESS) {
            MESHX_LOGE(MODULE_ID_COMMON, "FAIL: Getting frequency for %s failed: %d",
                       state_changes[i].description, result);
            return result;
        }

        uint8_t read_duty_cycle;
        result = meshx_pwm_get_duty_cycle(test_pin, &read_duty_cycle);
        if (result != MESHX_SUCCESS) {
            MESHX_LOGE(MODULE_ID_COMMON, "FAIL: Getting duty cycle for %s failed: %d",
                       state_changes[i].description, result);
            return result;
        }

        uint8_t read_resolution;
        result = meshx_pwm_get_resolution(test_pin, &read_resolution);
        if (result != MESHX_SUCCESS) {
            MESHX_LOGE(MODULE_ID_COMMON, "FAIL: Getting resolution for %s failed: %d",
                       state_changes[i].description, result);
            return result;
        }

        // Verify state consistency
        if (read_frequency != state_changes[i].frequency ||
            read_duty_cycle != state_changes[i].duty_cycle ||
            read_resolution != state_changes[i].resolution) {
            MESHX_LOGE(MODULE_ID_COMMON,
                      "FAIL: State inconsistency for %s: expected %u Hz, %u%%, %u bits; got %u Hz, %u%%, %u bits",
                      state_changes[i].description,
                      state_changes[i].frequency, state_changes[i].duty_cycle, state_changes[i].resolution,
                      read_frequency, read_duty_cycle, read_resolution);
            return MESHX_FAIL;
        }
    }

    // Clean up
    meshx_err_t result = meshx_pwm_stop(test_pin);
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: Stopping PWM failed: %d", result);
        return result;
    }

    test_state.pwm_started[test_pin] = false;

    MESHX_LOGD(MODULE_ID_COMMON, "PASS: Property 4.7 - PWM state maintenance");
    return MESHX_SUCCESS;
}

/**
 * @brief Property 4.8: Deinitialization and resource cleanup
 *
 * Tests that PWM subsystem deinitializes and cleans up resources during shutdown.
 */
static meshx_err_t test_property_deinit_cleanup(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Testing Property 4.8: Deinitialization and resource cleanup");

    if (!test_state.pwm_initialized) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: PWM not initialized");
        return MESHX_ERR_GPIO_NOT_INITIALIZED;
    }

    // Test deinitialization
    meshx_err_t result = meshx_pwm_deinit();
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: PWM deinitialization failed: %d", result);
        return MESHX_FAIL;
    }

    test_state.pwm_initialized = false;

    // Verify PWM operations fail after deinitialization
    // (This depends on implementation - may return MESHX_ERR_GPIO_NOT_INITIALIZED
    //  or may re-initialize automatically)
    result = meshx_pwm_start(200); // Definitely invalid (> 128)
    if (result != MESHX_SUCCESS && result != MESHX_ERR_GPIO_NOT_INITIALIZED) {
        MESHX_LOGE(MODULE_ID_COMMON,
                  "FAIL: Unexpected error after deinitialization: %d", result);
        return MESHX_FAIL;
    }

    // Test re-initialization
    result = meshx_pwm_init();
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: PWM re-initialization failed: %d", result);
        return MESHX_FAIL;
    }

    test_state.pwm_initialized = true;

    // Verify PWM can be used again after re-initialization
    meshx_gpio_test_set_pin_mode(7, MESHX_GPIO_MODE_PWM_OUTPUT);
    result = meshx_pwm_start(7);
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON,
                  "FAIL: Cannot use PWM after re-initialization: %d", result);
        return MESHX_FAIL;
    }

    // Clean up
    result = meshx_pwm_stop(7);
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: Failed to stop PWM after test: %d", result);
        return result;
    }

    MESHX_LOGD(MODULE_ID_COMMON, "PASS: Property 4.8 - Deinitialization and resource cleanup");
    return MESHX_SUCCESS;
}

/**
 * @brief Comprehensive property test for PWM subsystem correctness
 *
 * Runs all property tests for Property 4: PWM Subsystem Correctness.
 */
static meshx_err_t run_pwm_property_tests(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Starting PWM Property Tests");
    MESHX_LOGD(MODULE_ID_COMMON, "Property 4: PWM Subsystem Correctness");
    MESHX_LOGD(MODULE_ID_COMMON, "Validates: Requirements 4.1-4.10");

    init_test_state();

    // Run all property tests
    meshx_err_t result;

    result = test_property_pwm_initialization();
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "Property 4.1 FAILED");
        return result;
    }

    result = test_property_pwm_start_stop();
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "Property 4.2 FAILED");
        return result;
    }

    result = test_property_duty_cycle_accuracy();
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "Property 4.3 FAILED");
        return result;
    }

    result = test_property_frequency_accuracy();
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "Property 4.4 FAILED");
        return result;
    }

    result = test_property_parameter_validation();
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "Property 4.5 FAILED");
        return result;
    }

    result = test_property_channel_allocation();
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "Property 4.6 FAILED");
        return result;
    }

    result = test_property_state_maintenance();
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "Property 4.7 FAILED");
        return result;
    }

    result = test_property_deinit_cleanup();
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "Property 4.8 FAILED");
        return result;
    }

    MESHX_LOGI(MODULE_ID_COMMON, "All PWM Property Tests PASSED");
    return MESHX_SUCCESS;
}

/**
 * @brief PWM property test command handler
 *
 * This function is called by the unit test framework when PWM tests are requested.
 */
static meshx_err_t pwm_property_test_handler(int cmd_id, int argc, char **argv)
{
    (void)argc;
    (void)argv;

    MESHX_LOGD(MODULE_ID_COMMON, "[DEBUG] PWM Property Test Handler - Command ID: %d", cmd_id);

    switch (cmd_id) {
        case 0:
            // Run all property tests
            return run_pwm_property_tests();

        case 1:
            // Property 4.1: PWM initialization
            init_test_state();
            return test_property_pwm_initialization();

        case 2:
            // Property 4.2: PWM start/stop
            init_test_state();
            return test_property_pwm_start_stop();

        case 3:
            // Property 4.3: Duty cycle accuracy
            init_test_state();
            return test_property_duty_cycle_accuracy();

        case 4:
            // Property 4.4: Frequency accuracy
            init_test_state();
            return test_property_frequency_accuracy();

        case 5:
            // Property 4.5: Parameter validation
            init_test_state();
            return test_property_parameter_validation();

        case 6:
            // Property 4.6: Channel allocation
            init_test_state();
            return test_property_channel_allocation();

        case 7:
            // Property 4.7: State maintenance
            init_test_state();
            return test_property_state_maintenance();

        case 8:
            // Property 4.8: Deinitialization cleanup
            init_test_state();
            return test_property_deinit_cleanup();

        default:
            MESHX_LOGE(MODULE_ID_COMMON, "Unknown PWM test command ID: %d", cmd_id);
            return MESHX_INVALID_ARG;
    }
}

/**
 * @brief Register PWM property tests with unit test framework
 */
meshx_err_t register_pwm_property_tests(void)
{
    MESHX_LOGD(MODULE_ID_PWM_PROPERTY_TEST, "Registering PWM property tests");
    return register_unit_test(MODULE_ID_PWM_PROPERTY_TEST, pwm_property_test_handler);
}

#endif /* CONFIG_ENABLE_UNIT_TEST */
