/**
 * @file meshx_gpio_kv.c
 * @brief MeshX GPIO KV Engine Persistence Implementation
 *
 * This file implements the GPIO configuration persistence API using
 * the MeshX KV Engine. It provides save/load functions for GPIO
 * configuration with versioning, CRC validation, and corruption
 * fallback to compiled defaults.
 *
 * @author MeshX Team
 * @date 2024
 */

#include <string.h>
#include <stdbool.h>
#include "interface/gpio/meshx_gpio.h"
#include "interface/gpio/meshx_gpio_types.h"
#include "interface/gpio/meshx_gpio_kv.h"
#include "interface/utils/meshx_fal_interface.h"
#include "meshx_kv_engine.h"
#include "meshx_err.h"
#include "interface/logging/meshx_log.h"
#include "module_id.h"

/*============================================================================
 * Internal Constants
 *============================================================================*/

/** @brief Maximum buffer size for GPIO configuration serialization */
#define MESHX_GPIO_KV_MAX_CONFIG_SIZE    512

/** @brief Maximum number of pins supported in KV storage */
#define MESHX_GPIO_KV_MAX_PINS           32

/*============================================================================
 * Internal Variables
 *============================================================================*/

/** @brief KV Engine partition reference (set during initialization) */
static const meshx_fal_partition_t *gpio_kv_partition = NULL;

/** @brief Current product name for key prefix */
static char current_product_name[MESHX_GPIO_KV_PRODUCT_NAME_MAX_LEN + 1] = {0};

/** @brief Flag indicating if KV persistence is initialized */
static bool kv_persistence_initialized = false;

/*============================================================================
 * Internal Helper Functions
 *============================================================================*/

/**
 * @brief Serialize GPIO configuration to buffer.
 *
 * @param configs Array of pin configurations
 * @param pin_count Number of pins
 * @param buf Output buffer
 * @param buf_len Buffer length
 * @param[out] out_size Actual serialized size
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
static meshx_err_t serialize_gpio_config(const meshx_gpio_pin_config_t *configs,
                                         uint8_t pin_count,
                                         uint8_t *buf,
                                         uint16_t buf_len,
                                         uint16_t *out_size)
{
    if (!configs || !buf || !out_size || pin_count > MESHX_GPIO_KV_MAX_PINS) {
        return MESHX_INVALID_ARG;
    }

    /* Calculate required size */
    uint16_t required_size = MESHX_GPIO_CONFIG_HEADER_SIZE;
    required_size += pin_count * MESHX_GPIO_PIN_CONFIG_SIZE;

    /* Check for PWM pins that need extended configuration */
    for (uint8_t i = 0; i < pin_count; i++) {
        if (configs[i].mode == MESHX_GPIO_MODE_PWM_OUTPUT) {
            required_size += MESHX_GPIO_PWM_CONFIG_SIZE;
        }
    }

    if (buf_len < required_size) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Buffer too small: %d < %d", buf_len, required_size);
        return MESHX_NO_MEM;
    }

    /* Initialize header */
    meshx_gpio_config_header_kv_t *header = (meshx_gpio_config_header_kv_t *)buf;
    meshx_gpio_kv_init_header(header, pin_count);

    /* Set flags based on pin types */
    for (uint8_t i = 0; i < pin_count; i++) {
        if (configs[i].mode == MESHX_GPIO_MODE_PWM_OUTPUT) {
            header->flags |= MESHX_GPIO_CONFIG_FLAG_HAS_PWM;
        }
    }

    /* Serialize pin configurations */
    uint8_t *ptr = buf + MESHX_GPIO_CONFIG_HEADER_SIZE;
    for (uint8_t i = 0; i < pin_count; i++) {
        meshx_gpio_pin_config_kv_t *pin_cfg = (meshx_gpio_pin_config_kv_t *)ptr;
        meshx_gpio_kv_serialize_pin(&configs[i], pin_cfg);
        ptr += MESHX_GPIO_PIN_CONFIG_SIZE;

        /* Serialize PWM configuration if applicable */
        if (configs[i].mode == MESHX_GPIO_MODE_PWM_OUTPUT) {
            meshx_gpio_pwm_config_kv_t *pwm_cfg = (meshx_gpio_pwm_config_kv_t *)ptr;
            meshx_gpio_kv_serialize_pwm(&configs[i], pwm_cfg);
            ptr += MESHX_GPIO_PWM_CONFIG_SIZE;
        }
    }

    /* Update total size in header */
    header->total_size = (uint16_t)(ptr - buf);

    /* Calculate and set CRC16 */
    meshx_gpio_kv_finalize_crc16(buf, header->total_size);

    *out_size = header->total_size;

    MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_GPIO, "Serialized %d pins, size=%d", pin_count, *out_size);
    return MESHX_SUCCESS;
}

/**
 * @brief Deserialize GPIO configuration from buffer.
 *
 * @param buf Input buffer
 * @param buf_len Buffer length
 * @param configs Array to store pin configurations
 * @param max_pins Maximum pins that can be stored
 * @param[out] out_pin_count Actual pin count read
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
static meshx_err_t deserialize_gpio_config(const uint8_t *buf,
                                           uint16_t buf_len,
                                           meshx_gpio_pin_config_t *configs,
                                           uint8_t max_pins,
                                           uint8_t *out_pin_count)
{
    if (!buf || !configs || !out_pin_count) {
        return MESHX_INVALID_ARG;
    }

    if (buf_len < MESHX_GPIO_CONFIG_HEADER_SIZE) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Buffer too small for header");
        return MESHX_INVALID_ARG;
    }

    /* Parse header */
    const meshx_gpio_config_header_kv_t *header = (const meshx_gpio_config_header_kv_t *)buf;

    /* Validate version */
    if (!meshx_gpio_kv_is_version_supported(header->version)) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Unsupported config version: %d", header->version);
        return MESHX_ERR_GPIO_CONFIG_INVALID;
    }

    /* Verify CRC16 */
    if (!meshx_gpio_kv_verify_crc16(buf, buf_len)) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "CRC verification failed");
        return MESHX_ERR_GPIO_CONFIG_INVALID;
    }

    /* Validate pin count */
    if (header->pin_count > max_pins) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Pin count %d exceeds max %d",
                   header->pin_count, max_pins);
        return MESHX_NO_MEM;
    }

    /* Deserialize pin configurations */
    const uint8_t *ptr = buf + MESHX_GPIO_CONFIG_HEADER_SIZE;
    uint8_t pin_count = header->pin_count;

    for (uint8_t i = 0; i < pin_count; i++) {
        if (ptr + MESHX_GPIO_PIN_CONFIG_SIZE > buf + buf_len) {
            MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Unexpected end of data at pin %d", i);
            return MESHX_ERR_GPIO_CONFIG_INVALID;
        }

        const meshx_gpio_pin_config_kv_t *pin_cfg = (const meshx_gpio_pin_config_kv_t *)ptr;
        meshx_gpio_kv_deserialize_pin(pin_cfg, &configs[i]);
        ptr += MESHX_GPIO_PIN_CONFIG_SIZE;

        /* Deserialize PWM configuration if present */
        if (pin_cfg->config_type == MESHX_GPIO_CONFIG_TYPE_PWM) {
            if (ptr + MESHX_GPIO_PWM_CONFIG_SIZE > buf + buf_len) {
                MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Unexpected end of PWM data at pin %d", i);
                return MESHX_ERR_GPIO_CONFIG_INVALID;
            }
            const meshx_gpio_pwm_config_kv_t *pwm_cfg = (const meshx_gpio_pwm_config_kv_t *)ptr;
            meshx_gpio_kv_deserialize_pwm(pwm_cfg, &configs[i]);
            ptr += MESHX_GPIO_PWM_CONFIG_SIZE;
        }

        /* Deserialize interrupt configuration if present */
        if (pin_cfg->config_type == MESHX_GPIO_CONFIG_TYPE_INTR) {
            if (ptr + MESHX_GPIO_INTR_CONFIG_SIZE > buf + buf_len) {
                MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Unexpected end of intr data at pin %d", i);
                return MESHX_ERR_GPIO_CONFIG_INVALID;
            }
            const meshx_gpio_intr_config_kv_t *intr_cfg = (const meshx_gpio_intr_config_kv_t *)ptr;
            meshx_gpio_kv_deserialize_intr(intr_cfg, &configs[i]);
            ptr += MESHX_GPIO_INTR_CONFIG_SIZE;
        }
    }

    *out_pin_count = pin_count;

    MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_GPIO, "Deserialized %d pins", pin_count);
    return MESHX_SUCCESS;
}

/*============================================================================
 * Public API Implementation
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
                               const char *product_name)
{
    if (!kv_partition || !product_name) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Invalid arguments for KV init");
        return MESHX_INVALID_ARG;
    }

    /* Initialize KV Engine */
    meshx_err_t err = meshx_kv_engine_init(kv_partition);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to initialize KV Engine: %d", err);
        return err;
    }

    /* Store partition reference */
    gpio_kv_partition = kv_partition;

    /* Store product name for key prefix */
    strncpy(current_product_name, product_name, MESHX_GPIO_KV_PRODUCT_NAME_MAX_LEN);
    current_product_name[MESHX_GPIO_KV_PRODUCT_NAME_MAX_LEN] = '\0';

    kv_persistence_initialized = true;

    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "GPIO KV persistence initialized for product: %s",
               current_product_name);
    return MESHX_SUCCESS;
}

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
                                         uint8_t pin_count)
{
    if (!kv_persistence_initialized) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "KV persistence not initialized");
        return MESHX_ERR_GPIO_NOT_INITIALIZED;
    }

    if (!configs || pin_count == 0) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Invalid arguments for save");
        return MESHX_INVALID_ARG;
    }

    /* Construct KV key */
    char key[MESHX_GPIO_KV_KEY_MAX_LEN];
    if (meshx_gpio_kv_make_config_key(current_product_name, key, sizeof(key)) == 0) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to construct config key");
        return MESHX_FAIL;
    }

    /* Serialize configuration */
    uint8_t buffer[MESHX_GPIO_KV_MAX_CONFIG_SIZE];
    uint16_t config_size = 0;

    meshx_err_t err = serialize_gpio_config(configs, pin_count, buffer, sizeof(buffer), &config_size);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to serialize config: %d", err);
        return err;
    }

    /* Save to KV Engine (buffered in RAM) */
    err = meshx_kv_engine_set(key, buffer, config_size);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to set KV value: %d", err);
        return err;
    }

    /* Commit to flash */
    err = meshx_kv_engine_commit();
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to commit KV: %d", err);
        return err;
    }

    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "Saved GPIO config: %d pins, %d bytes",
               pin_count, config_size);
    return MESHX_SUCCESS;
}

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
                                           uint8_t *out_pin_count)
{
    if (!kv_persistence_initialized) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "KV persistence not initialized");
        return MESHX_ERR_GPIO_NOT_INITIALIZED;
    }

    if (!configs || !out_pin_count) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Invalid arguments for load");
        return MESHX_INVALID_ARG;
    }

    /* Construct KV key */
    char key[MESHX_GPIO_KV_KEY_MAX_LEN];
    if (meshx_gpio_kv_make_config_key(current_product_name, key, sizeof(key)) == 0) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to construct config key");
        return MESHX_FAIL;
    }

    /* Read from KV Engine */
    uint8_t buffer[MESHX_GPIO_KV_MAX_CONFIG_SIZE];
    uint16_t read_size = sizeof(buffer);

    meshx_err_t err = meshx_kv_engine_read(key, buffer, read_size);
    if (err != MESHX_SUCCESS) {
        if (err == MESHX_NOT_FOUND) {
            MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "No GPIO config found in KV, using defaults");
        } else {
            MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to read KV: %d", err);
        }
        return err;
    }

    /* Deserialize configuration */
    err = deserialize_gpio_config(buffer, read_size, configs, max_pins, out_pin_count);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to deserialize config (corrupted?): %d", err);
        /* Return NOT_FOUND to trigger fallback to defaults */
        return MESHX_NOT_FOUND;
    }

    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "Loaded GPIO config: %d pins", *out_pin_count);
    return MESHX_SUCCESS;
}

/**
 * @brief Check if GPIO configuration exists in KV Engine.
 *
 * @param[out] exists true if configuration exists, false otherwise
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_config_exists_in_kv(bool *exists)
{
    if (!kv_persistence_initialized) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "KV persistence not initialized");
        return MESHX_ERR_GPIO_NOT_INITIALIZED;
    }

    if (!exists) {
        return MESHX_INVALID_ARG;
    }

    /* Construct KV key */
    char key[MESHX_GPIO_KV_KEY_MAX_LEN];
    if (meshx_gpio_kv_make_config_key(current_product_name, key, sizeof(key)) == 0) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to construct config key");
        return MESHX_FAIL;
    }

    /* Try to read with zero-length buffer to check existence */
    uint8_t dummy;
    meshx_err_t err = meshx_kv_engine_read(key, &dummy, 0);

    *exists = (err == MESHX_SUCCESS);

    return MESHX_SUCCESS;
}

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
                                            const meshx_gpio_pin_state_t *state)
{
    if (!kv_persistence_initialized) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "KV persistence not initialized");
        return MESHX_ERR_GPIO_NOT_INITIALIZED;
    }

    if (!state) {
        return MESHX_INVALID_ARG;
    }

    /* Construct KV key for state */
    char key[MESHX_GPIO_KV_KEY_MAX_LEN];
    if (meshx_gpio_kv_make_state_key(current_product_name, logical_pin, key, sizeof(key)) == 0) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to construct state key");
        return MESHX_FAIL;
    }

    /* Serialize pin state */
    meshx_gpio_pin_state_kv_t state_kv = {
        .logical_pin = logical_pin,
        .current_level = state->current_level,
        .pwm_started = state->mode_state.pwm.started,
        .current_duty = state->mode_state.pwm.duty_cycle,
        .reserved = 0
    };

    /* Save to KV Engine */
    meshx_err_t err = meshx_kv_engine_set(key, &state_kv, sizeof(state_kv));
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to save pin state: %d", err);
        return err;
    }

    /* Commit to flash */
    err = meshx_kv_engine_commit();
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to commit pin state: %d", err);
        return err;
    }

    MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_GPIO, "Saved state for pin %u: level=%u",
               logical_pin, state->current_level);
    return MESHX_SUCCESS;
}

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
                                              meshx_gpio_pin_state_t *state)
{
    if (!kv_persistence_initialized) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "KV persistence not initialized");
        return MESHX_ERR_GPIO_NOT_INITIALIZED;
    }

    if (!state) {
        return MESHX_INVALID_ARG;
    }

    /* Construct KV key for state */
    char key[MESHX_GPIO_KV_KEY_MAX_LEN];
    if (meshx_gpio_kv_make_state_key(current_product_name, logical_pin, key, sizeof(key)) == 0) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to construct state key");
        return MESHX_FAIL;
    }

    /* Read from KV Engine */
    meshx_gpio_pin_state_kv_t state_kv;
    uint16_t read_size = sizeof(state_kv);

    meshx_err_t err = meshx_kv_engine_read(key, &state_kv, read_size);
    if (err != MESHX_SUCCESS) {
        if (err == MESHX_NOT_FOUND) {
            MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_GPIO, "No state found for pin %u", logical_pin);
        } else {
            MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to read pin state: %d", err);
        }
        return err;
    }

    /* Apply loaded state */
    state->current_level = state_kv.current_level;
    state->mode_state.pwm.started = state_kv.pwm_started;
    state->mode_state.pwm.duty_cycle = state_kv.current_duty;

    MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_GPIO, "Loaded state for pin %u: level=%u",
               logical_pin, state->current_level);
    return MESHX_SUCCESS;
}

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
                                     uint16_t *out_size)
{
    if (!configs || !buf || !out_size) {
        return MESHX_INVALID_ARG;
    }

    /* Export uses the same serialization format as KV storage */
    return serialize_gpio_config(configs, pin_count, buf, buf_len, out_size);
}

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
                                     uint8_t *out_pin_count)
{
    if (!buf || !configs || !out_pin_count) {
        return MESHX_INVALID_ARG;
    }

    /* Import uses the same deserialization format as KV storage */
    return deserialize_gpio_config(buf, buf_len, configs, max_pins, out_pin_count);
}

/**
 * @brief Clear GPIO configuration from KV Engine.
 *
 * Removes the stored GPIO configuration, forcing fallback to defaults
 * on next load.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_clear_config_in_kv(void)
{
    if (!kv_persistence_initialized) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "KV persistence not initialized");
        return MESHX_ERR_GPIO_NOT_INITIALIZED;
    }

    /* Construct KV key */
    char key[MESHX_GPIO_KV_KEY_MAX_LEN];
    if (meshx_gpio_kv_make_config_key(current_product_name, key, sizeof(key)) == 0) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to construct config key");
        return MESHX_FAIL;
    }

    /* Remove from KV Engine */
    meshx_err_t err = meshx_kv_engine_remove(key);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to remove config: %d", err);
        return err;
    }

    /* Commit the removal */
    err = meshx_kv_engine_commit();
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_GPIO, "Failed to commit removal: %d", err);
        return err;
    }

    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "Cleared GPIO config from KV");
    return MESHX_SUCCESS;
}

/**
 * @brief Deinitialize GPIO KV Engine persistence.
 *
 * Cleans up KV persistence resources. Does not affect the underlying
 * KV Engine partition.
 *
 * @return meshx_err_t MESHX_SUCCESS on success, error code on failure
 */
meshx_err_t meshx_gpio_kv_deinit(void)
{
    if (!kv_persistence_initialized) {
        return MESHX_SUCCESS;
    }

    gpio_kv_partition = NULL;
    memset(current_product_name, 0, sizeof(current_product_name));
    kv_persistence_initialized = false;

    MESHX_LOGI(MODULE_ID_COMPONENT_MESHX_GPIO, "GPIO KV persistence deinitialized");
    return MESHX_SUCCESS;
}

/**
 * @brief Check if GPIO KV persistence is initialized.
 *
 * @return true if initialized, false otherwise
 */
bool meshx_gpio_kv_is_initialized(void)
{
    return kv_persistence_initialized;
}
