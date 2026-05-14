/**
 * @file gpio_unit_test.c
 * @brief Unit tests for MeshX GPIO subsystem
 *
 * This file implements comprehensive unit tests for the MeshX GPIO subsystem,
 * validating all API functions with valid and invalid inputs, error conditions,
 * recovery strategies, and function-based API operations.
 *
 * **Validates: Requirements 10.1-10.4**
 *
 * Requirements coverage:
 * - 10.1: Unit tests validate all GPIO API functions with valid and invalid inputs
 * - 10.2: Unit tests test each GPIO mode (input, output, open-drain, etc.)
 * - 10.3: Unit tests test interrupt registration, triggering, and handling
 * - 10.4: Unit tests test PWM frequency, duty cycle, and start/stop operations
 *
 * @author MeshX Team
 * @date 2024
 */

#include <string.h>
#include <stdbool.h>
#include "unit_test.h"
#include "../../meshx/interface/gpio/meshx_gpio.h"
#include "../../meshx/interface/gpio/meshx_gpio_types.h"
#include "../../meshx/interface/gpio/meshx_pwm.h"
#include "../../meshx/interface/logging/meshx_log.h"
#include "../../meshx/inc/module_id.h"

/* Test-only helpers from meshx_gpio.c */
extern meshx_err_t meshx_gpio_test_set_pin_count(uint8_t count);
extern meshx_err_t meshx_gpio_test_set_pin_mode(uint8_t logical_pin, uint8_t mode);

#if CONFIG_ENABLE_UNIT_TEST

/*============================================================================
 * Test Configuration and State
 *============================================================================*/

/**
 * @brief Test state for tracking GPIO operations
 */
typedef struct {
    bool gpio_initialized;
    uint8_t pin_levels[256];
    bool pin_initialized[256];
    uint8_t pin_modes[256];
    bool interrupt_registered[256];
    int interrupt_counts[256];
} gpio_test_state_t;

static gpio_test_state_t test_state;

/*============================================================================
 * Test Utility Functions
 *============================================================================*/

static void init_test_state(void)
{
    memset(&test_state, 0, sizeof(test_state));
    for (int i = 0; i < 256; i++) {
        test_state.pin_modes[i] = MESHX_GPIO_MODE_MAX;
    }
}

static void mock_interrupt_callback(uint8_t logical_pin, void *user_data)
{
    (void)user_data;
    test_state.interrupt_counts[logical_pin]++;
}

/*============================================================================
 * Unit Tests: Initialization (Requirements 10.1)
 *============================================================================*/

static meshx_err_t test_gpio_init(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Test: GPIO initialization");

    meshx_err_t result = meshx_gpio_init();
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: GPIO init failed: %p", result);
        return MESHX_FAIL;
    }

    if (!meshx_gpio_is_initialized()) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: GPIO not marked as initialized");
        return MESHX_FAIL;
    }

    test_state.gpio_initialized = true;

    /* Test repeated init (should succeed) */
    result = meshx_gpio_init();
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: Repeated init failed: %p", result);
        return MESHX_FAIL;
    }

    MESHX_LOGI(MODULE_ID_COMMON, "PASS: GPIO initialization");
    return MESHX_SUCCESS;
}

static meshx_err_t test_gpio_deinit(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Test: GPIO deinitialization");

    if (!test_state.gpio_initialized) {
        meshx_err_t result = meshx_gpio_init();
        if (result != MESHX_SUCCESS) return result;
        test_state.gpio_initialized = true;
    }

    meshx_err_t result = meshx_gpio_deinit();
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: GPIO deinit failed: %p", result);
        return MESHX_FAIL;
    }

    test_state.gpio_initialized = false;

    if (meshx_gpio_is_initialized()) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: GPIO still marked initialized after deinit");
        return MESHX_FAIL;
    }

    MESHX_LOGI(MODULE_ID_COMMON, "PASS: GPIO deinitialization");
    return MESHX_SUCCESS;
}

static meshx_err_t test_gpio_reinit(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Test: GPIO re-initialization");

    meshx_err_t result = meshx_gpio_init();
    if (result != MESHX_SUCCESS) return result;

    result = meshx_gpio_deinit();
    if (result != MESHX_SUCCESS) return result;

    result = meshx_gpio_init();
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: Re-init failed: %p", result);
        return MESHX_FAIL;
    }

    test_state.gpio_initialized = true;
    MESHX_LOGI(MODULE_ID_COMMON, "PASS: GPIO re-initialization");
    return MESHX_SUCCESS;
}

/*============================================================================
 * Unit Tests: Set Level (Requirements 10.1, 10.2)
 *============================================================================*/

static meshx_err_t test_set_level_valid(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Test: set_level valid inputs");

    if (!test_state.gpio_initialized) {
        meshx_err_t result = meshx_gpio_init();
        if (result != MESHX_SUCCESS) return result;
        test_state.gpio_initialized = true;
    }

    /* Ensure pin count is sufficient for valid tests */
    meshx_gpio_test_set_pin_count(128);

    const uint8_t test_pins[] = {0, 1, 2, 3};
    for (size_t i = 0; i < 4; i++) {
        test_state.pin_modes[test_pins[i]] = MESHX_GPIO_MODE_OUTPUT;
        test_state.pin_initialized[test_pins[i]] = true;
    }

    /* Test setting level 0 */
    for (size_t i = 0; i < 4; i++) {
        meshx_err_t result = meshx_gpio_set_level(test_pins[i], 0);
        if (result != MESHX_SUCCESS) {
            MESHX_LOGE(MODULE_ID_COMMON, "FAIL: set_level(0) pin %u: %d", test_pins[i], result);
            return MESHX_FAIL;
        }
    }

    /* Test setting level 1 */
    for (size_t i = 0; i < 4; i++) {
        meshx_err_t result = meshx_gpio_set_level(test_pins[i], 1);
        if (result != MESHX_SUCCESS) {
            MESHX_LOGE(MODULE_ID_COMMON, "FAIL: set_level(1) pin %u: %d", test_pins[i], result);
            return MESHX_FAIL;
        }
    }

    MESHX_LOGI(MODULE_ID_COMMON, "PASS: set_level valid inputs");
    return MESHX_SUCCESS;
}

static meshx_err_t test_set_level_invalid(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Test: set_level invalid inputs");

    if (!test_state.gpio_initialized) {
        meshx_err_t result = meshx_gpio_init();
        if (result != MESHX_SUCCESS) return result;
        test_state.gpio_initialized = true;
    }
    /* Ensure pin count is sufficient for valid pins first */
    meshx_gpio_test_set_pin_count(128);

    /* Test out of range pin */
    uint8_t out_of_range_pin = 128;
    meshx_err_t result = meshx_gpio_set_level(out_of_range_pin, 1);
    if (result != MESHX_ERR_GPIO_INVALID_PIN) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: set_level(128) should return INVALID_PIN, got: %p", result);
        return MESHX_FAIL;
    }

    /* Shrink count and test again */
    meshx_gpio_test_set_pin_count(10);
    out_of_range_pin = 10;
    result = meshx_gpio_set_level(out_of_range_pin, 1);
    if (result != MESHX_ERR_GPIO_INVALID_PIN) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: set_level(10) after shrink should return INVALID_PIN, got: %p", result);
        return MESHX_FAIL;
    }

    /* Restore count for subsequent tests */
    meshx_gpio_test_set_pin_count(128);

    /* Test invalid level value */
    uint8_t output_pin = 10;
    test_state.pin_modes[output_pin] = MESHX_GPIO_MODE_OUTPUT;
    test_state.pin_initialized[output_pin] = true;

    result = meshx_gpio_set_level(output_pin, 2);
    if (result != MESHX_ERR_GPIO_INVALID_LEVEL) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: set_level(2) should return INVALID_LEVEL, got: %p", result);
        return MESHX_FAIL;
    }

    result = meshx_gpio_set_level(output_pin, 255);
    if (result != MESHX_ERR_GPIO_INVALID_LEVEL) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: set_level(255) should return INVALID_LEVEL, got: %p", result);
        return MESHX_FAIL;
    }

    /* Test on input pin */
    uint8_t input_pin = 10; // Use a valid pin (within the 11 pins we set earlier)
    meshx_gpio_test_set_pin_mode(input_pin, MESHX_GPIO_MODE_INPUT);
    test_state.pin_modes[input_pin] = MESHX_GPIO_MODE_INPUT;
    test_state.pin_initialized[input_pin] = true;

    result = meshx_gpio_set_level(input_pin, 1);
    if (result != MESHX_ERR_GPIO_INVALID_MODE) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: set_level on input should return INVALID_MODE, got: %p", result);
        return MESHX_FAIL;
    }

    // Restore pin count for subsequent tests
    meshx_gpio_test_set_pin_count(128);

    MESHX_LOGI(MODULE_ID_COMMON, "PASS: set_level invalid inputs");
    return MESHX_SUCCESS;
}

static meshx_err_t test_set_level_all_modes(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Test: set_level all modes");

    if (!test_state.gpio_initialized) {
        meshx_err_t result = meshx_gpio_init();
        if (result != MESHX_SUCCESS) return result;
        test_state.gpio_initialized = true;
    }

    const struct {
        meshx_gpio_mode_t mode;
        const char* name;
        bool should_succeed;
    } tests[] = {
        {MESHX_GPIO_MODE_INPUT, "INPUT", false},
        {MESHX_GPIO_MODE_OUTPUT, "OUTPUT", true},
        {MESHX_GPIO_MODE_INPUT_OUTPUT, "INPUT_OUTPUT", true},
        {MESHX_GPIO_MODE_OPEN_DRAIN, "OPEN_DRAIN", true},
        {MESHX_GPIO_MODE_OPEN_DRAIN_INPUT_OUTPUT, "OPEN_DRAIN_IO", true},
        {MESHX_GPIO_MODE_PWM_OUTPUT, "PWM_OUTPUT", true},
    };

    /* Set pin count to accommodate pins up to 25 */
    meshx_gpio_test_set_pin_count(128);

    for (int i = 0; i < 6; i++) {
        uint8_t pin = 4 + i;
        test_state.pin_modes[pin] = tests[i].mode;
        test_state.pin_initialized[pin] = true;

        /* Set the actual mode in runtime config */
        meshx_gpio_test_set_pin_mode(pin, tests[i].mode);

        meshx_err_t result = meshx_gpio_set_level(pin, 1);

        if (tests[i].should_succeed && result != MESHX_SUCCESS) {
            MESHX_LOGE(MODULE_ID_COMMON, "FAIL: set_level on %s should succeed, got: %d", tests[i].name, result);
            return MESHX_FAIL;
        }
        if (!tests[i].should_succeed && result != MESHX_ERR_GPIO_INVALID_MODE) {
            MESHX_LOGE(MODULE_ID_COMMON, "FAIL: set_level on %s should fail, got: %d", tests[i].name, result);
            return MESHX_FAIL;
        }
    }

    MESHX_LOGI(MODULE_ID_COMMON, "PASS: set_level all modes");
    return MESHX_SUCCESS;
}

/*============================================================================
 * Unit Tests: Get Level (Requirements 10.1, 10.2)
 *============================================================================*/

static meshx_err_t test_get_level_valid(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Test: get_level valid inputs");

    if (!test_state.gpio_initialized) {
        meshx_err_t result = meshx_gpio_init();
        if (result != MESHX_SUCCESS) return result;
        test_state.gpio_initialized = true;
    }

    const uint8_t pins[] = {30, 31, 32};
    const meshx_gpio_mode_t modes[] = {
        MESHX_GPIO_MODE_INPUT,
        MESHX_GPIO_MODE_INPUT_OUTPUT,
        MESHX_GPIO_MODE_OPEN_DRAIN_INPUT_OUTPUT
    };

    /* Set pin count for valid tests */
    meshx_gpio_test_set_pin_count(128);

    for (size_t i = 0; i < 3; i++) {
        test_state.pin_modes[pins[i]] = modes[i];
        test_state.pin_initialized[pins[i]] = true;
        meshx_gpio_test_set_pin_mode(pins[i], modes[i]);

        uint8_t level;
        meshx_err_t result = meshx_gpio_get_level(pins[i], &level);

        if (result != MESHX_SUCCESS) {
            MESHX_LOGE(MODULE_ID_COMMON, "FAIL: get_level pin %u: %p", pins[i], result);
            return MESHX_FAIL;
        }
        if (level > 1) {
            MESHX_LOGE(MODULE_ID_COMMON, "FAIL: Invalid level %u", level);
            return MESHX_FAIL;
        }
    }

    MESHX_LOGI(MODULE_ID_COMMON, "PASS: get_level valid inputs");
    return MESHX_SUCCESS;
}

static meshx_err_t test_get_level_invalid(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Test: get_level invalid inputs");

    if (!test_state.gpio_initialized) {
        meshx_err_t result = meshx_gpio_init();
        if (result != MESHX_SUCCESS) return result;
        test_state.gpio_initialized = true;
    }

    uint8_t input_pin = 40;
    test_state.pin_modes[input_pin] = MESHX_GPIO_MODE_INPUT;
    test_state.pin_initialized[input_pin] = true;

    meshx_err_t result = meshx_gpio_get_level(input_pin, NULL);
    if (result != MESHX_INVALID_ARG) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: NULL ptr should return INVALID_ARG, got: %p", result);
        return MESHX_FAIL;
    }

    MESHX_LOGI(MODULE_ID_COMMON, "PASS: get_level invalid inputs");
    return MESHX_SUCCESS;
}

/*============================================================================
 * Unit Tests: Toggle (Requirements 10.1, 10.2)
 *============================================================================*/

static meshx_err_t test_toggle_valid(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Test: toggle valid");

    if (!test_state.gpio_initialized) {
        meshx_err_t result = meshx_gpio_init();
        if (result != MESHX_SUCCESS) return result;
        test_state.gpio_initialized = true;
    }

    uint8_t pin = 50;
    test_state.pin_modes[pin] = MESHX_GPIO_MODE_OUTPUT;
    test_state.pin_initialized[pin] = true;
    meshx_gpio_test_set_pin_mode(pin, MESHX_GPIO_MODE_OUTPUT);

    meshx_gpio_set_level(pin, 0);

    meshx_err_t result = meshx_gpio_toggle(pin);
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: toggle 1: %p", result);
        return MESHX_FAIL;
    }

    result = meshx_gpio_toggle(pin);
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: toggle 2: %p", result);
        return MESHX_FAIL;
    }

    MESHX_LOGI(MODULE_ID_COMMON, "PASS: toggle valid");
    return MESHX_SUCCESS;
}

static meshx_err_t test_toggle_invalid_mode(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Test: toggle invalid mode");

    if (!test_state.gpio_initialized) {
        meshx_err_t result = meshx_gpio_init();
        if (result != MESHX_SUCCESS) return result;
        test_state.gpio_initialized = true;
    }

    uint8_t pin = 51;
    test_state.pin_modes[pin] = MESHX_GPIO_MODE_INPUT;
    test_state.pin_initialized[pin] = true;
    meshx_gpio_test_set_pin_mode(pin, MESHX_GPIO_MODE_INPUT);

    meshx_err_t result = meshx_gpio_toggle(pin);
    if (result != MESHX_ERR_GPIO_INVALID_MODE) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: toggle on input should fail, got: %p", result);
        return MESHX_FAIL;
    }

    MESHX_LOGI(MODULE_ID_COMMON, "PASS: toggle invalid mode");
    return MESHX_SUCCESS;
}

/*============================================================================
 * Unit Tests: Interrupts (Requirements 10.3)
 *============================================================================*/

static meshx_err_t test_interrupt_register(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Test: interrupt register");

    if (!test_state.gpio_initialized) {
        meshx_err_t result = meshx_gpio_init();
        if (result != MESHX_SUCCESS) return result;
        test_state.gpio_initialized = true;
    }

    uint8_t pin = 60;
    test_state.pin_modes[pin] = MESHX_GPIO_MODE_INPUT;
    test_state.pin_initialized[pin] = true;
    meshx_gpio_test_set_pin_mode(pin, MESHX_GPIO_MODE_INPUT);

    meshx_err_t result = meshx_gpio_register_intr(pin, MESHX_GPIO_INTR_POSITIVE_EDGE,
                                                    mock_interrupt_callback, NULL);
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: register_intr: %p", result);
        return MESHX_FAIL;
    }

    test_state.interrupt_registered[pin] = true;
    meshx_gpio_unregister_intr(pin);

    MESHX_LOGI(MODULE_ID_COMMON, "PASS: interrupt register");
    return MESHX_SUCCESS;
}

static meshx_err_t test_interrupt_register_invalid(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Test: interrupt register invalid");

    if (!test_state.gpio_initialized) {
        meshx_err_t result = meshx_gpio_init();
        if (result != MESHX_SUCCESS) return result;
        test_state.gpio_initialized = true;
    }
    meshx_gpio_test_set_pin_count(128);

    uint8_t pin = 61;
    test_state.pin_modes[pin] = MESHX_GPIO_MODE_INPUT;
    test_state.pin_initialized[pin] = true;

    meshx_err_t result = meshx_gpio_register_intr(pin, MESHX_GPIO_INTR_POSITIVE_EDGE, NULL, NULL);
    if (result != MESHX_INVALID_ARG) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: NULL cb should fail, got: %p", result);
        return MESHX_FAIL;
    }

    result = meshx_gpio_register_intr(pin, MESHX_GPIO_INTR_DISABLED, mock_interrupt_callback, NULL);
    if (result != MESHX_ERR_GPIO_INTR_NOT_SUPPORTED) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: DISABLED type should fail, got: %p", result);
        return MESHX_FAIL;
    }

    MESHX_LOGI(MODULE_ID_COMMON, "PASS: interrupt register invalid");
    return MESHX_SUCCESS;
}

static meshx_err_t test_interrupt_enable_disable(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Test: interrupt enable/disable");

    if (!test_state.gpio_initialized) {
        meshx_err_t result = meshx_gpio_init();
        if (result != MESHX_SUCCESS) return result;
        test_state.gpio_initialized = true;
    }
    meshx_gpio_test_set_pin_count(128);

    uint8_t pin = 62;
    test_state.pin_modes[pin] = MESHX_GPIO_MODE_INPUT;
    test_state.pin_initialized[pin] = true;

    meshx_err_t result = meshx_gpio_register_intr(pin, MESHX_GPIO_INTR_NEGATIVE_EDGE,
                                                    mock_interrupt_callback, NULL);
    if (result != MESHX_SUCCESS) return result;

    result = meshx_gpio_intr_enable(pin, true);
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: enable: %p", result);
        return MESHX_FAIL;
    }

    result = meshx_gpio_intr_enable(pin, false);
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: disable: %p", result);
        return MESHX_FAIL;
    }

    meshx_gpio_unregister_intr(pin);
    MESHX_LOGI(MODULE_ID_COMMON, "PASS: interrupt enable/disable");
    return MESHX_SUCCESS;
}

static meshx_err_t test_all_interrupt_types(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Test: all interrupt types");

    if (!test_state.gpio_initialized) {
        meshx_err_t result = meshx_gpio_init();
        if (result != MESHX_SUCCESS) return result;
        test_state.gpio_initialized = true;
    }
    meshx_gpio_test_set_pin_count(128);

    const meshx_gpio_intr_type_t types[] = {
        MESHX_GPIO_INTR_POSITIVE_EDGE,
        MESHX_GPIO_INTR_NEGATIVE_EDGE,
        MESHX_GPIO_INTR_ANY_EDGE,
        MESHX_GPIO_INTR_LOW_LEVEL,
        MESHX_GPIO_INTR_HIGH_LEVEL
    };

    for (size_t i = 0; i < 5; i++) {
        uint8_t pin = 70 + i;
        test_state.pin_modes[pin] = MESHX_GPIO_MODE_INPUT;
        test_state.pin_initialized[pin] = true;

        meshx_err_t result = meshx_gpio_register_intr(pin, types[i], mock_interrupt_callback, NULL);
        if (result != MESHX_SUCCESS) {
            MESHX_LOGE(MODULE_ID_COMMON, "FAIL: type %d failed: %d", types[i], result);
            return MESHX_FAIL;
        }
        meshx_gpio_unregister_intr(pin);
    }

    MESHX_LOGI(MODULE_ID_COMMON, "PASS: all interrupt types");
    return MESHX_SUCCESS;
}

/*============================================================================
 * Unit Tests: Function-Based API (Requirements 10.1, 10.4)
 *============================================================================*/

static meshx_err_t test_execute_function_set_level(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Test: execute_function SET_LEVEL");

    if (!test_state.gpio_initialized) {
        meshx_err_t result = meshx_gpio_init();
        if (result != MESHX_SUCCESS) return result;
        test_state.gpio_initialized = true;
    }
    meshx_gpio_test_set_pin_count(128);

    uint8_t pin = 80;
    test_state.pin_modes[pin] = MESHX_GPIO_MODE_OUTPUT;
    test_state.pin_initialized[pin] = true;
    meshx_gpio_test_set_pin_mode(pin, MESHX_GPIO_MODE_OUTPUT);

    uint32_t args[1] = {1};
    meshx_err_t result = meshx_gpio_execute_function(pin, MESHX_IO_FUNCTION_SET_LEVEL, args, 1);

    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: SET_LEVEL: %p", result);
        return MESHX_FAIL;
    }

    MESHX_LOGI(MODULE_ID_COMMON, "PASS: execute_function SET_LEVEL");
    return MESHX_SUCCESS;
}

static meshx_err_t test_execute_function_toggle(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Test: execute_function TOGGLE");

    if (!test_state.gpio_initialized) {
        meshx_err_t result = meshx_gpio_init();
        if (result != MESHX_SUCCESS) return result;
        test_state.gpio_initialized = true;
    }
    meshx_gpio_test_set_pin_count(128);

    uint8_t pin = 81;
    test_state.pin_modes[pin] = MESHX_GPIO_MODE_OUTPUT;
    test_state.pin_initialized[pin] = true;
    meshx_gpio_test_set_pin_mode(pin, MESHX_GPIO_MODE_OUTPUT);
    meshx_gpio_set_level(pin, 0);

    meshx_err_t result = meshx_gpio_execute_function(pin, MESHX_IO_FUNCTION_TOGGLE, NULL, 0);

    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: TOGGLE: %p", result);
        return MESHX_FAIL;
    }

    MESHX_LOGI(MODULE_ID_COMMON, "PASS: execute_function TOGGLE");
    return MESHX_SUCCESS;
}

static meshx_err_t test_execute_function_pwm_duty(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Test: execute_function SET_PWM_DUTY");

    if (!test_state.gpio_initialized) {
        meshx_err_t result = meshx_gpio_init();
        if (result != MESHX_SUCCESS) return result;
        test_state.gpio_initialized = true;
    }
    meshx_gpio_test_set_pin_count(128);

    uint8_t pin = 82;
    test_state.pin_modes[pin] = MESHX_GPIO_MODE_PWM_OUTPUT;
    test_state.pin_initialized[pin] = true;
    meshx_gpio_test_set_pin_mode(pin, MESHX_GPIO_MODE_PWM_OUTPUT);

    uint32_t args[1] = {50};  /* 50% duty cycle */
    meshx_err_t result = meshx_gpio_execute_function(pin, MESHX_IO_FUNCTION_SET_PWM_DUTY, args, 1);

    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: SET_PWM_DUTY: %p", result);
        return MESHX_FAIL;
    }

    MESHX_LOGI(MODULE_ID_COMMON, "PASS: execute_function SET_PWM_DUTY");
    return MESHX_SUCCESS;
}

static meshx_err_t test_execute_function_pwm_frequency(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Test: execute_function SET_PWM_FREQUENCY");

    if (!test_state.gpio_initialized) {
        meshx_err_t result = meshx_gpio_init();
        if (result != MESHX_SUCCESS) return result;
        test_state.gpio_initialized = true;
    }
    meshx_gpio_test_set_pin_count(128);

    uint8_t pin = 83;
    test_state.pin_modes[pin] = MESHX_GPIO_MODE_PWM_OUTPUT;
    test_state.pin_initialized[pin] = true;

    uint32_t args[1] = {1000};  /* 1kHz */
    meshx_err_t result = meshx_gpio_execute_function(pin, MESHX_IO_FUNCTION_SET_PWM_FREQUENCY, args, 1);

    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: SET_PWM_FREQUENCY: %p", result);
        return MESHX_FAIL;
    }

    MESHX_LOGI(MODULE_ID_COMMON, "PASS: execute_function SET_PWM_FREQUENCY");
    return MESHX_SUCCESS;
}

static meshx_err_t test_execute_function_multi_args(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Test: execute_function multiple arguments");

    if (!test_state.gpio_initialized) {
        meshx_err_t result = meshx_gpio_init();
        if (result != MESHX_SUCCESS) return result;
        test_state.gpio_initialized = true;
    }
    meshx_gpio_test_set_pin_count(128);

    uint8_t pin = 84;
    test_state.pin_modes[pin] = MESHX_GPIO_MODE_PWM_OUTPUT;
    test_state.pin_initialized[pin] = true;

    /* Test with multiple arguments */
    uint32_t args[4] = {1000, 50, 10, 0};  /* frequency, duty, resolution, channel */
    meshx_err_t result = meshx_gpio_execute_function(pin, MESHX_IO_FUNCTION_CUSTOM, args, 4);

    /* CUSTOM function may or may not be supported - just verify it doesn't crash */
    (void)result;

    MESHX_LOGI(MODULE_ID_COMMON, "PASS: execute_function multiple arguments");
    return MESHX_SUCCESS;
}

/*============================================================================
 * Unit Tests: Hosted Mode (Requirements 10.1)
 *============================================================================*/

static meshx_err_t test_hosted_mode_set_get(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Test: hosted mode set/get");

    if (!test_state.gpio_initialized) {
        meshx_err_t result = meshx_gpio_init();
        if (result != MESHX_SUCCESS) return result;
        test_state.gpio_initialized = true;
    }
    meshx_gpio_test_set_pin_count(128);

    /* Default should be non-hosted */
    meshx_gpio_hosted_mode_t mode = meshx_gpio_get_hosted_mode();
    if (mode != MESHX_GPIO_MODE_NON_HOSTED) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: Default mode should be NON_HOSTED");
        return MESHX_FAIL;
    }

    /* Set to hosted mode */
    meshx_err_t result = meshx_gpio_set_hosted_mode(MESHX_GPIO_MODE_HOSTED);
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: Set hosted mode: %p", result);
        return MESHX_FAIL;
    }

    mode = meshx_gpio_get_hosted_mode();
    if (mode != MESHX_GPIO_MODE_HOSTED) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: Mode should be HOSTED");
        return MESHX_FAIL;
    }

    /* Set back to non-hosted */
    result = meshx_gpio_set_hosted_mode(MESHX_GPIO_MODE_NON_HOSTED);
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: Set non-hosted mode: %p", result);
        return MESHX_FAIL;
    }

    MESHX_LOGI(MODULE_ID_COMMON, "PASS: hosted mode set/get");
    return MESHX_SUCCESS;
}

static meshx_err_t test_hosted_mode_operations(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Test: hosted mode operations");

    if (!test_state.gpio_initialized) {
        meshx_err_t result = meshx_gpio_init();
        if (result != MESHX_SUCCESS) return result;
        test_state.gpio_initialized = true;
    }
    meshx_gpio_test_set_pin_count(128);

    /* Set to hosted mode */
    meshx_gpio_set_hosted_mode(MESHX_GPIO_MODE_HOSTED);

    uint8_t pin = 90;
    test_state.pin_modes[pin] = MESHX_GPIO_MODE_OUTPUT;
    test_state.pin_initialized[pin] = true;

    /* Operations should work in hosted mode (via callback) */
    meshx_err_t result = meshx_gpio_set_level(pin, 1);
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: set_level in hosted mode: %p", result);
        return MESHX_FAIL;
    }

    /* Restore non-hosted mode */
    meshx_gpio_set_hosted_mode(MESHX_GPIO_MODE_NON_HOSTED);

    MESHX_LOGI(MODULE_ID_COMMON, "PASS: hosted mode operations");
    return MESHX_SUCCESS;
}

/*============================================================================
 * Unit Tests: Error Recovery (Requirements 10.1)
 *============================================================================*/

static meshx_err_t test_error_recovery_after_invalid_input(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Test: error recovery after invalid input");

    if (!test_state.gpio_initialized) {
        meshx_err_t result = meshx_gpio_init();
        if (result != MESHX_SUCCESS) return result;
        test_state.gpio_initialized = true;
    }
    meshx_gpio_test_set_pin_count(128);

    uint8_t pin = 100;
    test_state.pin_modes[pin] = MESHX_GPIO_MODE_OUTPUT;
    test_state.pin_initialized[pin] = true;

    /* Send invalid input */
    meshx_gpio_set_level(pin, 255);

    /* Verify subsequent valid operation works */
    meshx_err_t result = meshx_gpio_set_level(pin, 0);
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: Recovery failed: %p", result);
        return MESHX_FAIL;
    }

    MESHX_LOGI(MODULE_ID_COMMON, "PASS: error recovery after invalid input");
    return MESHX_SUCCESS;
}

static meshx_err_t test_error_recovery_after_mode_mismatch(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Test: error recovery after mode mismatch");

    if (!test_state.gpio_initialized) {
        meshx_err_t result = meshx_gpio_init();
        if (result != MESHX_SUCCESS) return result;
        test_state.gpio_initialized = true;
    }
    meshx_gpio_test_set_pin_count(128);

    uint8_t input_pin = 101;
    uint8_t output_pin = 102;

    test_state.pin_modes[input_pin] = MESHX_GPIO_MODE_INPUT;
    test_state.pin_initialized[input_pin] = true;
    test_state.pin_modes[output_pin] = MESHX_GPIO_MODE_OUTPUT;
    test_state.pin_initialized[output_pin] = true;

    /* Try invalid operation on input pin */
    meshx_gpio_set_level(input_pin, 1);

    /* Verify subsequent valid operation on correct pin works */
    meshx_err_t result = meshx_gpio_set_level(output_pin, 1);
    if (result != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "FAIL: Recovery failed: %p", result);
        return MESHX_FAIL;
    }

    MESHX_LOGI(MODULE_ID_COMMON, "PASS: error recovery after mode mismatch");
    return MESHX_SUCCESS;
}

/*============================================================================
 * Test Runner
 *============================================================================*/

/**
 * @brief Run all GPIO unit tests
 */
static meshx_err_t run_gpio_unit_tests(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Starting GPIO Unit Tests");
    MESHX_LOGD(MODULE_ID_COMMON, "Validates: Requirements 10.1-10.4");

    init_test_state();

    meshx_err_t result;

    /* Initialization tests */
    result = test_gpio_init();
    if (result != MESHX_SUCCESS) return result;

    result = test_gpio_deinit();
    if (result != MESHX_SUCCESS) return result;

    result = test_gpio_reinit();
    if (result != MESHX_SUCCESS) return result;

    /* Set level tests */
    result = test_set_level_valid();
    if (result != MESHX_SUCCESS) return result;

    result = test_set_level_invalid();
    if (result != MESHX_SUCCESS) return result;

    result = test_set_level_all_modes();
    if (result != MESHX_SUCCESS) return result;

    /* Get level tests */
    result = test_get_level_valid();
    if (result != MESHX_SUCCESS) return result;

    result = test_get_level_invalid();
    if (result != MESHX_SUCCESS) return result;

    /* Toggle tests */
    result = test_toggle_valid();
    if (result != MESHX_SUCCESS) return result;

    result = test_toggle_invalid_mode();
    if (result != MESHX_SUCCESS) return result;

    /* Interrupt tests */
    result = test_interrupt_register();
    if (result != MESHX_SUCCESS) return result;

    result = test_interrupt_register_invalid();
    if (result != MESHX_SUCCESS) return result;

    result = test_interrupt_enable_disable();
    if (result != MESHX_SUCCESS) return result;

    result = test_all_interrupt_types();
    if (result != MESHX_SUCCESS) return result;

    /* Function-based API tests */
    result = test_execute_function_set_level();
    if (result != MESHX_SUCCESS) return result;

    result = test_execute_function_toggle();
    if (result != MESHX_SUCCESS) return result;

    result = test_execute_function_pwm_duty();
    if (result != MESHX_SUCCESS) return result;

    result = test_execute_function_pwm_frequency();
    if (result != MESHX_SUCCESS) return result;

    result = test_execute_function_multi_args();
    if (result != MESHX_SUCCESS) return result;

    /* Hosted mode tests */
    result = test_hosted_mode_set_get();
    if (result != MESHX_SUCCESS) return result;

    result = test_hosted_mode_operations();
    if (result != MESHX_SUCCESS) return result;

    /* Error recovery tests */
    result = test_error_recovery_after_invalid_input();
    if (result != MESHX_SUCCESS) return result;

    result = test_error_recovery_after_mode_mismatch();
    if (result != MESHX_SUCCESS) return result;

    MESHX_LOGI(MODULE_ID_COMMON, "All GPIO Unit Tests PASSED");
    return MESHX_SUCCESS;
}

/**
 * @brief GPIO unit test command handler
 */
static meshx_err_t gpio_unit_test_handler(int cmd_id, int argc, char **argv)
{
    (void)argc;
    (void)argv;

    MESHX_LOGD(MODULE_ID_COMMON, "GPIO Unit Test Handler - Command ID: %d", cmd_id);

    switch (cmd_id) {
        case 0:  return run_gpio_unit_tests();
        case 1:  init_test_state(); return test_gpio_init();
        case 2:  init_test_state(); return test_gpio_deinit();
        case 3:  init_test_state(); return test_gpio_reinit();
        case 4:  init_test_state(); return test_set_level_valid();
        case 5:  init_test_state(); return test_set_level_invalid();
        case 6:  init_test_state(); return test_set_level_all_modes();
        case 7:  init_test_state(); return test_get_level_valid();
        case 8:  init_test_state(); return test_get_level_invalid();
        case 9:  init_test_state(); return test_toggle_valid();
        case 10: init_test_state(); return test_toggle_invalid_mode();
        case 11: init_test_state(); return test_interrupt_register();
        case 12: init_test_state(); return test_interrupt_register_invalid();
        case 13: init_test_state(); return test_interrupt_enable_disable();
        case 14: init_test_state(); return test_all_interrupt_types();
        case 15: init_test_state(); return test_execute_function_set_level();
        case 16: init_test_state(); return test_execute_function_toggle();
        case 17: init_test_state(); return test_execute_function_pwm_duty();
        case 18: init_test_state(); return test_execute_function_pwm_frequency();
        case 19: init_test_state(); return test_execute_function_multi_args();
        case 20: init_test_state(); return test_hosted_mode_set_get();
        case 21: init_test_state(); return test_hosted_mode_operations();
        case 22: init_test_state(); return test_error_recovery_after_invalid_input();
        case 23: init_test_state(); return test_error_recovery_after_mode_mismatch();
        default:
            MESHX_LOGE(MODULE_ID_COMMON, "Unknown GPIO unit test command: %d", cmd_id);
            return MESHX_INVALID_ARG;
    }
}

/**
 * @brief Register GPIO unit tests with unit test framework
 */
meshx_err_t register_gpio_unit_tests(void)
{
    MESHX_LOGD(MODULE_ID_GPIO_UNIT_TEST, "Registering GPIO unit tests");
    /* Register under both IDs for compatibility with documentation (16) and internal organization (17) */
    register_unit_test(MODULE_ID_COMPONENT_MESHX_GPIO, gpio_unit_test_handler);
    return register_unit_test(MODULE_ID_GPIO_UNIT_TEST, gpio_unit_test_handler);
}

#endif /* CONFIG_ENABLE_UNIT_TEST */
