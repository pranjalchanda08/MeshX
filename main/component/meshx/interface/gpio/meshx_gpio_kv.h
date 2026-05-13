/**
 * @file meshx_gpio_kv.h
 * @brief MeshX GPIO KV Engine Serialization Format
 *
 * This file defines the binary serialization structures for GPIO configuration
 * persistence using the MeshX KV Engine. The format is designed for:
 * - Compact storage with versioning support
 * - CRC16 checksums compatible with KV Engine's CRC algorithm
 * - Product-specific key prefixes for OTA update compatibility
 * - Forward/backward compatibility for configuration migration
 *
 * @author MeshX Team
 * @date 2024
 */

#ifndef __MESHX_GPIO_KV_H
#define __MESHX_GPIO_KV_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "interface/utils/meshx_fal_interface.h"
#include "meshx_gpio_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Version Definitions
 *============================================================================*/

/** @brief Current GPIO configuration serialization format version */
#define MESHX_GPIO_CONFIG_VERSION_CURRENT    1

/** @brief Minimum supported version (for backward compatibility) */
#define MESHX_GPIO_CONFIG_VERSION_MIN        1

/** @brief Maximum supported version (for forward compatibility) */
#define MESHX_GPIO_CONFIG_VERSION_MAX        1

/*============================================================================
 * KV Engine Key Prefixes
 *============================================================================*/

/** @brief Maximum length for product name in key prefix */
#define MESHX_GPIO_KV_PRODUCT_NAME_MAX_LEN   16

/** @brief Maximum total key length (KV Engine limit is 32) */
#define MESHX_GPIO_KV_KEY_MAX_LEN            32

/** @brief Key suffix for main GPIO configuration */
#define MESHX_GPIO_KV_KEY_CONFIG             "config"

/** @brief Key suffix for GPIO pin state */
#define MESHX_GPIO_KV_KEY_STATE              "state"

/** @brief Key suffix for PWM configuration */
#define MESHX_GPIO_KV_KEY_PWM                "pwm"

/** @brief Key suffix for interrupt configuration */
#define MESHX_GPIO_KV_KEY_INTR               "intr"

/**
 * @brief Construct KV Engine key for GPIO configuration
 * @param product_name Product name (max 16 chars)
 * @param buf Buffer to store the key
 * @param buf_len Buffer length
 * @return Key length, or 0 on error
 */
static inline uint8_t meshx_gpio_kv_make_config_key(const char *product_name,
                                                     char *buf, uint8_t buf_len)
{
    if (!product_name || !buf || buf_len < MESHX_GPIO_KV_KEY_MAX_LEN) {
        return 0;
    }
    return (uint8_t)snprintf(buf, buf_len, "gpio_%.*s_%s",
                             MESHX_GPIO_KV_PRODUCT_NAME_MAX_LEN,
                             product_name,
                             MESHX_GPIO_KV_KEY_CONFIG);
}

/**
 * @brief Construct KV Engine key for GPIO pin state
 * @param product_name Product name (max 16 chars)
 * @param logical_pin Logical pin number
 * @param buf Buffer to store the key
 * @param buf_len Buffer length
 * @return Key length, or 0 on error
 */
static inline uint8_t meshx_gpio_kv_make_state_key(const char *product_name,
                                                    uint8_t logical_pin,
                                                    char *buf, uint8_t buf_len)
{
    if (!product_name || !buf || buf_len < MESHX_GPIO_KV_KEY_MAX_LEN) {
        return 0;
    }
    return (uint8_t)snprintf(buf, buf_len, "gpio_%.*s_%s_%u",
                             MESHX_GPIO_KV_PRODUCT_NAME_MAX_LEN,
                             product_name,
                             MESHX_GPIO_KV_KEY_STATE,
                             logical_pin);
}

/**
 * @brief Construct KV Engine key for PWM configuration
 * @param product_name Product name (max 16 chars)
 * @param logical_pin Logical pin number
 * @param buf Buffer to store the key
 * @param buf_len Buffer length
 * @return Key length, or 0 on error
 */
static inline uint8_t meshx_gpio_kv_make_pwm_key(const char *product_name,
                                                  uint8_t logical_pin,
                                                  char *buf, uint8_t buf_len)
{
    if (!product_name || !buf || buf_len < MESHX_GPIO_KV_KEY_MAX_LEN) {
        return 0;
    }
    return (uint8_t)snprintf(buf, buf_len, "gpio_%.*s_%s_%u",
                             MESHX_GPIO_KV_PRODUCT_NAME_MAX_LEN,
                             product_name,
                             MESHX_GPIO_KV_KEY_PWM,
                             logical_pin);
}

/**
 * @brief Construct KV Engine key for interrupt configuration
 * @param product_name Product name (max 16 chars)
 * @param logical_pin Logical pin number
 * @param buf Buffer to store the key
 * @param buf_len Buffer length
 * @return Key length, or 0 on error
 */
static inline uint8_t meshx_gpio_kv_make_intr_key(const char *product_name,
                                                   uint8_t logical_pin,
                                                   char *buf, uint8_t buf_len)
{
    if (!product_name || !buf || buf_len < MESHX_GPIO_KV_KEY_MAX_LEN) {
        return 0;
    }
    return (uint8_t)snprintf(buf, buf_len, "gpio_%.*s_%s_%u",
                             MESHX_GPIO_KV_PRODUCT_NAME_MAX_LEN,
                             product_name,
                             MESHX_GPIO_KV_KEY_INTR,
                             logical_pin);
}

/*============================================================================
 * Binary Serialization Structures
 *============================================================================*/

/**
 * @struct meshx_gpio_config_header_kv_t
 * @brief Header for serialized GPIO configuration stored in KV Engine.
 *
 * This header precedes all GPIO configuration data in KV Engine storage.
 * It includes versioning for migration support and CRC16 for integrity.
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t  version;           /**< Format version (MESHX_GPIO_CONFIG_VERSION_*) */
    uint8_t  pin_count;         /**< Number of configured pins in this config */
    uint16_t crc16;             /**< CRC16 of all data following this header */
    uint16_t total_size;        /**< Total size of serialized data (header + pins) */
    uint8_t  flags;             /**< Configuration flags (reserved for future use) */
    uint8_t  reserved[3];       /**< Reserved for future use, must be 0 */
} meshx_gpio_config_header_kv_t;

/**
 * @struct meshx_gpio_pin_config_kv_t
 * @brief Serialized GPIO pin configuration for KV Engine storage.
 *
 * Compact representation of a single GPIO pin configuration.
 * Total size: 16 bytes (aligned for efficient storage).
 */
typedef struct {
    uint8_t  logical_pin;       /**< Logical pin number (0-255) */
    uint8_t  physical_pin;      /**< Physical pin number (BSP-specific) */
    uint8_t  mode;              /**< Pin mode (meshx_gpio_mode_t) */
    uint8_t  pull;              /**< Pull resistor setting (meshx_gpio_pull_t) */
    uint8_t  drive_strength;    /**< Drive strength (meshx_gpio_drive_t) */
    uint8_t  initial_level;     /**< Initial output level (0 or 1) */
    uint8_t  signal_inversion;  /**< Signal inversion flag (0=false, 1=true) */
    uint8_t  config_type;       /**< Extended config type: 0=none, 1=PWM, 2=interrupt */
} meshx_gpio_pin_config_kv_t;

/**
 * @struct meshx_gpio_pwm_config_kv_t
 * @brief Serialized PWM configuration for KV Engine storage.
 *
 * Total size: 8 bytes.
 * Follows meshx_gpio_pin_config_kv_t when config_type == 1.
 */
typedef struct {
    uint32_t frequency;         /**< PWM frequency in Hz */
    uint8_t  duty_cycle;        /**< Duty cycle (0-100%) */
    uint8_t  resolution;        /**< PWM resolution in bits */
    uint8_t  channel;           /**< Hardware PWM channel */
    uint8_t  reserved;          /**< Reserved, must be 0 */
} meshx_gpio_pwm_config_kv_t;

/**
 * @struct meshx_gpio_intr_config_kv_t
 * @brief Serialized interrupt configuration for KV Engine storage.
 *
 * Total size: 8 bytes.
 * Follows meshx_gpio_pin_config_kv_t when config_type == 2.
 */
typedef struct {
    uint8_t  trigger_type;      /**< Interrupt trigger type (meshx_gpio_intr_type_t) */
    uint8_t  task_priority;     /**< Interrupt task priority */
    uint16_t task_stack_size;   /**< Interrupt task stack size in bytes */
    uint8_t  flags;             /**< Interrupt flags (reserved) */
    uint8_t  reserved[3];       /**< Reserved, must be 0 */
} meshx_gpio_intr_config_kv_t;

/**
 * @struct meshx_gpio_pin_state_kv_t
 * @brief Serialized GPIO pin state for KV Engine storage.
 *
 * Used for persisting runtime pin state across reboots.
 * Total size: 8 bytes.
 */
typedef struct {
    uint8_t  logical_pin;       /**< Logical pin number */
    uint8_t  current_level;     /**< Current pin level (0 or 1) */
    uint8_t  pwm_started;       /**< PWM running flag (0=false, 1=true) */
    uint8_t  current_duty;      /**< Current PWM duty cycle (if applicable) */
    uint32_t reserved;          /**< Reserved for future use */
} meshx_gpio_pin_state_kv_t;
#pragma pack(pop)

/*============================================================================
 * Size Constants
 *============================================================================*/

/** @brief Size of configuration header */
#define MESHX_GPIO_CONFIG_HEADER_SIZE    sizeof(meshx_gpio_config_header_kv_t)

/** @brief Size of basic pin configuration */
#define MESHX_GPIO_PIN_CONFIG_SIZE       sizeof(meshx_gpio_pin_config_kv_t)

/** @brief Size of PWM configuration extension */
#define MESHX_GPIO_PWM_CONFIG_SIZE       sizeof(meshx_gpio_pwm_config_kv_t)

/** @brief Size of interrupt configuration extension */
#define MESHX_GPIO_INTR_CONFIG_SIZE      sizeof(meshx_gpio_intr_config_kv_t)

/** @brief Size of pin state record */
#define MESHX_GPIO_PIN_STATE_SIZE        sizeof(meshx_gpio_pin_state_kv_t)

/** @brief Maximum size per pin (including extensions) */
#define MESHX_GPIO_PIN_MAX_SIZE          (MESHX_GPIO_PIN_CONFIG_SIZE + \
                                          MESHX_GPIO_PWM_CONFIG_SIZE)

/** @brief Calculate total config size for given pin count */
#define MESHX_GPIO_CONFIG_TOTAL_SIZE(pin_count) \
    (MESHX_GPIO_CONFIG_HEADER_SIZE + ((pin_count) * MESHX_GPIO_PIN_CONFIG_SIZE))

/*============================================================================
 * Configuration Flags
 *============================================================================*/

/** @brief Flag: Configuration has PWM extensions */
#define MESHX_GPIO_CONFIG_FLAG_HAS_PWM        (1 << 0)

/** @brief Flag: Configuration has interrupt extensions */
#define MESHX_GPIO_CONFIG_FLAG_HAS_INTR       (1 << 1)

/** @brief Flag: Configuration is migrated from older version */
#define MESHX_GPIO_CONFIG_FLAG_MIGRATED       (1 << 2)

/** @brief Flag: Configuration is in read-only mode */
#define MESHX_GPIO_CONFIG_FLAG_READ_ONLY      (1 << 3)

/*============================================================================
 * Config Type Values (for meshx_gpio_pin_config_kv_t.config_type)
 *============================================================================*/

/** @brief No extended configuration */
#define MESHX_GPIO_CONFIG_TYPE_NONE           0

/** @brief PWM configuration follows */
#define MESHX_GPIO_CONFIG_TYPE_PWM            1

/** @brief Interrupt configuration follows */
#define MESHX_GPIO_CONFIG_TYPE_INTR           2

/*============================================================================
 * CRC16 Functions
 *============================================================================*/

/**
 * @brief Calculate CRC16 compatible with KV Engine.
 *
 * Uses the same CRC-16 algorithm as meshx_kv_engine.c (Modbus CRC-16
 * with polynomial 0xA001). This ensures the GPIO subsystem can
 * independently verify data integrity.
 *
 * @param data Pointer to data buffer
 * @param len Length of data in bytes
 * @return CRC16 value
 */
static inline uint16_t meshx_gpio_kv_calc_crc16(const uint8_t *data, uint32_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

/**
 * @brief Verify CRC16 of serialized GPIO configuration.
 *
 * @param data Pointer to serialized data (including header)
 * @param len Total length of data
 * @return true if CRC is valid, false otherwise
 */
static inline bool meshx_gpio_kv_verify_crc16(const uint8_t *data, uint32_t len)
{
    if (!data || len < MESHX_GPIO_CONFIG_HEADER_SIZE) {
        return false;
    }

    const meshx_gpio_config_header_kv_t *header =
        (const meshx_gpio_config_header_kv_t *)data;

    /* CRC covers everything after the crc16 field in the header */
    const uint8_t *crc_data = data + offsetof(meshx_gpio_config_header_kv_t, total_size);
    uint32_t crc_len = len - offsetof(meshx_gpio_config_header_kv_t, total_size);

    uint16_t calculated = meshx_gpio_kv_calc_crc16(crc_data, crc_len);
    return (calculated == header->crc16);
}

/*============================================================================
 * Serialization/Deserialization Functions
 *============================================================================*/

/**
 * @brief Initialize a GPIO configuration header.
 *
 * @param header Pointer to header to initialize
 * @param pin_count Number of pins in configuration
 */
static inline void meshx_gpio_kv_init_header(meshx_gpio_config_header_kv_t *header,
                                             uint8_t pin_count)
{
    if (!header) return;

    header->version = MESHX_GPIO_CONFIG_VERSION_CURRENT;
    header->pin_count = pin_count;
    header->crc16 = 0;
    header->total_size = (uint16_t)MESHX_GPIO_CONFIG_TOTAL_SIZE(pin_count);
    header->flags = 0;
    header->reserved[0] = 0;
    header->reserved[1] = 0;
    header->reserved[2] = 0;
}

/**
 * @brief Calculate and set CRC16 for serialized configuration.
 *
 * @param data Pointer to serialized data buffer (including header)
 * @param len Total length of data
 */
static inline void meshx_gpio_kv_finalize_crc16(uint8_t *data, uint32_t len)
{
    if (!data || len < MESHX_GPIO_CONFIG_HEADER_SIZE) return;

    meshx_gpio_config_header_kv_t *header = (meshx_gpio_config_header_kv_t *)data;

    /* CRC covers everything after the crc16 field */
    const uint8_t *crc_data = data + offsetof(meshx_gpio_config_header_kv_t, total_size);
    uint32_t crc_len = len - offsetof(meshx_gpio_config_header_kv_t, total_size);

    header->crc16 = meshx_gpio_kv_calc_crc16(crc_data, crc_len);
}

/**
 * @brief Convert runtime pin config to serialized format.
 *
 * @param src Source runtime configuration
 * @param dst Destination serialized configuration
 */
static inline void meshx_gpio_kv_serialize_pin(const meshx_gpio_pin_config_t *src,
                                               meshx_gpio_pin_config_kv_t *dst)
{
    if (!src || !dst) return;

    dst->logical_pin = src->logical_pin;
    dst->physical_pin = src->physical_pin;
    dst->mode = src->mode;
    dst->pull = src->pull;
    dst->drive_strength = src->drive_strength;
    dst->initial_level = src->initial_level;
    dst->signal_inversion = src->signal_inversion ? 1 : 0;

    /* Determine config type based on mode */
    if (src->mode == 5) { /* MESHX_GPIO_MODE_PWM_OUTPUT */
        dst->config_type = MESHX_GPIO_CONFIG_TYPE_PWM;
    } else if (src->mode == 0 && src->mode_config.interrupt.trigger != 0) {
        /* Input mode with interrupt */
        dst->config_type = MESHX_GPIO_CONFIG_TYPE_INTR;
    } else {
        dst->config_type = MESHX_GPIO_CONFIG_TYPE_NONE;
    }
}

/**
 * @brief Convert serialized pin config to runtime format.
 *
 * @param src Source serialized configuration
 * @param dst Destination runtime configuration
 */
static inline void meshx_gpio_kv_deserialize_pin(const meshx_gpio_pin_config_kv_t *src,
                                                 meshx_gpio_pin_config_t *dst)
{
    if (!src || !dst) return;

    dst->logical_pin = src->logical_pin;
    dst->physical_pin = src->physical_pin;
    dst->mode = src->mode;
    dst->pull = src->pull;
    dst->drive_strength = src->drive_strength;
    dst->initial_level = src->initial_level;
    dst->signal_inversion = src->signal_inversion ? true : false;
}

/**
 * @brief Serialize PWM configuration.
 *
 * @param src Source runtime PWM configuration
 * @param dst Destination serialized configuration
 */
static inline void meshx_gpio_kv_serialize_pwm(const meshx_gpio_pin_config_t *src,
                                               meshx_gpio_pwm_config_kv_t *dst)
{
    if (!src || !dst) return;

    dst->frequency = src->mode_config.pwm.frequency;
    dst->duty_cycle = src->mode_config.pwm.duty_cycle;
    dst->resolution = src->mode_config.pwm.resolution;
    dst->channel = src->mode_config.pwm.channel;
    dst->reserved = 0;
}

/**
 * @brief Deserialize PWM configuration.
 *
 * @param src Source serialized configuration
 * @param dst Destination runtime configuration
 */
static inline void meshx_gpio_kv_deserialize_pwm(const meshx_gpio_pwm_config_kv_t *src,
                                                 meshx_gpio_pin_config_t *dst)
{
    if (!src || !dst) return;

    dst->mode_config.pwm.frequency = src->frequency;
    dst->mode_config.pwm.duty_cycle = src->duty_cycle;
    dst->mode_config.pwm.resolution = src->resolution;
    dst->mode_config.pwm.channel = src->channel;
}

/**
 * @brief Serialize interrupt configuration.
 *
 * @param src Source runtime interrupt configuration
 * @param dst Destination serialized configuration
 */
static inline void meshx_gpio_kv_serialize_intr(const meshx_gpio_pin_config_t *src,
                                                meshx_gpio_intr_config_kv_t *dst)
{
    if (!src || !dst) return;

    dst->trigger_type = src->mode_config.interrupt.trigger;
    dst->task_priority = src->mode_config.interrupt.task_priority;
    dst->task_stack_size = src->mode_config.interrupt.task_stack_size;
    dst->flags = 0;
    dst->reserved[0] = 0;
    dst->reserved[1] = 0;
    dst->reserved[2] = 0;
}

/**
 * @brief Deserialize interrupt configuration.
 *
 * @param src Source serialized configuration
 * @param dst Destination runtime configuration
 */
static inline void meshx_gpio_kv_deserialize_intr(const meshx_gpio_intr_config_kv_t *src,
                                                  meshx_gpio_pin_config_t *dst)
{
    if (!src || !dst) return;

    dst->mode_config.interrupt.trigger = src->trigger_type;
    dst->mode_config.interrupt.task_priority = src->task_priority;
    dst->mode_config.interrupt.task_stack_size = src->task_stack_size;
}

/*============================================================================
 * Version Compatibility Functions
 *============================================================================*/

/**
 * @brief Check if a configuration version is supported.
 *
 * @param version Version number to check
 * @return true if version is supported, false otherwise
 */
static inline bool meshx_gpio_kv_is_version_supported(uint8_t version)
{
    return (version >= MESHX_GPIO_CONFIG_VERSION_MIN &&
            version <= MESHX_GPIO_CONFIG_VERSION_MAX);
}

/**
 * @brief Get migration requirement between versions.
 *
 * @param from_version Source version
 * @param to_version Target version
 * @return true if migration is needed, false if direct loading is possible
 */
static inline bool meshx_gpio_kv_needs_migration(uint8_t from_version,
                                                  uint8_t to_version)
{
    return (from_version != to_version);
}

/*============================================================================
 * KV Engine Persistence API Functions
 *============================================================================*/

/**
 * @brief Initialize GPIO KV Engine persistence.
 *
 * This function initializes the KV Engine for GPIO configuration storage.
 * It must be called before any other GPIO KV functions.
 *
 * @param kv_partition Pointer to the flash partition to use for KV storage
 * @param product_name Product name for key prefix (max 16 chars)
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_kv_init(const meshx_fal_partition_t *kv_partition,
                               const char *product_name);

/**
 * @brief Save GPIO configuration to KV Engine.
 *
 * This function serializes and saves the GPIO configuration to KV Engine
 * with versioning and CRC validation.
 *
 * @param configs Array of pin configurations to save
 * @param pin_count Number of pins in configuration
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_save_config_to_kv(const meshx_gpio_pin_config_t *configs,
                                         uint8_t pin_count);

/**
 * @brief Load GPIO configuration from KV Engine.
 *
 * This function loads and deserializes GPIO configuration from KV Engine.
 * If the configuration is not found or corrupted, returns MESHX_NOT_FOUND
 * to allow fallback to compiled defaults.
 *
 * @param configs Array to store loaded pin configurations
 * @param max_pins Maximum pins that can be stored
 * @param[out] out_pin_count Actual number of pins loaded
 * @return meshx_err_t MESHX_SUCCESS on success, MESHX_NOT_FOUND if not found,
 *         error code on other failures
 */
meshx_err_t meshx_gpio_load_config_from_kv(meshx_gpio_pin_config_t *configs,
                                           uint8_t max_pins,
                                           uint8_t *out_pin_count);

/**
 * @brief Check if GPIO configuration exists in KV Engine.
 *
 * @param[out] exists true if configuration exists, false otherwise
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_config_exists_in_kv(bool *exists);

/**
 * @brief Save current pin state to KV Engine.
 *
 * Persists the runtime state of a GPIO pin across reboots.
 *
 * @param logical_pin Logical pin number
 * @param state Pointer to pin state to save
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_save_pin_state_to_kv(uint8_t logical_pin,
                                            const meshx_gpio_pin_state_t *state);

/**
 * @brief Load pin state from KV Engine.
 *
 * Loads previously persisted runtime state of a GPIO pin.
 *
 * @param logical_pin Logical pin number
 * @param[out] state Pointer to store loaded pin state
 * @return meshx_err_t MESHX_SUCCESS on success, MESHX_NOT_FOUND if not found,
 *         error code on other failures
 */
meshx_err_t meshx_gpio_load_pin_state_from_kv(uint8_t logical_pin,
                                              meshx_gpio_pin_state_t *state);

/**
 * @brief Export GPIO configuration to serialized format.
 *
 * Exports the configuration in binary format for transfer to another device.
 *
 * @param configs Array of pin configurations
 * @param pin_count Number of pins
 * @param[out] buf Output buffer
 * @param buf_len Buffer length
 * @param[out] out_size Actual exported size
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_export_config(const meshx_gpio_pin_config_t *configs,
                                     uint8_t pin_count,
                                     uint8_t *buf,
                                     uint16_t buf_len,
                                     uint16_t *out_size);

/**
 * @brief Import GPIO configuration from serialized format.
 *
 * Imports configuration from binary format received from another device.
 *
 * @param buf Input buffer
 * @param buf_len Buffer length
 * @param[out] configs Array to store imported configurations
 * @param max_pins Maximum pins that can be stored
 * @param[out] out_pin_count Actual number of pins imported
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_import_config(const uint8_t *buf,
                                     uint16_t buf_len,
                                     meshx_gpio_pin_config_t *configs,
                                     uint8_t max_pins,
                                     uint8_t *out_pin_count);

/**
 * @brief Clear GPIO configuration from KV Engine.
 *
 * Removes the stored GPIO configuration, forcing fallback to defaults
 * on next load.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_clear_config_in_kv(void);

/**
 * @brief Deinitialize GPIO KV Engine persistence.
 *
 * Cleans up KV persistence resources. Does not affect the underlying
 * KV Engine partition.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_kv_deinit(void);

/**
 * @brief Check if GPIO KV persistence is initialized.
 *
 * @return true if initialized, false otherwise
 */
bool meshx_gpio_kv_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_GPIO_KV_H */
