/**
 * @file meshx_pwm.c
 * @brief MeshX PWM Runtime Implementation
 *
 * This file implements the PWM runtime subsystem for MeshX.
 * It provides the runtime API for PWM operations with parameter validation,
 * hardware channel allocation, and PWM state management.
 *
 * @author MeshX Team
 * @date 2024
 */

#include <string.h>
#include <stdbool.h>
#include "interface/gpio/meshx_pwm.h"
#include "interface/gpio/meshx_gpio_platform.h"
#include "../../inc/meshx_err.h"
#include "../../interface/logging/meshx_log.h"
#include "../../inc/module_id.h"

/**
 * @brief PWM configuration structure
 */
typedef struct {
    uint8_t logical_pin;                    /**< Logical pin number */
    uint32_t frequency;                     /**< PWM frequency in Hz */
    uint8_t duty_cycle;                     /**< Duty cycle (0-100%) */
    uint8_t resolution;                     /**< Resolution in bits */
    uint8_t channel;                        /**< Hardware channel number */
} meshx_pwm_config_t;

/**
 * @brief PWM runtime state structure
 */
typedef struct {
    bool started;                           /**< PWM output started flag */
    uint32_t frequency;                     /**< Current frequency in Hz */
    uint8_t duty_cycle;                     /**< Current duty cycle (0-100%) */
    uint8_t resolution;                     /**< Current resolution in bits */
} meshx_pwm_state_t;

/**
 * @brief PWM runtime state structure
 */
typedef struct {
    bool initialized;                       /**< PWM subsystem initialized flag */
    uint8_t pwm_pin_count;                  /**< Number of configured PWM pins */
    meshx_pwm_config_t* pwm_configs;        /**< PWM configuration array */
    meshx_pwm_state_t* pwm_states;          /**< PWM runtime state array */
    uint8_t channel_allocation[16];         /**< PWM channel allocation tracking */
    uint8_t allocated_channels;             /**< Number of allocated channels */
} meshx_pwm_runtime_t;

static meshx_pwm_runtime_t pwm_runtime;
#if CONFIG_ENABLE_UNIT_TEST
static meshx_pwm_config_t test_pwm_configs[128];
static meshx_pwm_state_t test_pwm_states[128];
#endif

/**
 * @brief Initialize PWM subsystem
 *
 * This function initializes the PWM subsystem based on compiled configuration.
 * It sets up hardware PWM channels and configures all PWM pins according to
 * their YAML configuration.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_pwm_init(void)
{
    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "Initializing PWM subsystem");

    if (pwm_runtime.initialized) {
        MESHX_LOGW(MODULE_ID_COMPONENT_MESHX_GPIO, "PWM already initialized");
        return MESHX_SUCCESS;
    }

    // Initialize platform-specific PWM
    meshx_err_t err = meshx_pwm_platform_init();
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Platform PWM init failed: %d", err);
        return err;
    }

    // TODO: Load configuration from generated headers
    // For now, initialize with empty configuration
    pwm_runtime.pwm_pin_count = 0;
    pwm_runtime.pwm_configs = NULL;
    pwm_runtime.pwm_states = NULL;
    memset(pwm_runtime.channel_allocation, 0xFF, sizeof(pwm_runtime.channel_allocation));
    pwm_runtime.allocated_channels = 0;

    pwm_runtime.initialized = true;
#if CONFIG_ENABLE_UNIT_TEST
    pwm_runtime.pwm_pin_count = 128;
    pwm_runtime.pwm_configs = test_pwm_configs;
    pwm_runtime.pwm_states = test_pwm_states;
    memset(test_pwm_configs, 0, sizeof(test_pwm_configs));
    memset(test_pwm_states, 0, sizeof(test_pwm_states));
    for (int i = 0; i < 128; i++) {
        test_pwm_configs[i].logical_pin = i;
        test_pwm_configs[i].channel = 0xFF;
        test_pwm_configs[i].frequency = 1000;
        test_pwm_configs[i].resolution = 10;
    }
#endif
    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "PWM subsystem initialized");
    return MESHX_SUCCESS;
}

/**
 * @brief Deinitialize PWM subsystem
 *
 * This function deinitializes the PWM subsystem and releases all resources.
 * It stops all PWM outputs, deconfigures channels, and cleans up platform resources.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_pwm_deinit(void)
{
    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "Deinitializing PWM subsystem");

    if (!pwm_runtime.initialized) {
        MESHX_LOGW(MODULE_ID_COMPONENT_MESHX_GPIO, "PWM not initialized");
        return MESHX_SUCCESS;
    }

    // Stop all active PWM outputs
    for (uint8_t i = 0; i < pwm_runtime.pwm_pin_count; i++) {
        if (pwm_runtime.pwm_states != NULL && pwm_runtime.pwm_states[i].started) {
            meshx_pwm_stop(i);
        }
    }

    // Deinitialize platform-specific PWM
    meshx_err_t err = meshx_pwm_platform_deinit();
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Platform PWM deinit failed: %d", err);
        return err;
    }

    // Free allocated resources
    // Note: In actual implementation, these would be freed if dynamically allocated
    // For now, just reset pointers since they're statically allocated or NULL
    pwm_runtime.pwm_configs = NULL;
    pwm_runtime.pwm_states = NULL;
    memset(pwm_runtime.channel_allocation, 0xFF, sizeof(pwm_runtime.channel_allocation));
    pwm_runtime.allocated_channels = 0;

    pwm_runtime.initialized = false;
    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "PWM subsystem deinitialized");
    return MESHX_SUCCESS;
}

/**
 * @brief Validate logical pin number for PWM
 *
 * @param logical_pin Logical pin number to validate
 * @return meshx_err_t MESHX_SUCCESS if valid, error code if invalid
 */
static meshx_err_t validate_pwm_logical_pin(uint8_t logical_pin)
{
    if (!pwm_runtime.initialized) {
        return MESHX_ERR_GPIO_NOT_INITIALIZED;
    }

    if (logical_pin >= pwm_runtime.pwm_pin_count) {
        return MESHX_ERR_GPIO_INVALID_PIN;
    }

    // Check if pin is configured for PWM
    if (pwm_runtime.pwm_configs != NULL &&
        pwm_runtime.pwm_configs[logical_pin].logical_pin != logical_pin) {
        return MESHX_ERR_GPIO_PWM_NOT_SUPPORTED;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Validate PWM frequency
 *
 * @param frequency Frequency in Hz
 * @return meshx_err_t MESHX_SUCCESS if valid, error code if invalid
 */
static meshx_err_t validate_pwm_frequency(uint32_t frequency)
{
    if (frequency == 0) {
        return MESHX_ERR_GPIO_PWM_INVALID_PARAM;
    }

    // TODO: Check against hardware limits from BSP constraints
    // For now, just check basic validity
    if (frequency > 40000000) {  // 40MHz theoretical max
        return MESHX_ERR_GPIO_PWM_INVALID_PARAM;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Validate PWM duty cycle
 *
 * @param duty_cycle Duty cycle (0-100%)
 * @return meshx_err_t MESHX_SUCCESS if valid, error code if invalid
 */
static meshx_err_t validate_pwm_duty_cycle(uint8_t duty_cycle)
{
    if (duty_cycle > 100) {
        return MESHX_ERR_GPIO_PWM_INVALID_PARAM;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Validate PWM resolution
 *
 * @param resolution Resolution in bits
 * @return meshx_err_t MESHX_SUCCESS if valid, error code if invalid
 */
static meshx_err_t validate_pwm_resolution(uint8_t resolution)
{
    if (resolution == 0 || resolution > 20) {  // ESP32 supports up to 20-bit
        return MESHX_ERR_GPIO_PWM_INVALID_PARAM;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Allocate PWM channel
 *
 * @param logical_pin Logical pin number
 * @param[out] channel Allocated channel number
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
static meshx_err_t allocate_pwm_channel(uint8_t logical_pin, uint8_t* channel)
{
    if (channel == NULL) {
        return MESHX_INVALID_ARG;
    }

    // Check if channel is already allocated for this pin
    if (pwm_runtime.pwm_configs != NULL) {
        uint8_t configured_channel = pwm_runtime.pwm_configs[logical_pin].channel;
        if (configured_channel != 0xFF) {  // 0xFF means not configured
            // Check if channel is already in use
            for (uint8_t i = 0; i < pwm_runtime.allocated_channels; i++) {
                if (pwm_runtime.channel_allocation[i] == configured_channel) {
                    // Channel already allocated to another pin
                    return MESHX_ERR_GPIO_PWM_NOT_SUPPORTED;
                }
            }

            *channel = configured_channel;
            pwm_runtime.channel_allocation[pwm_runtime.allocated_channels++] = configured_channel;
            return MESHX_SUCCESS;
        }
    }

    // Find free channel (simple linear search)
    // TODO: Implement better channel allocation strategy
    for (uint8_t ch = 0; ch < sizeof(pwm_runtime.channel_allocation); ch++) {
        bool channel_in_use = false;
        for (uint8_t i = 0; i < pwm_runtime.allocated_channels; i++) {
            if (pwm_runtime.channel_allocation[i] == ch) {
                channel_in_use = true;
                break;
            }
        }

        if (!channel_in_use) {
            *channel = ch;
            pwm_runtime.channel_allocation[pwm_runtime.allocated_channels++] = ch;

            // Update configuration if available
            if (pwm_runtime.pwm_configs != NULL) {
                pwm_runtime.pwm_configs[logical_pin].channel = ch;
            }

            return MESHX_SUCCESS;
        }
    }

    return MESHX_ERR_GPIO_PWM_NOT_SUPPORTED;  // No channels available
}

/**
 * @brief Start PWM output on pin
 *
 * @param logical_pin Logical pin number (0-255)
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_pwm_start(uint8_t logical_pin)
{
    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "Starting PWM on pin %u", logical_pin);

    // Validate pin
    meshx_err_t err = validate_pwm_logical_pin(logical_pin);
    if (err != MESHX_SUCCESS) {
        return err;
    }

    // Check if already started
    if (pwm_runtime.pwm_states != NULL && pwm_runtime.pwm_states[logical_pin].started) {
        MESHX_LOGW(MODULE_ID_COMPONENT_MESHX_GPIO, "PWM already started on pin %u", logical_pin);
        return MESHX_SUCCESS;
    }

    // Get configuration
    uint32_t frequency = 1000;  // Default 1kHz
    uint8_t duty_cycle = 50;    // Default 50%
    uint8_t resolution = 10;    // Default 10-bit

    if (pwm_runtime.pwm_configs != NULL) {
        frequency = pwm_runtime.pwm_configs[logical_pin].frequency;
        duty_cycle = pwm_runtime.pwm_configs[logical_pin].duty_cycle;
        resolution = pwm_runtime.pwm_configs[logical_pin].resolution;
    }

    // Validate parameters
    err = validate_pwm_frequency(frequency);
    if (err != MESHX_SUCCESS) return err;

    err = validate_pwm_duty_cycle(duty_cycle);
    if (err != MESHX_SUCCESS) return err;

    err = validate_pwm_resolution(resolution);
    if (err != MESHX_SUCCESS) return err;

    // Allocate PWM channel
    uint8_t channel;
    err = allocate_pwm_channel(logical_pin, &channel);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO,
                   "Failed to allocate PWM channel for pin %u: %d", logical_pin, err);
        return err;
    }

    // TODO: Get physical pin from logical pin mapping
    // For now, use logical pin as physical pin (simplified)
    uint8_t physical_pin = logical_pin;

    // Start platform-specific PWM
    err = meshx_pwm_platform_start(physical_pin, frequency, duty_cycle, resolution);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO,
                   "Platform PWM start failed for pin %u: %d", logical_pin, err);
        return err;
    }

    // Update runtime state
    if (pwm_runtime.pwm_states != NULL) {
        pwm_runtime.pwm_states[logical_pin].started = true;
        pwm_runtime.pwm_states[logical_pin].frequency = frequency;
        pwm_runtime.pwm_states[logical_pin].duty_cycle = duty_cycle;
        pwm_runtime.pwm_states[logical_pin].resolution = resolution;
    }

    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO,
               "PWM started on pin %u: %u Hz, %u%%, %u-bit",
               logical_pin, frequency, duty_cycle, resolution);
    return MESHX_SUCCESS;
}

/**
 * @brief Stop PWM output on pin
 *
 * @param logical_pin Logical pin number (0-255)
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_pwm_stop(uint8_t logical_pin)
{
    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "Stopping PWM on pin %u", logical_pin);

    // Validate pin
    meshx_err_t err = validate_pwm_logical_pin(logical_pin);
    if (err != MESHX_SUCCESS) {
        return err;
    }

    // Check if already stopped
    if (pwm_runtime.pwm_states == NULL || !pwm_runtime.pwm_states[logical_pin].started) {
        MESHX_LOGW(MODULE_ID_COMPONENT_MESHX_GPIO, "PWM already stopped on pin %u", logical_pin);
        return MESHX_SUCCESS;
    }

    // TODO: Get physical pin from logical pin mapping
    // For now, use logical pin as physical pin (simplified)
    uint8_t physical_pin = logical_pin;

    // Stop platform-specific PWM
    err = meshx_pwm_platform_stop(physical_pin);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO,
                   "Platform PWM stop failed for pin %u: %d", logical_pin, err);
        return err;
    }

    // Free allocated channel
    if (pwm_runtime.pwm_configs != NULL) {
        uint8_t channel = pwm_runtime.pwm_configs[logical_pin].channel;
        if (channel != 0xFF) {
            // Remove from allocation tracking
            for (uint8_t i = 0; i < pwm_runtime.allocated_channels; i++) {
                if (pwm_runtime.channel_allocation[i] == channel) {
                    // Shift remaining channels
                    for (uint8_t j = i; j < pwm_runtime.allocated_channels - 1; j++) {
                        pwm_runtime.channel_allocation[j] = pwm_runtime.channel_allocation[j + 1];
                    }
                    pwm_runtime.allocated_channels--;
                    break;
                }
            }
        }
    }

    // Update runtime state
    pwm_runtime.pwm_states[logical_pin].started = false;

    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "PWM stopped on pin %u", logical_pin);
    return MESHX_SUCCESS;
}

/**
 * @brief Set PWM duty cycle
 *
 * @param logical_pin Logical pin number (0-255)
 * @param duty_cycle Duty cycle (0-100%)
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_pwm_set_duty_cycle(uint8_t logical_pin, uint8_t duty_cycle)
{
    MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_GPIO,
               "Setting PWM duty cycle on pin %u to %u%%", logical_pin, duty_cycle);

    // Validate pin
    meshx_err_t err = validate_pwm_logical_pin(logical_pin);
    if (err != MESHX_SUCCESS) {
        return err;
    }

    // Validate duty cycle
    err = validate_pwm_duty_cycle(duty_cycle);
    if (err != MESHX_SUCCESS) {
        return err;
    }

    // Check if PWM is started
    if (pwm_runtime.pwm_states == NULL || !pwm_runtime.pwm_states[logical_pin].started) {
        return MESHX_ERR_GPIO_PWM_NOT_SUPPORTED;
    }

    // TODO: Get physical pin from logical pin mapping
    // For now, use logical pin as physical pin (simplified)
    uint8_t physical_pin = logical_pin;

    // Set platform-specific PWM duty cycle
    err = meshx_pwm_platform_set_duty_cycle(physical_pin, duty_cycle);
    if (err != MESHX_SUCCESS) {
        return err;
    }

    // Update runtime state
    pwm_runtime.pwm_states[logical_pin].duty_cycle = duty_cycle;

    MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_GPIO,
               "PWM duty cycle set on pin %u to %u%%", logical_pin, duty_cycle);
    return MESHX_SUCCESS;
}

/**
 * @brief Get PWM duty cycle
 *
 * @param logical_pin Logical pin number (0-255)
 * @param[out] duty_cycle Pointer to store duty cycle
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_pwm_get_duty_cycle(uint8_t logical_pin, uint8_t *duty_cycle)
{
    MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_GPIO, "Getting PWM duty cycle from pin %u", logical_pin);

    if (duty_cycle == NULL) {
        return MESHX_INVALID_ARG;
    }

    // Validate pin
    meshx_err_t err = validate_pwm_logical_pin(logical_pin);
    if (err != MESHX_SUCCESS) {
        return err;
    }

    // Check if PWM is started
    if (pwm_runtime.pwm_states == NULL || !pwm_runtime.pwm_states[logical_pin].started) {
        return MESHX_ERR_GPIO_PWM_NOT_SUPPORTED;
    }

    // TODO: Get from platform-specific implementation
    // For now, return from runtime state
    *duty_cycle = pwm_runtime.pwm_states[logical_pin].duty_cycle;

    MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_GPIO,
               "PWM duty cycle on pin %u is %u%%", logical_pin, *duty_cycle);
    return MESHX_SUCCESS;
}

/**
 * @brief Set PWM frequency
 *
 * @param logical_pin Logical pin number (0-255)
 * @param frequency Frequency in Hz
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_pwm_set_frequency(uint8_t logical_pin, uint32_t frequency)
{
    MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_GPIO,
               "Setting PWM frequency on pin %u to %u Hz", logical_pin, frequency);

    // Validate pin
    meshx_err_t err = validate_pwm_logical_pin(logical_pin);
    if (err != MESHX_SUCCESS) {
        return err;
    }

    // Validate frequency
    err = validate_pwm_frequency(frequency);
    if (err != MESHX_SUCCESS) {
        return err;
    }

    // Check if PWM is started
    if (pwm_runtime.pwm_states == NULL || !pwm_runtime.pwm_states[logical_pin].started) {
        return MESHX_ERR_GPIO_PWM_NOT_SUPPORTED;
    }

    // TODO: Get physical pin from logical pin mapping
    // For now, use logical pin as physical pin (simplified)
    uint8_t physical_pin = logical_pin;

    // Set platform-specific PWM frequency
    err = meshx_pwm_platform_set_frequency(physical_pin, frequency);
    if (err != MESHX_SUCCESS) {
        return err;
    }

    // Update runtime state
    pwm_runtime.pwm_states[logical_pin].frequency = frequency;

    MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_GPIO,
               "PWM frequency set on pin %u to %u Hz", logical_pin, frequency);
    return MESHX_SUCCESS;
}

/**
 * @brief Get PWM frequency
 *
 * @param logical_pin Logical pin number (0-255)
 * @param[out] frequency Pointer to store frequency
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_pwm_get_frequency(uint8_t logical_pin, uint32_t *frequency)
{
    MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_GPIO, "Getting PWM frequency from pin %u", logical_pin);

    if (frequency == NULL) {
        return MESHX_INVALID_ARG;
    }

    // Validate pin
    meshx_err_t err = validate_pwm_logical_pin(logical_pin);
    if (err != MESHX_SUCCESS) {
        return err;
    }

    // Check if PWM is started
    if (pwm_runtime.pwm_states == NULL || !pwm_runtime.pwm_states[logical_pin].started) {
        return MESHX_ERR_GPIO_PWM_NOT_SUPPORTED;
    }

    // TODO: Get from platform-specific implementation
    // For now, return from runtime state
    *frequency = pwm_runtime.pwm_states[logical_pin].frequency;

    MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_GPIO,
               "PWM frequency on pin %u is %u Hz", logical_pin, *frequency);
    return MESHX_SUCCESS;
}

/**
 * @brief Set PWM resolution
 *
 * @param logical_pin Logical pin number (0-255)
 * @param resolution Resolution in bits (typically 8-16)
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_pwm_set_resolution(uint8_t logical_pin, uint8_t resolution)
{
    MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_GPIO,
               "Setting PWM resolution on pin %u to %u bits", logical_pin, resolution);

    // Validate pin
    meshx_err_t err = validate_pwm_logical_pin(logical_pin);
    if (err != MESHX_SUCCESS) {
        return err;
    }

    // Validate resolution
    err = validate_pwm_resolution(resolution);
    if (err != MESHX_SUCCESS) {
        return err;
    }

    // Check if PWM is started
    if (pwm_runtime.pwm_states == NULL || !pwm_runtime.pwm_states[logical_pin].started) {
        return MESHX_ERR_GPIO_PWM_NOT_SUPPORTED;
    }

    // Update runtime state
    pwm_runtime.pwm_states[logical_pin].resolution = resolution;

    MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_GPIO,
               "PWM resolution set on pin %u to %u bits", logical_pin, resolution);
    return MESHX_SUCCESS;
}

/**
 * @brief Get PWM resolution
 *
 * @param logical_pin Logical pin number (0-255)
 * @param[out] resolution Pointer to store resolution
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_pwm_get_resolution(uint8_t logical_pin, uint8_t *resolution)
{
    MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_GPIO, "Getting PWM resolution from pin %u", logical_pin);

    if (resolution == NULL) {
        return MESHX_INVALID_ARG;
    }

    // Validate pin
    meshx_err_t err = validate_pwm_logical_pin(logical_pin);
    if (err != MESHX_SUCCESS) {
        return err;
    }

    // Check if PWM is started
    if (pwm_runtime.pwm_states == NULL || !pwm_runtime.pwm_states[logical_pin].started) {
        return MESHX_ERR_GPIO_PWM_NOT_SUPPORTED;
    }

    // TODO: Get from platform-specific implementation
    // For now, return from runtime state
    *resolution = pwm_runtime.pwm_states[logical_pin].resolution;

    MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_GPIO,
               "PWM resolution on pin %u is %u bits", logical_pin, *resolution);
    return MESHX_SUCCESS;
}

/**
 * @brief Check if PWM subsystem is initialized
 *
 * @return true if initialized
 * @return false if not initialized
 */
bool meshx_pwm_is_initialized(void)
{
    return pwm_runtime.initialized;
}

/**
 * @brief Get number of configured PWM pins
 *
 * @return uint8_t Number of configured PWM pins
 */
uint8_t meshx_pwm_get_pin_count(void)
{
    return pwm_runtime.pwm_pin_count;
}

/**
 * @brief Get number of allocated PWM channels
 *
 * @return uint8_t Number of allocated channels
 */
uint8_t meshx_pwm_get_allocated_channels(void)
{
    return pwm_runtime.allocated_channels;
}

/**
 * @brief Check if PWM is started on pin
 *
 * @param logical_pin Logical pin number
 * @return true if PWM is started
 * @return false if PWM is not started
 */
bool meshx_pwm_is_started(uint8_t logical_pin)
{
    if (!pwm_runtime.initialized || logical_pin >= pwm_runtime.pwm_pin_count) {
        return false;
    }

    if (pwm_runtime.pwm_states == NULL) {
        return false;
    }

    return pwm_runtime.pwm_states[logical_pin].started;
}

#if CONFIG_ENABLE_UNIT_TEST
void meshx_pwm_test_set_pin_count(uint8_t count)
{
    if (count > 128) count = 128;
    pwm_runtime.pwm_pin_count = count;
}
#endif
