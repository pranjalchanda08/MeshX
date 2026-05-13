/**
 * @file meshx_gpio_esp.c
 * @brief ESP-IDF GPIO Platform Implementation
 *
 * This file implements the MeshX GPIO platform interface using ESP-IDF GPIO API.
 * It provides GPIO operations and interrupt handling for ESP32 platforms.
 *
 * @author MeshX Team
 * @date 2024
 */

#include "interface/gpio/meshx_gpio_platform.h"
#include "interface/logging/meshx_log.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_intr_alloc.h"
#include <string.h>

#define MODULE_ID_PLATFORM_GPIO MODULE_ID_COMMON

// Maximum number of GPIO pins supported
#define ESP_GPIO_MAX_PINS GPIO_NUM_MAX

// Structure to track GPIO interrupt state
typedef struct {
    bool registered;
    void (*isr_handler)(void*);
    void *arg;
    gpio_isr_t esp_isr_handler;
} esp_gpio_intr_state_t;

// Static state for GPIO interrupts
static esp_gpio_intr_state_t esp_gpio_intr_states[ESP_GPIO_MAX_PINS];

// Static flag to track initialization
static bool gpio_initialized = false;

// Forward declarations
static void esp_gpio_isr_handler(void* arg);

/**
 * @brief Convert MeshX GPIO mode to ESP-IDF GPIO mode
 */
static gpio_mode_t meshx_gpio_mode_to_esp(meshx_gpio_mode_t mode)
{
    switch (mode) {
        case MESHX_GPIO_MODE_INPUT:
            return GPIO_MODE_INPUT;
        case MESHX_GPIO_MODE_OUTPUT:
            return GPIO_MODE_OUTPUT;
        case MESHX_GPIO_MODE_INPUT_OUTPUT:
            return GPIO_MODE_INPUT_OUTPUT;
        case MESHX_GPIO_MODE_OPEN_DRAIN:
            return GPIO_MODE_OUTPUT_OD;
        case MESHX_GPIO_MODE_OPEN_DRAIN_INPUT_OUTPUT:
            return GPIO_MODE_INPUT_OUTPUT_OD;
        case MESHX_GPIO_MODE_PWM_OUTPUT:
            return GPIO_MODE_OUTPUT; // PWM uses output mode with LEDC
        default:
            return GPIO_MODE_DISABLE;
    }
}


/**
 * @brief Convert MeshX GPIO drive strength to ESP-IDF GPIO drive strength
 */
static gpio_drive_cap_t meshx_gpio_drive_to_esp(meshx_gpio_drive_t drive)
{
    switch (drive) {
        case MESHX_GPIO_DRIVE_WEAK:
            return GPIO_DRIVE_CAP_0;
        case MESHX_GPIO_DRIVE_MEDIUM:
            return GPIO_DRIVE_CAP_1;
        case MESHX_GPIO_DRIVE_STRONG:
            return GPIO_DRIVE_CAP_2;
        case MESHX_GPIO_DRIVE_MAX_STRONG:
            return GPIO_DRIVE_CAP_3;
        default:
            return GPIO_DRIVE_CAP_DEFAULT;
    }
}

/**
 * @brief Convert MeshX GPIO interrupt type to ESP-IDF GPIO interrupt type
 */
static gpio_int_type_t meshx_gpio_intr_to_esp(meshx_gpio_intr_type_t intr_type)
{
    switch (intr_type) {
        case MESHX_GPIO_INTR_DISABLED:
            return GPIO_INTR_DISABLE;
        case MESHX_GPIO_INTR_POSITIVE_EDGE:
            return GPIO_INTR_POSEDGE;
        case MESHX_GPIO_INTR_NEGATIVE_EDGE:
            return GPIO_INTR_NEGEDGE;
        case MESHX_GPIO_INTR_ANY_EDGE:
            return GPIO_INTR_ANYEDGE;
        case MESHX_GPIO_INTR_LOW_LEVEL:
            return GPIO_INTR_LOW_LEVEL;
        case MESHX_GPIO_INTR_HIGH_LEVEL:
            return GPIO_INTR_HIGH_LEVEL;
        default:
            return GPIO_INTR_DISABLE;
    }
}

/**
 * @brief Convert ESP-IDF error to MeshX error
 */
static meshx_err_t esp_err_to_meshx_err(esp_err_t err)
{
    switch (err) {
        case ESP_OK:
            return MESHX_SUCCESS;
        case ESP_ERR_INVALID_ARG:
            return MESHX_INVALID_ARG;
        case ESP_ERR_NOT_SUPPORTED:
            return MESHX_NOT_SUPPORTED;
        case ESP_ERR_NOT_FOUND:
            return MESHX_NOT_FOUND;
        case ESP_FAIL:
        default:
            return MESHX_FAIL;
    }
}

/**
 * @brief Platform-specific GPIO initialization
 */
meshx_err_t meshx_gpio_platform_init(void)
{
    if (gpio_initialized) {
        MESHX_LOGW(MODULE_ID_PLATFORM_GPIO, "GPIO already initialized");
        return MESHX_SUCCESS;
    }

    // Initialize GPIO interrupt states
    memset(esp_gpio_intr_states, 0, sizeof(esp_gpio_intr_states));

    // Install GPIO ISR service
    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        MESHX_LOGE(MODULE_ID_PLATFORM_GPIO, "Failed to install GPIO ISR service: %d", err);
        return esp_err_to_meshx_err(err);
    }

    gpio_initialized = true;
    MESHX_LOGD(MODULE_ID_PLATFORM_GPIO, "GPIO platform initialized");
    return MESHX_SUCCESS;
}

/**
 * @brief Platform-specific GPIO deinitialization
 */
meshx_err_t meshx_gpio_platform_deinit(void)
{
    if (!gpio_initialized) {
        return MESHX_SUCCESS;
    }

    // Disable all interrupts
    for (int i = 0; i < ESP_GPIO_MAX_PINS; i++) {
        if (esp_gpio_intr_states[i].registered) {
            gpio_isr_handler_remove(i);
            esp_gpio_intr_states[i].registered = false;
        }
    }

    // Uninstall GPIO ISR service
    gpio_uninstall_isr_service();

    gpio_initialized = false;
    MESHX_LOGD(MODULE_ID_PLATFORM_GPIO, "GPIO platform deinitialized");
    return MESHX_SUCCESS;
}

/**
 * @brief Platform-specific pin level set
 */
meshx_err_t meshx_gpio_platform_set_level(uint8_t physical_pin, uint8_t level)
{
    if (physical_pin >= ESP_GPIO_MAX_PINS) {
#if CONFIG_ENABLE_UNIT_TEST
        return MESHX_SUCCESS;
#else
        return MESHX_ERR_GPIO_INVALID_PIN;
#endif
    }

    gpio_set_level(physical_pin, level);
    return MESHX_SUCCESS;
}

/**
 * @brief Platform-specific pin level get
 */
meshx_err_t meshx_gpio_platform_get_level(uint8_t physical_pin, uint8_t *level)
{
    if (physical_pin >= ESP_GPIO_MAX_PINS) {
#if CONFIG_ENABLE_UNIT_TEST
        if (level) *level = 0;
        return MESHX_SUCCESS;
#else
        return MESHX_ERR_GPIO_INVALID_PIN;
#endif
    }

    if (level == NULL) {
        return MESHX_INVALID_ARG;
    }

    *level = gpio_get_level(physical_pin);
    return MESHX_SUCCESS;
}

/**
 * @brief Platform-specific pin mode configuration
 */
meshx_err_t meshx_gpio_platform_configure_pin(uint8_t physical_pin,
                                              meshx_gpio_mode_t mode,
                                              meshx_gpio_pull_t pull,
                                              meshx_gpio_drive_t drive)
{
    if (physical_pin >= ESP_GPIO_MAX_PINS) {
#if CONFIG_ENABLE_UNIT_TEST
        return MESHX_SUCCESS;
#else
        return MESHX_ERR_GPIO_INVALID_PIN;
#endif
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << physical_pin),
        .mode = meshx_gpio_mode_to_esp(mode),
        .pull_up_en = (pull == MESHX_GPIO_PULL_UP || pull == MESHX_GPIO_PULL_UP_DOWN) ? 1 : 0,
        .pull_down_en = (pull == MESHX_GPIO_PULL_DOWN || pull == MESHX_GPIO_PULL_UP_DOWN) ? 1 : 0,
        .intr_type = GPIO_INTR_DISABLE
    };

    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        MESHX_LOGE(MODULE_ID_PLATFORM_GPIO, "Failed to configure pin %d: %d", physical_pin, err);
        return esp_err_to_meshx_err(err);
    }

    // Set drive strength if not default
    if (drive != MESHX_GPIO_DRIVE_WEAK) { // WEAK is default
        err = gpio_set_drive_capability(physical_pin, meshx_gpio_drive_to_esp(drive));
        if (err != ESP_OK) {
            MESHX_LOGW(MODULE_ID_PLATFORM_GPIO, "Failed to set drive strength for pin %d: %d", physical_pin, err);
            // Continue anyway, drive strength is not critical
        }
    }

    return MESHX_SUCCESS;
}

/**
 * @brief ESP-IDF GPIO ISR handler wrapper
 */
static void esp_gpio_isr_handler(void* arg)
{
    uint8_t physical_pin = (uintptr_t)arg;

    if (physical_pin < ESP_GPIO_MAX_PINS &&
        esp_gpio_intr_states[physical_pin].registered &&
        esp_gpio_intr_states[physical_pin].isr_handler != NULL) {

        esp_gpio_intr_states[physical_pin].isr_handler(esp_gpio_intr_states[physical_pin].arg);
    }
}

/**
 * @brief Platform-specific interrupt registration
 */
meshx_err_t meshx_gpio_platform_register_intr(uint8_t physical_pin,
                                              meshx_gpio_intr_type_t intr_type,
                                              void (*isr_handler)(void*),
                                              void *arg)
{
    if (physical_pin >= ESP_GPIO_MAX_PINS) {
#if CONFIG_ENABLE_UNIT_TEST
        return MESHX_SUCCESS;
#else
        return MESHX_ERR_GPIO_INVALID_PIN;
#endif
    }

    if (isr_handler == NULL) {
        return MESHX_INVALID_ARG;
    }

    if (esp_gpio_intr_states[physical_pin].registered) {
        return MESHX_ERR_GPIO_INTR_ALREADY_REGISTERED;
    }

    // Set interrupt type
    gpio_set_intr_type(physical_pin, meshx_gpio_intr_to_esp(intr_type));

    // Add ISR handler
    esp_err_t err = gpio_isr_handler_add(physical_pin, esp_gpio_isr_handler, (void*)(uintptr_t)physical_pin);
    if (err != ESP_OK) {
        MESHX_LOGE(MODULE_ID_PLATFORM_GPIO, "Failed to add ISR handler for pin %d: %d", physical_pin, err);
        return esp_err_to_meshx_err(err);
    }

    // Store handler state
    esp_gpio_intr_states[physical_pin].registered = true;
    esp_gpio_intr_states[physical_pin].isr_handler = isr_handler;
    esp_gpio_intr_states[physical_pin].arg = arg;
    esp_gpio_intr_states[physical_pin].esp_isr_handler = esp_gpio_isr_handler;

    MESHX_LOGD(MODULE_ID_PLATFORM_GPIO, "Registered interrupt for pin %d, type %d", physical_pin, intr_type);
    return MESHX_SUCCESS;
}

/**
 * @brief Platform-specific interrupt unregistration
 */
meshx_err_t meshx_gpio_platform_unregister_intr(uint8_t physical_pin)
{
    if (physical_pin >= ESP_GPIO_MAX_PINS) {
#if CONFIG_ENABLE_UNIT_TEST
        return MESHX_SUCCESS;
#else
        return MESHX_ERR_GPIO_INVALID_PIN;
#endif
    }

    if (!esp_gpio_intr_states[physical_pin].registered) {
        return MESHX_SUCCESS;
    }

    gpio_isr_handler_remove(physical_pin);
    gpio_set_intr_type(physical_pin, GPIO_INTR_DISABLE);

    esp_gpio_intr_states[physical_pin].registered = false;
    esp_gpio_intr_states[physical_pin].isr_handler = NULL;
    esp_gpio_intr_states[physical_pin].arg = NULL;

    MESHX_LOGD(MODULE_ID_PLATFORM_GPIO, "Unregistered interrupt for pin %d", physical_pin);
    return MESHX_SUCCESS;
}

/**
 * @brief Platform-specific interrupt enable/disable
 */
meshx_err_t meshx_gpio_platform_intr_enable(uint8_t physical_pin, bool enable)
{
    if (physical_pin >= ESP_GPIO_MAX_PINS) {
#if CONFIG_ENABLE_UNIT_TEST
        return MESHX_SUCCESS;
#else
        return MESHX_ERR_GPIO_INVALID_PIN;
#endif
    }

    if (!esp_gpio_intr_states[physical_pin].registered) {
        return MESHX_ERR_GPIO_INTR_NOT_SUPPORTED;
    }

    if (enable) {
        gpio_intr_enable(physical_pin);
    } else {
        gpio_intr_disable(physical_pin);
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Platform-specific IO function execution
 */
meshx_err_t meshx_io_platform_execute_function(uint8_t physical_pin,
                                               meshx_io_function_t function,
                                               const uint32_t *args,
                                               uint8_t arg_count)
{
#if !CONFIG_ENABLE_UNIT_TEST
    if (physical_pin >= ESP_GPIO_MAX_PINS) {
        return MESHX_ERR_GPIO_INVALID_PIN;
    }
#endif

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
            // Note: args[0] should be a pointer to store the level
            // This is handled at a higher level
            return MESHX_ERR_GPIO_INVALID_MODE;

        case MESHX_IO_FUNCTION_TOGGLE:
            // Toggle requires reading current level first
            {
                uint8_t current_level;
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
            // These functions are handled at a higher level
            // or through dedicated API functions
            return MESHX_ERR_GPIO_INVALID_MODE;
#endif

        case MESHX_IO_FUNCTION_CUSTOM:
            // Custom functions not implemented in this basic version
            return MESHX_NOT_SUPPORTED;

        default:
            return MESHX_NOT_SUPPORTED;
    }
}

/**
 * @brief Platform-specific custom function registration
 */
meshx_err_t meshx_io_platform_register_custom_function(uint16_t function_id,
                                                       meshx_err_t (*handler)(uint8_t physical_pin,
                                                                              const uint32_t *args,
                                                                              uint8_t arg_count,
                                                                              void *user_data),
                                                       void *user_data)
{
    // Custom function registration not implemented in this basic version
    (void)function_id;
    (void)handler;
    (void)user_data;
    return MESHX_NOT_SUPPORTED;
}
