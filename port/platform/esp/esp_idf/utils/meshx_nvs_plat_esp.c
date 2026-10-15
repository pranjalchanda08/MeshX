/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_nvs_plat_esp.c
 * @brief ESP-IDF implementation for Portable NVS Platform Bridge.
 *
 * @author Pranjal Chanda
 */

#include "interface/utils/meshx_nvs_interface.h"
#include "interface/logging/meshx_log.h"
#include "meshx_kv_engine.h"
#include "esp_partition.h"

#define MODULE_ID_PLATFORM_NVS MODULE_ID_COMMON

static meshx_fal_partition_t nvs_part = {
    .name = "meshx_nvs",
    .size = 0,
    .sector_size = 4096,
    .priv = NULL
};

/**
 * @brief Open the NVS partition and initialize the KV engine.
 *
 * @param[out] p_nvs_handle Pointer to store the NVS handle.
 * @return meshx_err_t.
 */
meshx_err_t meshx_nvs_plat_open(uintptr_t *p_nvs_handle)
{
    if (p_nvs_handle == NULL) return MESHX_INVALID_ARG;

    // Find the partition by name
    const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, nvs_part.name);

    if (part == NULL) {
        MESHX_LOGE(MODULE_ID_PLATFORM_NVS, "Partition '%s' not found!", nvs_part.name);
        return MESHX_NOT_FOUND;
    }

    nvs_part.size = part->size;
    nvs_part.priv = (void *)part;

    MESHX_LOGD(MODULE_ID_PLATFORM_NVS, "Found partition '%s' at 0x%lx, size %d",
               nvs_part.name, part->address, part->size);

    meshx_err_t err = meshx_kv_engine_init(&nvs_part);
    if (err == MESHX_SUCCESS) {
        *p_nvs_handle = (uintptr_t)&nvs_part;
    } else {
        MESHX_LOGE(MODULE_ID_PLATFORM_NVS, "KV Engine init failed: %d", err);
    }

    return err;
}

/**
 * @brief Close the NVS partition.
 *
 * @param[in] p_nvs_handle NVS handle.
 * @return meshx_err_t.
 */
meshx_err_t meshx_nvs_plat_close(uintptr_t p_nvs_handle)
{
    return MESHX_SUCCESS;
}

/**
 * @brief Read a value from the NVS partition.
 *
 * @param[in] p_nvs_handle NVS handle.
 * @param[in] key Key to read.
 * @param[out] p_data Buffer to store the value.
 * @param[in] len Length of the buffer.
 * @return meshx_err_t.
 */
meshx_err_t meshx_nvs_plat_read(uintptr_t p_nvs_handle, char const *key , uint8_t *p_data, uint16_t len)
{
    return meshx_kv_engine_read(key, p_data, len);
}

/**
 * @brief Write a value to the NVS partition.
 *
 * @param[in] p_nvs_handle NVS handle.
 * @param[in] key Key to write.
 * @param[in] p_data Buffer containing the value.
 * @param[in] len Length of the value.
 * @return meshx_err_t.
 */
meshx_err_t meshx_nvs_plat_write(uintptr_t p_nvs_handle, char const * key, uint8_t const *p_data, uint16_t len)
{
    return meshx_kv_engine_set(key, p_data, len);
}

/**
 * @brief Erase all data in the NVS partition.
 *
 * @param[in] p_nvs_handle NVS handle.
 * @return meshx_err_t.
 */
meshx_err_t meshx_nvs_plat_erase(uintptr_t p_nvs_handle)
{
    return meshx_kv_engine_erase_all();
}

/**
 * @brief Remove a value from the NVS partition.
 *
 * @param[in] p_nvs_handle NVS handle.
 * @param[in] key Key to remove.
 * @return meshx_err_t.
 */
meshx_err_t meshx_nvs_plat_remove(uintptr_t p_nvs_handle, char const* key)
{
    return meshx_kv_engine_remove(key);
}

/**
 * @brief Commit any pending writes to the NVS partition.
 *
 * @param[in] p_nvs_handle NVS handle.
 * @return meshx_err_t.
 */
meshx_err_t meshx_nvs_plat_commit(uintptr_t p_nvs_handle)
{
    return meshx_kv_engine_commit();
}
