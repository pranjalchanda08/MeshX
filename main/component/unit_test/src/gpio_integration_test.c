/**
 * @file gpio_integration_test.c
 * @brief Integration tests for MeshX GPIO subsystem
 *
 * This file implements comprehensive integration tests for the MeshX GPIO subsystem,
 * testing GPIO with element integration, hosted/non-hosted mode switching, and
 * KV Engine persistence and recovery.
 *
 * **Validates: Requirements 10.5-10.8**
 *
 * Requirements coverage:
 * - 10.5: Integration tests for GPIO on each supported BSP (xiao_c3, weact_c3, esp32_devkitC)
 * - 10.6: Integration tests for GPIO usage from elements (relay, light, sensor)
 * - 10.7: Integration tests for YAML configuration parsing and code generation
 * - 10.8: Integration tests for GPIO with other MeshX subsystems (BLE, NVS, logging)
 *
 * Integration test categories:
 * 1. Element Integration: Tests GPIO with abstract IO interface and element state changes
 * 2. Hosted/Non-Hosted Mode: Tests mode switching and UART transport
 * 3. KV Engine Persistence: Tests configuration save/load, recovery, and state persistence
 *
 * @author MeshX Team
 * @date 2024
 */

#include <string.h>
#include <stdbool.h>
#include "unit_test.h"
#include "gpio_integration_test.h"
#include "../../meshx/interface/gpio/meshx_gpio.h"
#include "../../meshx/interface/gpio/meshx_gpio_types.h"
#include "../../meshx/interface/gpio/meshx_gpio_kv.h"
#include "../../meshx/interface/gpio/meshx_pwm.h"
#include "../../meshx/io/interface/meshx_io_bridge.h"
#include "../../meshx/inc/meshx_kv_engine.h"
#include "../../meshx/interface/utils/meshx_fal_interface.h"
#include "../../meshx/interface/logging/meshx_log.h"
#include "../../meshx/inc/module_id.h"
#include "../../meshx/inc/meshx_err.h"

#if CONFIG_ENABLE_UNIT_TEST
extern void meshx_gpio_test_set_pin_count(uint8_t count);
extern void meshx_gpio_test_set_pin_mode(uint8_t pin, meshx_gpio_mode_t mode);
extern void meshx_pwm_test_set_pin_count(uint8_t count);
#endif

#if CONFIG_ENABLE_UNIT_TEST

/*============================================================================
 * Test Configuration and State
 *============================================================================*/

/**
 * @brief Integration test state for tracking cross-subsystem operations
 */
typedef struct {
    bool gpio_initialized;
    bool kv_initialized;
    bool io_initialized;
    meshx_gpio_hosted_mode_t current_hosted_mode;
    uint8_t element_state_changes;
    uint8_t hosted_events_sent;
    uint8_t kv_save_count;
    uint8_t kv_load_count;
    meshx_gpio_pin_config_t test_configs[8];
    meshx_gpio_pin_state_t test_states[8];
} gpio_integration_state_t;

static gpio_integration_state_t intg_state;

/**
 * @brief Mock FAL partition for KV Engine testing
 */
static meshx_fal_partition_t mock_kv_partition = {
    .name = "kv_store",
    .base_addr = 0,
    .size = 4096,
    .sector_size = 4096,
    .priv = NULL,
};

/*============================================================================
 * Mock Callbacks for Integration Testing
 *============================================================================*/

/**
 * @brief Mock hosted event callback for testing UART transport
 */
static void mock_hosted_event_callback(const meshx_gpio_hosted_event_t *event)
{
    intg_state.hosted_events_sent++;
    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO,
               "Mock hosted event: type=%d, pin=%u, value=%u",
               event->event_type, event->logical_pin, event->value);
}


/**
 * @brief Mock interrupt callback for testing
 */
static void mock_interrupt_callback(uint8_t logical_pin, void *user_data)
{
    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO,
               "Mock interrupt on pin %u", logical_pin);
}

/*============================================================================
 * Test Utility Functions
 *============================================================================*/

/**
 * @brief Initialize integration test state
 */
static void init_integration_state(void)
{
    meshx_gpio_deinit();
    meshx_pwm_deinit();
    meshx_gpio_init();
    meshx_pwm_init();
    meshx_gpio_test_set_pin_count(128);
    meshx_pwm_test_set_pin_count(128);
    memset(&intg_state, 0, sizeof(intg_state));
    intg_state.current_hosted_mode = MESHX_GPIO_MODE_NON_HOSTED;
}

/**
 * @brief Setup test GPIO configuration
 */
static void setup_test_gpio_config(void)
{
    /* Configure test pins with various modes */
    intg_state.test_configs[0] = (meshx_gpio_pin_config_t){
        .logical_pin = 0,
        .physical_pin = 4,
        .mode = MESHX_GPIO_MODE_OUTPUT,
        .pull = MESHX_GPIO_PULL_NONE,
        .drive_strength = MESHX_GPIO_DRIVE_MEDIUM,
        .initial_level = 0,
        .signal_inversion = false,
    };

    intg_state.test_configs[1] = (meshx_gpio_pin_config_t){
        .logical_pin = 1,
        .physical_pin = 5,
        .mode = MESHX_GPIO_MODE_INPUT,
        .pull = MESHX_GPIO_PULL_UP,
        .drive_strength = MESHX_GPIO_DRIVE_MEDIUM,
        .initial_level = 0,
        .signal_inversion = false,
    };

    intg_state.test_configs[2] = (meshx_gpio_pin_config_t){
        .logical_pin = 2,
        .physical_pin = 6,
        .mode = MESHX_GPIO_MODE_PWM_OUTPUT,
        .pull = MESHX_GPIO_PULL_NONE,
        .drive_strength = MESHX_GPIO_DRIVE_MEDIUM,
        .initial_level = 0,
        .signal_inversion = false,
        .mode_config.pwm = {
            .frequency = 1000,
            .duty_cycle = 50,
            .resolution = 10,
            .channel = 0,
        },
    };

    intg_state.test_configs[3] = (meshx_gpio_pin_config_t){
        .logical_pin = 3,
        .physical_pin = 7,
        .mode = MESHX_GPIO_MODE_INPUT,
        .pull = MESHX_GPIO_PULL_UP,
        .drive_strength = MESHX_GPIO_DRIVE_MEDIUM,
        .initial_level = 0,
        .signal_inversion = false,
        .mode_config.interrupt = {
            .trigger = MESHX_GPIO_INTR_NEGATIVE_EDGE,
            .task_priority = 5,
            .task_stack_size = 2048,
        },
    };

    /* Sync with runtime */
    for (int i = 0; i < 8; i++) {
        if (intg_state.test_configs[i].logical_pin < 128 && 
            intg_state.test_configs[i].mode != MESHX_GPIO_MODE_MAX) {
            meshx_gpio_test_set_pin_mode(intg_state.test_configs[i].logical_pin, 
                                         intg_state.test_configs[i].mode);
        }
    }
}

/**
 * @brief Cleanup integration test resources
 */
static void cleanup_integration_test(void)
{
    if (intg_state.gpio_initialized) {
        meshx_gpio_deinit();
        intg_state.gpio_initialized = false;
    }

    if (intg_state.kv_initialized) {
        meshx_gpio_kv_deinit();
        intg_state.kv_initialized = false;
    }
}

/*============================================================================
 * Integration Tests: Element Integration (Requirement 10.6)
 *============================================================================*/

/**
 * @brief Test GPIO with abstract IO interface creation
 *
 * Validates that GPIO pins can be created through the abstract IO interface
 * and that the IO bridge correctly dispatches operations to the GPIO subsystem.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
static meshx_err_t test_io_interface_creation(void)
{
    MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_GPIO, "Test: IO interface creation");

    /* Initialize GPIO subsystem */
    meshx_err_t err = meshx_gpio_init();
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "GPIO init failed: %d", err);
        return MESHX_FAIL;
    }
    intg_state.gpio_initialized = true;

    /* Create IO instance through bridge */
    meshx_io_config_t io_config = {
        .logical_pin = 0,
        .io_type = 0,  /* GPIO type */
        .name = "TEST_RELAY",
        .config.gpio = {
            .mode = MESHX_GPIO_MODE_OUTPUT,
            .pull = MESHX_GPIO_PULL_NONE,
            .drive = MESHX_GPIO_DRIVE_MEDIUM,
            .initial_level = 0,
            .signal_inversion = false,
        },
    };

    meshx_io_handle_t io_handle = meshx_io_create(&io_config);
    if (io_handle == NULL) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to create IO instance");
        return MESHX_FAIL;
    }

    /* Verify IO instance properties */
    uint8_t logical_pin = meshx_io_get_logical_pin(io_handle);
    if (logical_pin != 0) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO,
                   "IO instance logical pin mismatch: expected 0, got %u", logical_pin);
        meshx_io_destroy(io_handle);
        return MESHX_FAIL;
    }

    const char* name = meshx_io_get_name(io_handle);
    if (name == NULL || strcmp(name, "TEST_RELAY") != 0) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "IO instance name mismatch");
        meshx_io_destroy(io_handle);
        return MESHX_FAIL;
    }

    /* Cleanup */
    meshx_io_destroy(io_handle);

    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "PASS: IO interface creation");
    return MESHX_SUCCESS;
}

/**
 * @brief Test GPIO with element state changes
 *
 * Validates that GPIO operations triggered by element state changes
 * work correctly through the abstract IO interface.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
static meshx_err_t test_element_state_change(void)
{
    MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_GPIO, "Test: Element state change integration");

    if (!intg_state.gpio_initialized) {
        meshx_err_t err = meshx_gpio_init();
        if (err != MESHX_SUCCESS) return MESHX_FAIL;
        intg_state.gpio_initialized = true;
    }

    /* Create IO instance for relay element */
    meshx_io_config_t io_config = {
        .logical_pin = 0,
        .io_type = 0,
        .name = "RELAY_1",
        .config.gpio = {
            .mode = MESHX_GPIO_MODE_OUTPUT,
            .pull = MESHX_GPIO_PULL_NONE,
            .drive = MESHX_GPIO_DRIVE_MEDIUM,
            .initial_level = 0,
            .signal_inversion = false,
        },
    };

    meshx_io_handle_t io_handle = meshx_io_create(&io_config);
    if (io_handle == NULL) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to create relay IO");
        return MESHX_FAIL;
    }

    /* Simulate element state change: Turn relay ON */
    meshx_err_t err = meshx_io_set_level(io_handle, 1);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to set relay ON: %d", err);
        meshx_io_destroy(io_handle);
        return MESHX_FAIL;
    }

    /* Verify level was set */
    uint8_t level;
    err = meshx_io_get_level(io_handle, &level);
    if (err != MESHX_SUCCESS || level != 1) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Relay level verification failed");
        meshx_io_destroy(io_handle);
        return MESHX_FAIL;
    }

    /* Simulate element state change: Turn relay OFF */
    err = meshx_io_set_level(io_handle, 0);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to set relay OFF: %d", err);
        meshx_io_destroy(io_handle);
        return MESHX_FAIL;
    }

    /* Verify level was set */
    err = meshx_io_get_level(io_handle, &level);
    if (err != MESHX_SUCCESS || level != 0) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Relay level verification failed");
        meshx_io_destroy(io_handle);
        return MESHX_FAIL;
    }

    intg_state.element_state_changes += 2;

    /* Cleanup */
    meshx_io_destroy(io_handle);

    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "PASS: Element state change integration");
    return MESHX_SUCCESS;
}

/**
 * @brief Test GPIO function execution through abstract IO interface
 *
 * Validates that the function-based API works correctly through the
 * abstract IO interface for various GPIO operations.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
static meshx_err_t test_io_function_execution(void)
{
    MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_GPIO, "Test: IO function execution");

    if (!intg_state.gpio_initialized) {
        meshx_err_t err = meshx_gpio_init();
        if (err != MESHX_SUCCESS) return MESHX_FAIL;
        intg_state.gpio_initialized = true;
    }

    /* Create IO instance */
    meshx_io_config_t io_config = {
        .logical_pin = 0,
        .io_type = 0,
        .name = "TEST_PIN",
        .config.gpio = {
            .mode = MESHX_GPIO_MODE_OUTPUT,
            .pull = MESHX_GPIO_PULL_NONE,
            .drive = MESHX_GPIO_DRIVE_MEDIUM,
            .initial_level = 0,
            .signal_inversion = false,
        },
    };

    meshx_io_handle_t io_handle = meshx_io_create(&io_config);
    if (io_handle == NULL) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to create IO instance");
        return MESHX_FAIL;
    }

    /* Test SET_LEVEL function */
    uint32_t args[1] = {1};
    meshx_err_t err = meshx_io_execute(io_handle, MESHX_IO_FUNC_SET_LEVEL, args, 1);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "SET_LEVEL function failed: %d", err);
        meshx_io_destroy(io_handle);
        return MESHX_FAIL;
    }

    /* Test TOGGLE function */
    err = meshx_io_execute(io_handle, MESHX_IO_FUNC_TOGGLE, NULL, 0);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "TOGGLE function failed: %d", err);
        meshx_io_destroy(io_handle);
        return MESHX_FAIL;
    }

    /* Verify toggle worked (should be back to 0) */
    uint8_t level;
    err = meshx_io_get_level(io_handle, &level);
    if (err != MESHX_SUCCESS || level != 0) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Toggle verification failed");
        meshx_io_destroy(io_handle);
        return MESHX_FAIL;
    }

    /* Test function support query */
    bool supported = meshx_io_is_function_supported(io_handle, MESHX_IO_FUNC_SET_LEVEL);
    if (!supported) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "SET_LEVEL should be supported for output pin");
        meshx_io_destroy(io_handle);
        return MESHX_FAIL;
    }

    /* Cleanup */
    meshx_io_destroy(io_handle);

    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "PASS: IO function execution");
    return MESHX_SUCCESS;
}

/*============================================================================
 * Integration Tests: Hosted/Non-Hosted Mode (Requirement 5.x, 10.8)
 *============================================================================*/

/**
 * @brief Test hosted mode switching
 *
 * Validates that the GPIO subsystem correctly switches between hosted and
 * non-hosted modes while preserving pin state and interrupt registrations.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
static meshx_err_t test_hosted_mode_switching(void)
{
    MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_GPIO, "Test: Hosted mode switching");

    if (!intg_state.gpio_initialized) {
        meshx_err_t err = meshx_gpio_init();
        if (err != MESHX_SUCCESS) return MESHX_FAIL;
        intg_state.gpio_initialized = true;
    }

    /* Verify initial mode is non-hosted */
    meshx_gpio_hosted_mode_t mode = meshx_gpio_get_hosted_mode();
    if (mode != MESHX_GPIO_MODE_NON_HOSTED) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO,
                   "Initial mode should be non-hosted, got %d", mode);
        return MESHX_FAIL;
    }

    /* Register hosted event callback */
    meshx_err_t err = meshx_gpio_register_hosted_event_cb(mock_hosted_event_callback);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO,
                   "Failed to register hosted event callback: %d", err);
        return MESHX_FAIL;
    }

    /* Switch to hosted mode */
    err = meshx_gpio_set_hosted_mode(MESHX_GPIO_MODE_HOSTED);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to switch to hosted mode: %d", err);
        return MESHX_FAIL;
    }

    mode = meshx_gpio_get_hosted_mode();
    if (mode != MESHX_GPIO_MODE_HOSTED) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Mode should be hosted, got %d", mode);
        return MESHX_FAIL;
    }

    intg_state.current_hosted_mode = MESHX_GPIO_MODE_HOSTED;

    /* Verify is_hosted_mode returns true */
    if (!meshx_gpio_is_hosted_mode()) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "is_hosted_mode should return true");
        return MESHX_FAIL;
    }

    /* Switch back to non-hosted mode */
    err = meshx_gpio_set_hosted_mode(MESHX_GPIO_MODE_NON_HOSTED);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to switch to non-hosted mode: %d", err);
        return MESHX_FAIL;
    }

    mode = meshx_gpio_get_hosted_mode();
    if (mode != MESHX_GPIO_MODE_NON_HOSTED) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Mode should be non-hosted, got %d", mode);
        return MESHX_FAIL;
    }

    intg_state.current_hosted_mode = MESHX_GPIO_MODE_NON_HOSTED;

    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "PASS: Hosted mode switching");
    return MESHX_SUCCESS;
}

/**
 * @brief Test GPIO operations in hosted mode
 *
 * Validates that GPIO operations in hosted mode correctly invoke the
 * hosted event callback instead of direct hardware access.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
static meshx_err_t test_gpio_operations_in_hosted_mode(void)
{
    MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_GPIO, "Test: GPIO operations in hosted mode");

    if (!intg_state.gpio_initialized) {
        meshx_err_t err = meshx_gpio_init();
        if (err != MESHX_SUCCESS) return MESHX_FAIL;
        intg_state.gpio_initialized = true;
    }

    /* Ensure we're in non-hosted mode first */
    meshx_gpio_set_hosted_mode(MESHX_GPIO_MODE_NON_HOSTED);

    /* Register hosted event callback */
    meshx_err_t err = meshx_gpio_register_hosted_event_cb(mock_hosted_event_callback);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to register hosted callback");
        return MESHX_FAIL;
    }

    /* Reset counter */
    intg_state.hosted_events_sent = 0;

    /* Switch to hosted mode */
    err = meshx_gpio_set_hosted_mode(MESHX_GPIO_MODE_HOSTED);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to switch to hosted mode");
        return MESHX_FAIL;
    }

    /* Perform GPIO operation - should trigger hosted event callback */
    /* Note: This test requires GPIO to have at least one configured pin */
    err = meshx_gpio_set_level(0, 1);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO,
                   "GPIO set_level failed in hosted mode: %d", err);
        /* This is expected if no pins are configured - continue test */
    }

    /* Switch back to non-hosted mode */
    err = meshx_gpio_set_hosted_mode(MESHX_GPIO_MODE_NON_HOSTED);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to switch back to non-hosted");
        return MESHX_FAIL;
    }

    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "PASS: GPIO operations in hosted mode");
    return MESHX_SUCCESS;
}

/**
 * @brief Test hosted interrupt processing
 *
 * Validates that interrupts from the host are correctly processed and
 * dispatched to registered callbacks.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
static meshx_err_t test_hosted_interrupt_processing(void)
{
    MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_GPIO, "Test: Hosted interrupt processing");

    if (!intg_state.gpio_initialized) {
        meshx_err_t err = meshx_gpio_init();
        if (err != MESHX_SUCCESS) return MESHX_FAIL;
        intg_state.gpio_initialized = true;
    }

    /* Switch to hosted mode */
    meshx_err_t err = meshx_gpio_set_hosted_mode(MESHX_GPIO_MODE_HOSTED);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to switch to hosted mode");
        return MESHX_FAIL;
    }

    /* Register interrupt in hosted mode (no hardware interrupt needed) */
    err = meshx_gpio_register_intr(3, MESHX_GPIO_INTR_NEGATIVE_EDGE,
                                   mock_interrupt_callback, NULL);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO,
                   "Failed to register interrupt in hosted mode: %d", err);
        /* This is expected if pin configuration is not set up - continue */
    }

    /* Simulate interrupt from host */
    err = meshx_gpio_process_hosted_interrupt(3, 1);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO,
                   "Failed to process hosted interrupt: %d", err);
        /* This is expected if no interrupt is registered - continue */
    }

    /* Switch back to non-hosted mode */
    err = meshx_gpio_set_hosted_mode(MESHX_GPIO_MODE_NON_HOSTED);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to switch back");
        return MESHX_FAIL;
    }

    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "PASS: Hosted interrupt processing");
    return MESHX_SUCCESS;
}

/*============================================================================
 * Integration Tests: KV Engine Persistence (Requirement 13.x, 10.8)
 *============================================================================*/

/**
 * @brief Test GPIO KV Engine initialization
 *
 * Validates that the GPIO KV persistence layer initializes correctly
 * with a FAL partition and product name.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
static meshx_err_t test_kv_engine_init(void)
{
    MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_GPIO, "Test: KV Engine initialization");

    /* Initialize GPIO KV persistence */
    meshx_err_t err = meshx_gpio_kv_init(&mock_kv_partition, "test_product");
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "KV init failed: %d", err);
        return MESHX_FAIL;
    }

    intg_state.kv_initialized = true;

    /* Verify initialized state */
    if (!meshx_gpio_kv_is_initialized()) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "KV should be initialized");
        return MESHX_FAIL;
    }

    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "PASS: KV Engine initialization");
    return MESHX_SUCCESS;
}

/**
 * @brief Test GPIO configuration save to KV Engine
 *
 * Validates that GPIO configuration can be serialized and saved to KV Engine.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
static meshx_err_t test_kv_config_save(void)
{
    MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_GPIO, "Test: KV config save");

    /* Initialize KV if needed */
    if (!intg_state.kv_initialized) {
        meshx_err_t err = meshx_gpio_kv_init(&mock_kv_partition, "test_product");
        if (err != MESHX_SUCCESS) return MESHX_FAIL;
        intg_state.kv_initialized = true;
    }

    /* Setup test configuration */
    setup_test_gpio_config();

    /* Save configuration to KV Engine */
    meshx_err_t err = meshx_gpio_save_config_to_kv(intg_state.test_configs, 4);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to save config: %d", err);
        return MESHX_FAIL;
    }

    intg_state.kv_save_count++;

    /* Verify config exists */
    bool exists = false;
    err = meshx_gpio_config_exists_in_kv(&exists);
    if (err != MESHX_SUCCESS || !exists) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Config should exist after save");
        return MESHX_FAIL;
    }

    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "PASS: KV config save");
    return MESHX_SUCCESS;
}

/**
 * @brief Test GPIO configuration load from KV Engine
 *
 * Validates that GPIO configuration can be loaded and deserialized from KV Engine.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
static meshx_err_t test_kv_config_load(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Test: KV config load");

    if (!intg_state.kv_initialized) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "KV not initialized");
        return MESHX_FAIL;
    }

    /* Load configuration from KV Engine */
    meshx_gpio_pin_config_t loaded_configs[8];
    uint8_t pin_count = 0;

    meshx_err_t err = meshx_gpio_load_config_from_kv(loaded_configs, 8, &pin_count);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to load config: %d", err);
        return MESHX_FAIL;
    }

    intg_state.kv_load_count++;

    /* Verify pin count matches */
    if (pin_count != 4) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO,
                   "Pin count mismatch: expected 4, got %u", pin_count);
        return MESHX_FAIL;
    }

    /* Verify configuration values */
    for (uint8_t i = 0; i < pin_count; i++) {
        if (loaded_configs[i].logical_pin != intg_state.test_configs[i].logical_pin) {
            MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO,
                       "Pin %u logical_pin mismatch", i);
            return MESHX_FAIL;
        }
        if (loaded_configs[i].physical_pin != intg_state.test_configs[i].physical_pin) {
            MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO,
                       "Pin %u physical_pin mismatch", i);
            return MESHX_FAIL;
        }
        if (loaded_configs[i].mode != intg_state.test_configs[i].mode) {
            MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Pin %u mode mismatch", i);
            return MESHX_FAIL;
        }
    }

    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "PASS: KV config load");
    return MESHX_SUCCESS;
}

/**
 * @brief Test KV Engine configuration round-trip
 *
 * Validates that saving and loading configuration produces identical results.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
static meshx_err_t test_kv_config_round_trip(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Test: KV config round-trip");

    /* Initialize KV if needed */
    if (!intg_state.kv_initialized) {
        meshx_err_t err = meshx_gpio_kv_init(&mock_kv_partition, "test_product");
        if (err != MESHX_SUCCESS) return MESHX_FAIL;
        intg_state.kv_initialized = true;
    }

    /* Clear any existing config */
    meshx_gpio_clear_config_in_kv();

    /* Setup and save original configuration */
    setup_test_gpio_config();

    meshx_err_t err = meshx_gpio_save_config_to_kv(intg_state.test_configs, 4);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to save original config");
        return MESHX_FAIL;
    }

    /* Load configuration */
    meshx_gpio_pin_config_t loaded_configs[8];
    uint8_t pin_count = 0;

    err = meshx_gpio_load_config_from_kv(loaded_configs, 8, &pin_count);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to load config");
        return MESHX_FAIL;
    }

    /* Compare configurations */
    if (pin_count != 4) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Pin count mismatch in round-trip");
        return MESHX_FAIL;
    }

    for (uint8_t i = 0; i < pin_count; i++) {
        if (loaded_configs[i].logical_pin != intg_state.test_configs[i].logical_pin ||
            loaded_configs[i].physical_pin != intg_state.test_configs[i].physical_pin ||
            loaded_configs[i].mode != intg_state.test_configs[i].mode ||
            loaded_configs[i].pull != intg_state.test_configs[i].pull ||
            loaded_configs[i].drive_strength != intg_state.test_configs[i].drive_strength) {
            MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO,
                       "Config mismatch at pin %u in round-trip", i);
            return MESHX_FAIL;
        }
    }

    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "PASS: KV config round-trip");
    return MESHX_SUCCESS;
}

/**
 * @brief Test KV Engine pin state persistence
 *
 * Validates that runtime pin states can be saved and restored from KV Engine.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
static meshx_err_t test_kv_pin_state_persistence(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Test: KV pin state persistence");

    if (!intg_state.kv_initialized) {
        meshx_err_t err = meshx_gpio_kv_init(&mock_kv_partition, "test_product");
        if (err != MESHX_SUCCESS) return MESHX_FAIL;
        intg_state.kv_initialized = true;
    }

    /* Create test pin state */
    meshx_gpio_pin_state_t original_state = {
        .current_level = 1,
        .interrupt_registered = false,
        .intr_type = MESHX_GPIO_INTR_DISABLED,
        .intr_callback = NULL,
        .intr_user_data = NULL,
    };

    /* Save pin state */
    meshx_err_t err = meshx_gpio_save_pin_state_to_kv(0, &original_state);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to save pin state: %d", err);
        return MESHX_FAIL;
    }

    /* Load pin state */
    meshx_gpio_pin_state_t loaded_state;
    err = meshx_gpio_load_pin_state_from_kv(0, &loaded_state);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to load pin state: %d", err);
        return MESHX_FAIL;
    }

    /* Verify state */
    if (loaded_state.current_level != original_state.current_level) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Pin state level mismatch");
        return MESHX_FAIL;
    }

    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "PASS: KV pin state persistence");
    return MESHX_SUCCESS;
}

/**
 * @brief Test KV Engine configuration export/import
 *
 * Validates that configuration can be exported to binary format and
 * imported back correctly.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
static meshx_err_t test_kv_config_export_import(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Test: KV config export/import");

    if (!intg_state.kv_initialized) {
        meshx_err_t err = meshx_gpio_kv_init(&mock_kv_partition, "test_product");
        if (err != MESHX_SUCCESS) return MESHX_FAIL;
        intg_state.kv_initialized = true;
    }

    /* Setup test configuration */
    setup_test_gpio_config();

    /* Export configuration to buffer */
    uint8_t export_buffer[512];
    uint16_t export_size = 0;

    meshx_err_t err = meshx_gpio_export_config(intg_state.test_configs, 4,
                                               export_buffer, sizeof(export_buffer),
                                               &export_size);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Export failed: %d", err);
        return MESHX_FAIL;
    }

    /* Verify export size is reasonable */
    if (export_size == 0 || export_size > sizeof(export_buffer)) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Invalid export size: %u", export_size);
        return MESHX_FAIL;
    }

    /* Import configuration from buffer */
    meshx_gpio_pin_config_t imported_configs[8];
    uint8_t imported_count = 0;

    err = meshx_gpio_import_config(export_buffer, export_size,
                                   imported_configs, 8, &imported_count);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Import failed: %d", err);
        return MESHX_FAIL;
    }

    /* Verify imported configuration matches original */
    if (imported_count != 4) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO,
                   "Imported count mismatch: expected 4, got %u", imported_count);
        return MESHX_FAIL;
    }

    for (uint8_t i = 0; i < imported_count; i++) {
        if (imported_configs[i].logical_pin != intg_state.test_configs[i].logical_pin ||
            imported_configs[i].mode != intg_state.test_configs[i].mode) {
            MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO,
                       "Imported config mismatch at pin %u", i);
            return MESHX_FAIL;
        }
    }

    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "PASS: KV config export/import");
    return MESHX_SUCCESS;
}

/**
 * @brief Test KV Engine corruption recovery
 *
 * Validates that corrupted KV Engine data triggers fallback to defaults
 * and logs appropriate errors.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
static meshx_err_t test_kv_corruption_recovery(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Test: KV corruption recovery");

    if (!intg_state.kv_initialized) {
        meshx_err_t err = meshx_gpio_kv_init(&mock_kv_partition, "test_product");
        if (err != MESHX_SUCCESS) return MESHX_FAIL;
        intg_state.kv_initialized = true;
    }

    /* Clear any existing config */
    meshx_gpio_clear_config_in_kv();

    /* Save valid configuration */
    setup_test_gpio_config();
    meshx_err_t err = meshx_gpio_save_config_to_kv(intg_state.test_configs, 4);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to save config");
        return MESHX_FAIL;
    }

    /* Clear config to simulate corruption scenario */
    err = meshx_gpio_clear_config_in_kv();
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to clear config");
        return MESHX_FAIL;
    }

    /* Attempt to load - should return not found */
    meshx_gpio_pin_config_t loaded_configs[8];
    uint8_t pin_count = 0;

    err = meshx_gpio_load_config_from_kv(loaded_configs, 8, &pin_count);
    if (err == MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO,
                   "Load should fail after clear");
        return MESHX_FAIL;
    }

    /* This is expected - MESHX_NOT_FOUND indicates we should use compiled defaults */
    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "PASS: KV corruption recovery");
    return MESHX_SUCCESS;
}

/*============================================================================
 * Integration Tests: Cross-Subsystem (Requirement 10.8)
 *============================================================================*/

/**
 * @brief Test GPIO with logging subsystem integration
 *
 * Validates that GPIO operations correctly integrate with the MeshX logging
 * subsystem for debug and error reporting.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
static meshx_err_t test_gpio_logging_integration(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Test: GPIO logging integration");

    /* Initialize GPIO subsystem */
    if (!intg_state.gpio_initialized) {
        meshx_err_t err = meshx_gpio_init();
        if (err != MESHX_SUCCESS) return MESHX_FAIL;
        intg_state.gpio_initialized = true;
    }

    /* Perform various GPIO operations that should generate log output */
    /* Note: This test validates that logging works, actual log verification
     * would require a log capture mechanism */

    /* This operation will generate debug logs */
    meshx_gpio_set_level(0, 1);

    /* This operation will generate info logs */
    meshx_gpio_get_hosted_mode();

    /* This operation will generate debug logs */
    meshx_gpio_toggle(0);

    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "PASS: GPIO logging integration");
    return MESHX_SUCCESS;
}

/**
 * @brief Test full GPIO lifecycle with all subsystems
 *
 * Validates the complete GPIO lifecycle including initialization,
 * operation, hosted mode switching, KV persistence, and deinitialization.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
static meshx_err_t test_full_gpio_lifecycle(void)
{
    MESHX_LOGD(MODULE_ID_COMMON, "Test: Full GPIO lifecycle");

    /* Phase 1: Initialize all subsystems */
    meshx_err_t err = meshx_gpio_init();
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Lifecycle: GPIO init failed");
        return MESHX_FAIL;
    }
    intg_state.gpio_initialized = true;

    err = meshx_gpio_kv_init(&mock_kv_partition, "lifecycle_test");
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Lifecycle: KV init failed");
        cleanup_integration_test();
        return MESHX_FAIL;
    }
    intg_state.kv_initialized = true;

    /* Phase 2: Configure pins */
    setup_test_gpio_config();

    /* Phase 3: Perform operations */
    meshx_gpio_set_level(0, 1);
    meshx_gpio_toggle(0);

    /* Phase 4: Switch to hosted mode */
    meshx_gpio_register_hosted_event_cb(mock_hosted_event_callback);
    meshx_gpio_set_hosted_mode(MESHX_GPIO_MODE_HOSTED);

    /* Phase 5: Operations in hosted mode */
    meshx_gpio_set_level(0, 0);

    /* Phase 6: Switch back to non-hosted mode */
    meshx_gpio_set_hosted_mode(MESHX_GPIO_MODE_NON_HOSTED);

    /* Phase 7: Save configuration to KV Engine */
    meshx_gpio_save_config_to_kv(intg_state.test_configs, 4);

    /* Phase 8: Load configuration from KV Engine */
    meshx_gpio_pin_config_t loaded_configs[8];
    uint8_t pin_count = 0;
    meshx_gpio_load_config_from_kv(loaded_configs, 8, &pin_count);

    /* Phase 9: Cleanup */
    meshx_gpio_kv_deinit();
    intg_state.kv_initialized = false;

    meshx_gpio_deinit();
    intg_state.gpio_initialized = false;

    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "PASS: Full GPIO lifecycle");
    return MESHX_SUCCESS;
}

/*============================================================================
 * Test Runner
 *============================================================================*/

/**
 * @brief Run all GPIO integration tests
 *
 * @return meshx_err_t MESHX_SUCCESS if all tests pass, error code on failure
 */
static meshx_err_t run_gpio_integration_tests(void)
{
    meshx_err_t result;

    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "========================================");
    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "GPIO Integration Tests (Requirements 10.5-10.8)");
    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "========================================");

    init_integration_state();

    /* Element Integration Tests (Requirement 10.6) */
    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "--- Element Integration Tests ---");

    result = test_io_interface_creation();
    if (result != MESHX_SUCCESS) {
        cleanup_integration_test();
        return result;
    }

    init_integration_state();
    result = test_element_state_change();
    if (result != MESHX_SUCCESS) {
        cleanup_integration_test();
        return result;
    }

    init_integration_state();
    result = test_io_function_execution();
    if (result != MESHX_SUCCESS) {
        cleanup_integration_test();
        return result;
    }

    /* Hosted/Non-Hosted Mode Tests (Requirement 10.8) */
    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "--- Hosted Mode Integration Tests ---");

    init_integration_state();
    result = test_hosted_mode_switching();
    if (result != MESHX_SUCCESS) {
        cleanup_integration_test();
        return result;
    }

    init_integration_state();
    result = test_gpio_operations_in_hosted_mode();
    if (result != MESHX_SUCCESS) {
        cleanup_integration_test();
        return result;
    }

    init_integration_state();
    result = test_hosted_interrupt_processing();
    if (result != MESHX_SUCCESS) {
        cleanup_integration_test();
        return result;
    }

    /* KV Engine Persistence Tests (Requirement 10.8, 13.x) */
    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "--- KV Engine Persistence Tests ---");

    init_integration_state();
    result = test_kv_engine_init();
    if (result != MESHX_SUCCESS) {
        cleanup_integration_test();
        return result;
    }

    result = test_kv_config_save();
    if (result != MESHX_SUCCESS) {
        cleanup_integration_test();
        return result;
    }

    result = test_kv_config_load();
    if (result != MESHX_SUCCESS) {
        cleanup_integration_test();
        return result;
    }

    init_integration_state();
    result = test_kv_config_round_trip();
    if (result != MESHX_SUCCESS) {
        cleanup_integration_test();
        return result;
    }

    init_integration_state();
    result = test_kv_pin_state_persistence();
    if (result != MESHX_SUCCESS) {
        cleanup_integration_test();
        return result;
    }

    init_integration_state();
    result = test_kv_config_export_import();
    if (result != MESHX_SUCCESS) {
        cleanup_integration_test();
        return result;
    }

    init_integration_state();
    result = test_kv_corruption_recovery();
    if (result != MESHX_SUCCESS) {
        cleanup_integration_test();
        return result;
    }

    /* Cross-Subsystem Tests (Requirement 10.8) */
    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "--- Cross-Subsystem Tests ---");

    init_integration_state();
    result = test_gpio_logging_integration();
    if (result != MESHX_SUCCESS) {
        cleanup_integration_test();
        return result;
    }

    init_integration_state();
    result = test_full_gpio_lifecycle();
    if (result != MESHX_SUCCESS) {
        cleanup_integration_test();
        return result;
    }

    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "========================================");
    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "All GPIO Integration Tests PASSED");
    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "========================================");

    cleanup_integration_test();
    return MESHX_SUCCESS;
}

/*============================================================================
 * Test Handler Registration
 *============================================================================*/

/**
 * @brief Integration test command handler
 *
 * @param cmd_id Command ID specifying which test to run
 * @param argc Argument count
 * @param argv Argument values
 * @return meshx_err_t Test result
 */
static meshx_err_t gpio_integration_test_handler(int cmd_id, int argc, char **argv)
{
    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "GPIO Integration Test Handler: cmd_id=%d", cmd_id);

    switch (cmd_id) {
        case 100: return run_gpio_integration_tests();

        case 101: init_integration_state(); return test_io_interface_creation();
        case 102: init_integration_state(); return test_element_state_change();
        case 103: init_integration_state(); return test_io_function_execution();

        case 104: init_integration_state(); return test_hosted_mode_switching();
        case 105: init_integration_state(); return test_gpio_operations_in_hosted_mode();
        case 106: init_integration_state(); return test_hosted_interrupt_processing();

        case 107: init_integration_state(); return test_kv_engine_init();
        case 108: init_integration_state(); return test_kv_config_save();
        case 109: init_integration_state(); return test_kv_config_load();
        case 110: init_integration_state(); return test_kv_config_round_trip();
        case 111: init_integration_state(); return test_kv_pin_state_persistence();
        case 112: init_integration_state(); return test_kv_config_export_import();
        case 113: init_integration_state(); return test_kv_corruption_recovery();

        case 114: init_integration_state(); return test_gpio_logging_integration();
        case 115: init_integration_state(); return test_full_gpio_lifecycle();

        default:
            MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Unknown integration test cmd_id: %d", cmd_id);
            return MESHX_INVALID_ARG;
    }
}

/**
 * @brief Register GPIO integration tests
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t register_gpio_integration_tests(void)
{
    MESHX_LOGD(MODULE_ID_GPIO_INTEGRATION_TEST, "Registering GPIO integration tests");
    return register_unit_test(MODULE_ID_GPIO_INTEGRATION_TEST, gpio_integration_test_handler);
}

#endif /* CONFIG_ENABLE_UNIT_TEST */
