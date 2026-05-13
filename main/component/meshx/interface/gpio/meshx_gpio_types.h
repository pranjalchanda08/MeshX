/**
 * @file meshx_gpio_types.h
 * @brief MeshX GPIO Type Definitions
 *
 * This file defines data structures and types for GPIO configuration and state.
 *
 * @author MeshX Team
 * @date 2024
 */

#ifndef __MESHX_GPIO_TYPES_H
#define __MESHX_GPIO_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief GPIO Interrupt Trigger Types
 *
 * Forward declaration for use in meshx_gpio_pin_state_t.
 * Full definition is in meshx_gpio.h
 */
typedef enum {
    MESHX_GPIO_INTR_DISABLED = 0,           /**< Interrupt disabled */
    MESHX_GPIO_INTR_POSITIVE_EDGE,          /**< Positive edge trigger */
    MESHX_GPIO_INTR_NEGATIVE_EDGE,          /**< Negative edge trigger */
    MESHX_GPIO_INTR_ANY_EDGE,               /**< Any edge trigger */
    MESHX_GPIO_INTR_LOW_LEVEL,              /**< Low level trigger */
    MESHX_GPIO_INTR_HIGH_LEVEL,             /**< High level trigger */
    MESHX_GPIO_INTR_MAX                     /**< Maximum interrupt type */
} meshx_gpio_intr_type_t;

/**
 * @brief GPIO Interrupt Callback Type (forward declaration)
 *
 * Full documentation is in meshx_gpio.h
 */
typedef void (*meshx_gpio_intr_cb_t)(uint8_t logical_pin, void *user_data);

/**
 * @brief GPIO Pin Configuration Structure
 *
 * This structure defines the configuration for a single GPIO pin.
 * It is used both for compile-time configuration and runtime state.
 */
typedef struct {
    uint8_t logical_pin;                    /**< Logical pin number (0-255) */
    uint8_t physical_pin;                   /**< Physical pin number (BSP-specific) */
    uint8_t mode;                           /**< Pin mode (meshx_gpio_mode_t) */
    uint8_t pull;                           /**< Pull resistor setting (meshx_gpio_pull_t) */
    uint8_t drive_strength;                 /**< Drive strength (meshx_gpio_drive_t) */
    uint8_t initial_level;                  /**< Initial output level (0 or 1) */
    bool signal_inversion;                  /**< Signal inversion (active-low) */

    /** @brief Mode-specific configuration (union based on mode) */
    union {
        /** @brief Interrupt configuration (for input pins with interrupts) */
        struct {
            uint8_t trigger;                /**< Interrupt trigger type (meshx_gpio_intr_type_t) */
            uint8_t task_priority;          /**< Interrupt task priority */
            uint16_t task_stack_size;       /**< Interrupt task stack size */
        } interrupt;

        /** @brief PWM configuration (for PWM output pins) */
        struct {
            uint32_t frequency;             /**< PWM frequency in Hz */
            uint8_t duty_cycle;             /**< Initial duty cycle (0-100%) */
            uint8_t resolution;             /**< PWM resolution in bits */
            uint8_t channel;                /**< Hardware PWM channel */
        } pwm;

        /** @brief Custom function configuration */
        struct {
            uint16_t function_id;           /**< Custom function ID */
            uint32_t args[4];               /**< Custom function arguments */
            uint8_t arg_count;              /**< Number of arguments */
        } custom;
    } mode_config;
} meshx_gpio_pin_config_t;

/**
 * @brief GPIO Pin State Structure
 *
 * This structure tracks the runtime state of a GPIO pin.
 */
typedef struct {
    uint8_t current_level;                  /**< Current pin level (0 or 1) */
    bool interrupt_registered;              /**< Whether interrupt is registered */
    meshx_gpio_intr_type_t intr_type;       /**< Interrupt type (if registered) */
    meshx_gpio_intr_cb_t intr_callback;     /**< Interrupt callback function pointer */
    void *intr_user_data;                   /**< Interrupt user data pointer */

    /** @brief Mode-specific runtime state */
    union {
        /** @brief PWM runtime state */
        struct {
            bool started;                   /**< Whether PWM is started */
            uint32_t frequency;             /**< Current PWM frequency */
            uint8_t duty_cycle;             /**< Current PWM duty cycle */
        } pwm;

        /** @brief Custom function runtime state */
        struct {
            void *custom_data;              /**< Custom function data pointer */
            size_t data_size;               /**< Custom data size */
        } custom;
    } mode_state;
} meshx_gpio_pin_state_t;

/**
 * @brief GPIO Configuration Header
 *
 * This structure is used for serialized GPIO configuration storage
 * in KV Engine with versioning and CRC validation.
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t version;                        /**< Format version (1) */
    uint8_t pin_count;                      /**< Number of configured pins */
    uint16_t crc16;                         /**< CRC16 checksum (compatible with KV Engine) */
    uint8_t reserved[3];                    /**< Reserved for future use */
} meshx_gpio_config_header_t;

/**
 * @brief Serialized GPIO Pin Configuration
 *
 * Compact representation for KV Engine storage.
 */
typedef struct {
    uint8_t logical_pin;
    uint8_t physical_pin;
    uint8_t mode;
    uint8_t pull;
    uint8_t drive_strength;
    uint8_t initial_level;
    uint8_t signal_inversion;
    uint8_t reserved;
} meshx_gpio_pin_config_serialized_t;

/**
 * @brief Serialized PWM Configuration
 */
typedef struct {
    uint32_t frequency;
    uint8_t duty_cycle;
    uint8_t resolution;
    uint8_t channel;
    uint8_t reserved[1];
} meshx_gpio_pwm_config_serialized_t;

/**
 * @brief Serialized Interrupt Configuration
 */
typedef struct {
    uint8_t trigger_type;
    uint8_t task_priority;
    uint16_t task_stack_size;
} meshx_gpio_intr_config_serialized_t;
#pragma pack(pop)

/**
 * @brief Hosted Mode State
 */
typedef enum {
    MESHX_GPIO_MODE_NON_HOSTED = 0,         /**< Non-hosted mode (direct GPIO access) */
    MESHX_GPIO_MODE_HOSTED,                 /**< Hosted mode (UART transport) */
    MESHX_GPIO_MODE_TRANSITIONING           /**< Mode transition in progress */
} meshx_gpio_hosted_mode_t;

/**
 * @brief Hosted Mode GPIO Event
 *
 * Structure for serializing GPIO events in hosted mode.
 */
typedef struct {
    uint8_t event_type;                     /**< Event type (0 = level change, 1 = interrupt) */
    uint8_t logical_pin;                    /**< Logical pin number */
    uint8_t value;                          /**< Event value (level or interrupt type) */
    uint32_t timestamp;                     /**< Event timestamp */
} meshx_gpio_hosted_event_t;

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_GPIO_TYPES_H */
