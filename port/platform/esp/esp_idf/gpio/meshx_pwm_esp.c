/**
 * @file meshx_pwm_esp.c
 * @brief ESP-IDF PWM Platform Implementation
 *
 * This file implements the MeshX PWM platform interface using ESP-IDF LEDC API.
 * It provides PWM operations for ESP32 platforms with support for multiple channels
 * and timers.
 *
 * @author MeshX Team
 * @date 2024
 */

#include "interface/gpio/meshx_gpio_platform.h"
#include "interface/logging/meshx_log.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include <string.h>

#define MODULE_ID_PLATFORM_PWM MODULE_ID_COMMON

// Maximum number of PWM channels
#define ESP_PWM_MAX_CHANNELS LEDC_CHANNEL_MAX
#define ESP_PWM_MAX_TIMERS LEDC_TIMER_MAX

// Structure to track PWM timer state
typedef struct {
    bool allocated;
    uint32_t frequency;
    uint8_t resolution;
    ledc_timer_bit_t timer_bit;
    uint8_t ref_count; // Number of channels using this timer
} esp_pwm_timer_state_t;

// Structure to track PWM channel state
typedef struct {
    bool allocated;
    uint8_t physical_pin;
    ledc_channel_t channel;
    ledc_timer_t timer;
    uint32_t frequency;
    uint8_t duty_cycle;
    uint8_t resolution;
} esp_pwm_channel_state_t;

// Static state for PWM timers
static esp_pwm_timer_state_t esp_pwm_timers[ESP_PWM_MAX_TIMERS];

// Static state for PWM channels
static esp_pwm_channel_state_t esp_pwm_channels[ESP_PWM_MAX_CHANNELS];

// Static flag to track initialization
static bool pwm_initialized = false;

/**
 * @brief Convert resolution bits to LEDC timer bit
 *
 * Note: ESP32-C3 only supports up to 14-bit resolution
 */
static ledc_timer_bit_t resolution_to_timer_bit(uint8_t resolution)
{
    switch (resolution) {
        case 1: return LEDC_TIMER_1_BIT;
        case 2: return LEDC_TIMER_2_BIT;
        case 3: return LEDC_TIMER_3_BIT;
        case 4: return LEDC_TIMER_4_BIT;
        case 5: return LEDC_TIMER_5_BIT;
        case 6: return LEDC_TIMER_6_BIT;
        case 7: return LEDC_TIMER_7_BIT;
        case 8: return LEDC_TIMER_8_BIT;
        case 9: return LEDC_TIMER_9_BIT;
        case 10: return LEDC_TIMER_10_BIT;
        case 11: return LEDC_TIMER_11_BIT;
        case 12: return LEDC_TIMER_12_BIT;
        case 13: return LEDC_TIMER_13_BIT;
        case 14: return LEDC_TIMER_14_BIT;
        /* ESP32-C3 only supports up to 14-bit resolution */
        case 15:
        case 16:
            return LEDC_TIMER_14_BIT;  // Clamp to 14-bit max for ESP32-C3
        default: return LEDC_TIMER_13_BIT; // Default to 13-bit
    }
}

/**
 * @brief Find or allocate PWM timer
 */
static ledc_timer_t find_or_allocate_pwm_timer(uint32_t frequency, uint8_t resolution)
{
    // First, try to find existing timer with same frequency and resolution
    for (ledc_timer_t timer = 0; timer < ESP_PWM_MAX_TIMERS; timer++) {
        if (esp_pwm_timers[timer].allocated &&
            esp_pwm_timers[timer].frequency == frequency &&
            esp_pwm_timers[timer].resolution == resolution) {
            esp_pwm_timers[timer].ref_count++;
            return timer;
        }
    }

    // Find free timer
    for (ledc_timer_t timer = 0; timer < ESP_PWM_MAX_TIMERS; timer++) {
        if (!esp_pwm_timers[timer].allocated) {
            // Configure timer
            ledc_timer_config_t timer_conf = {
                .speed_mode = LEDC_LOW_SPEED_MODE,
                .duty_resolution = resolution_to_timer_bit(resolution),
                .timer_num = timer,
                .freq_hz = frequency,
                .clk_cfg = LEDC_AUTO_CLK
            };

            esp_err_t err = ledc_timer_config(&timer_conf);
            if (err != ESP_OK) {
                MESHX_LOGE(MODULE_ID_PLATFORM_PWM, "Failed to configure PWM timer %d: %d", timer, err);
                continue; // Try next timer
            }

            // Store timer state
            esp_pwm_timers[timer].allocated = true;
            esp_pwm_timers[timer].frequency = frequency;
            esp_pwm_timers[timer].resolution = resolution;
            esp_pwm_timers[timer].timer_bit = resolution_to_timer_bit(resolution);
            esp_pwm_timers[timer].ref_count = 1;

            MESHX_LOGD(MODULE_ID_PLATFORM_PWM, "Allocated PWM timer %d: freq=%d Hz, res=%d bits",
                       timer, frequency, resolution);
            return timer;
        }
    }

    return LEDC_TIMER_MAX; // No free timer
}

/**
 * @brief Find free PWM channel
 */
static ledc_channel_t find_free_pwm_channel(void)
{
    for (ledc_channel_t channel = 0; channel < ESP_PWM_MAX_CHANNELS; channel++) {
        if (!esp_pwm_channels[channel].allocated) {
            return channel;
        }
    }
    return LEDC_CHANNEL_MAX; // No free channel
}

/**
 * @brief Release PWM timer
 */
static void release_pwm_timer(ledc_timer_t timer)
{
    if (timer >= ESP_PWM_MAX_TIMERS || !esp_pwm_timers[timer].allocated) {
        return;
    }

    esp_pwm_timers[timer].ref_count--;
    if (esp_pwm_timers[timer].ref_count == 0) {
        // No more channels using this timer, can deallocate
        esp_pwm_timers[timer].allocated = false;
        MESHX_LOGD(MODULE_ID_PLATFORM_PWM, "Released PWM timer %d", timer);
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
 * @brief Platform-specific PWM initialization
 */
meshx_err_t meshx_pwm_platform_init(void)
{
    if (pwm_initialized) {
        MESHX_LOGW(MODULE_ID_PLATFORM_PWM, "PWM already initialized");
        return MESHX_SUCCESS;
    }

    // Initialize PWM timer states
    memset(esp_pwm_timers, 0, sizeof(esp_pwm_timers));

    // Initialize PWM channel states
    memset(esp_pwm_channels, 0, sizeof(esp_pwm_channels));

    pwm_initialized = true;
    MESHX_LOGD(MODULE_ID_PLATFORM_PWM, "PWM platform initialized");
    return MESHX_SUCCESS;
}

/**
 * @brief Platform-specific PWM deinitialization
 */
meshx_err_t meshx_pwm_platform_deinit(void)
{
    if (!pwm_initialized) {
        return MESHX_SUCCESS;
    }

    // Stop all PWM channels
    for (int i = 0; i < ESP_PWM_MAX_CHANNELS; i++) {
        if (esp_pwm_channels[i].allocated) {
            ledc_stop(LEDC_LOW_SPEED_MODE, esp_pwm_channels[i].channel, 0);
            release_pwm_timer(esp_pwm_channels[i].timer);
            esp_pwm_channels[i].allocated = false;
        }
    }

    pwm_initialized = false;
    MESHX_LOGD(MODULE_ID_PLATFORM_PWM, "PWM platform deinitialized");
    return MESHX_SUCCESS;
}

/**
 * @brief Platform-specific PWM start
 */
meshx_err_t meshx_pwm_platform_start(uint8_t physical_pin,
                                     uint32_t frequency,
                                     uint8_t duty_cycle,
                                     uint8_t resolution)
{
    if (!pwm_initialized) {
        return MESHX_ERR_GPIO_NOT_INITIALIZED;
    }

    if (duty_cycle > 100) {
        return MESHX_ERR_GPIO_PWM_INVALID_PARAM;
    }

    if (resolution < 1 || resolution > 16) {
        return MESHX_ERR_GPIO_PWM_INVALID_PARAM;
    }

    // Check if pin already has PWM
    for (int i = 0; i < ESP_PWM_MAX_CHANNELS; i++) {
        if (esp_pwm_channels[i].allocated && esp_pwm_channels[i].physical_pin == physical_pin) {
            MESHX_LOGW(MODULE_ID_PLATFORM_PWM, "PWM already started on pin %d", physical_pin);
            return MESHX_SUCCESS; // Already started
        }
    }

    // Find free PWM channel
    ledc_channel_t channel = find_free_pwm_channel();
    if (channel == LEDC_CHANNEL_MAX) {
        MESHX_LOGE(MODULE_ID_PLATFORM_PWM, "No free PWM channels available");
        return MESHX_ERR_GPIO_PWM_NOT_SUPPORTED;
    }

    // Find or allocate timer
    ledc_timer_t timer = find_or_allocate_pwm_timer(frequency, resolution);
    if (timer == LEDC_TIMER_MAX) {
        MESHX_LOGE(MODULE_ID_PLATFORM_PWM, "No free PWM timers available");
        return MESHX_ERR_GPIO_PWM_NOT_SUPPORTED;
    }

    // Configure channel
    ledc_channel_config_t channel_conf = {
        .gpio_num = physical_pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = timer,
        .duty = 0, // Will be set below
        .hpoint = 0
    };

    esp_err_t err = ledc_channel_config(&channel_conf);
    if (err != ESP_OK) {
        MESHX_LOGE(MODULE_ID_PLATFORM_PWM, "Failed to configure PWM channel %d: %d", channel, err);
        release_pwm_timer(timer);
        return esp_err_to_meshx_err(err);
    }

    // Calculate and set duty cycle
    uint32_t max_duty = (1 << resolution) - 1;
    uint32_t duty = (duty_cycle * max_duty) / 100;

    err = ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, duty);
    if (err != ESP_OK) {
        MESHX_LOGE(MODULE_ID_PLATFORM_PWM, "Failed to set PWM duty for channel %d: %d", channel, err);
        ledc_stop(LEDC_LOW_SPEED_MODE, channel, 0);
        release_pwm_timer(timer);
        return esp_err_to_meshx_err(err);
    }

    err = ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);
    if (err != ESP_OK) {
        MESHX_LOGE(MODULE_ID_PLATFORM_PWM, "Failed to update PWM duty for channel %d: %d", channel, err);
        ledc_stop(LEDC_LOW_SPEED_MODE, channel, 0);
        release_pwm_timer(timer);
        return esp_err_to_meshx_err(err);
    }

    // Store channel state
    esp_pwm_channels[channel].allocated = true;
    esp_pwm_channels[channel].physical_pin = physical_pin;
    esp_pwm_channels[channel].channel = channel;
    esp_pwm_channels[channel].timer = timer;
    esp_pwm_channels[channel].frequency = frequency;
    esp_pwm_channels[channel].duty_cycle = duty_cycle;
    esp_pwm_channels[channel].resolution = resolution;

    MESHX_LOGD(MODULE_ID_PLATFORM_PWM, "Started PWM on pin %d, channel %d, timer %d, freq %d Hz, duty %d%%, res %d bits",
               physical_pin, channel, timer, frequency, duty_cycle, resolution);
    return MESHX_SUCCESS;
}

/**
 * @brief Platform-specific PWM stop
 */
meshx_err_t meshx_pwm_platform_stop(uint8_t physical_pin)
{
    if (!pwm_initialized) {
        return MESHX_ERR_GPIO_NOT_INITIALIZED;
    }

    // Find channel by physical pin
    ledc_channel_t channel = LEDC_CHANNEL_MAX;
    for (int i = 0; i < ESP_PWM_MAX_CHANNELS; i++) {
        if (esp_pwm_channels[i].allocated && esp_pwm_channels[i].physical_pin == physical_pin) {
            channel = esp_pwm_channels[i].channel;
            break;
        }
    }

    if (channel == LEDC_CHANNEL_MAX) {
        MESHX_LOGW(MODULE_ID_PLATFORM_PWM, "No PWM found on pin %d", physical_pin);
        return MESHX_ERR_GPIO_PWM_NOT_SUPPORTED;
    }

    // Stop PWM
    esp_err_t err = ledc_stop(LEDC_LOW_SPEED_MODE, channel, 0);
    if (err != ESP_OK) {
        MESHX_LOGE(MODULE_ID_PLATFORM_PWM, "Failed to stop PWM on pin %d: %d", physical_pin, err);
        return esp_err_to_meshx_err(err);
    }

    // Release timer and free channel
    release_pwm_timer(esp_pwm_channels[channel].timer);
    esp_pwm_channels[channel].allocated = false;

    MESHX_LOGD(MODULE_ID_PLATFORM_PWM, "Stopped PWM on pin %d", physical_pin);
    return MESHX_SUCCESS;
}

/**
 * @brief Platform-specific PWM duty cycle set
 */
meshx_err_t meshx_pwm_platform_set_duty_cycle(uint8_t physical_pin, uint8_t duty_cycle)
{
    if (!pwm_initialized) {
        return MESHX_ERR_GPIO_NOT_INITIALIZED;
    }

    if (duty_cycle > 100) {
        return MESHX_ERR_GPIO_PWM_INVALID_PARAM;
    }

    // Find channel by physical pin
    ledc_channel_t channel = LEDC_CHANNEL_MAX;
    for (int i = 0; i < ESP_PWM_MAX_CHANNELS; i++) {
        if (esp_pwm_channels[i].allocated && esp_pwm_channels[i].physical_pin == physical_pin) {
            channel = esp_pwm_channels[i].channel;
            break;
        }
    }

    if (channel == LEDC_CHANNEL_MAX) {
        return MESHX_ERR_GPIO_PWM_NOT_SUPPORTED;
    }

    // Calculate duty value
    uint32_t max_duty = (1 << esp_pwm_channels[channel].resolution) - 1;
    uint32_t duty = (duty_cycle * max_duty) / 100;

    // Set duty cycle
    esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, duty);
    if (err != ESP_OK) {
        MESHX_LOGE(MODULE_ID_PLATFORM_PWM, "Failed to set PWM duty on pin %d: %d", physical_pin, err);
        return esp_err_to_meshx_err(err);
    }

    err = ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);
    if (err != ESP_OK) {
        MESHX_LOGE(MODULE_ID_PLATFORM_PWM, "Failed to update PWM duty on pin %d: %d", physical_pin, err);
        return esp_err_to_meshx_err(err);
    }

    // Update state
    esp_pwm_channels[channel].duty_cycle = duty_cycle;

    MESHX_LOGD(MODULE_ID_PLATFORM_PWM, "Set PWM duty on pin %d to %d%%", physical_pin, duty_cycle);
    return MESHX_SUCCESS;
}

/**
 * @brief Platform-specific PWM frequency set
 */
meshx_err_t meshx_pwm_platform_set_frequency(uint8_t physical_pin, uint32_t frequency)
{
    if (!pwm_initialized) {
        return MESHX_ERR_GPIO_NOT_INITIALIZED;
    }

    // Find channel by physical pin
    ledc_channel_t channel = LEDC_CHANNEL_MAX;
    for (int i = 0; i < ESP_PWM_MAX_CHANNELS; i++) {
        if (esp_pwm_channels[i].allocated && esp_pwm_channels[i].physical_pin == physical_pin) {
            channel = esp_pwm_channels[i].channel;
            break;
        }
    }

    if (channel == LEDC_CHANNEL_MAX) {
        return MESHX_ERR_GPIO_PWM_NOT_SUPPORTED;
    }

    // Get current timer and resolution
    ledc_timer_t timer = esp_pwm_channels[channel].timer;
    uint8_t resolution = esp_pwm_channels[channel].resolution;

    // Check if timer is shared with other channels
    if (esp_pwm_timers[timer].ref_count > 1) {
        // Timer is shared, need to allocate new timer
        ledc_timer_t new_timer = find_or_allocate_pwm_timer(frequency, resolution);
        if (new_timer == LEDC_TIMER_MAX) {
            MESHX_LOGE(MODULE_ID_PLATFORM_PWM, "No free PWM timers available for frequency change");
            return MESHX_ERR_GPIO_PWM_NOT_SUPPORTED;
        }

        // Reconfigure channel with new timer
        ledc_channel_config_t channel_conf = {
            .gpio_num = physical_pin,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = channel,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = new_timer,
            .duty = 0, // Will be recalculated
            .hpoint = 0
        };

        esp_err_t err = ledc_channel_config(&channel_conf);
        if (err != ESP_OK) {
            MESHX_LOGE(MODULE_ID_PLATFORM_PWM, "Failed to reconfigure PWM channel %d: %d", channel, err);
            release_pwm_timer(new_timer);
            return esp_err_to_meshx_err(err);
        }

        // Release old timer and update state
        release_pwm_timer(timer);
        esp_pwm_channels[channel].timer = new_timer;
        timer = new_timer;
    } else {
        // Timer is not shared, can reconfigure it
        ledc_timer_config_t timer_conf = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .duty_resolution = resolution_to_timer_bit(resolution),
            .timer_num = timer,
            .freq_hz = frequency,
            .clk_cfg = LEDC_AUTO_CLK
        };

        esp_err_t err = ledc_timer_config(&timer_conf);
        if (err != ESP_OK) {
            MESHX_LOGE(MODULE_ID_PLATFORM_PWM, "Failed to reconfigure PWM timer %d: %d", timer, err);
            return esp_err_to_meshx_err(err);
        }
    }

    // Recalculate and set duty cycle for new frequency
    uint32_t max_duty = (1 << resolution) - 1;
    uint32_t duty = (esp_pwm_channels[channel].duty_cycle * max_duty) / 100;

    esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, duty);
    if (err != ESP_OK) {
        MESHX_LOGE(MODULE_ID_PLATFORM_PWM, "Failed to set PWM duty after frequency change: %d", err);
        return esp_err_to_meshx_err(err);
    }

    err = ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);
    if (err != ESP_OK) {
        MESHX_LOGE(MODULE_ID_PLATFORM_PWM, "Failed to update PWM duty after frequency change: %d", err);
        return esp_err_to_meshx_err(err);
    }

    // Update states
    esp_pwm_timers[timer].frequency = frequency;
    esp_pwm_channels[channel].frequency = frequency;

    MESHX_LOGD(MODULE_ID_PLATFORM_PWM, "Set PWM frequency on pin %d to %d Hz", physical_pin, frequency);
    return MESHX_SUCCESS;
}
