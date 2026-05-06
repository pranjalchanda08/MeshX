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

    MESHX_LOGI(MODULE_ID_PLATFORM_NVS, "Found partition '%s' at 0x%lx, size %d", 
               nvs_part.name, part->address, part->size);

    meshx_err_t err = meshx_kv_engine_init(&nvs_part);
    if (err == MESHX_SUCCESS) {
        *p_nvs_handle = (uintptr_t)&nvs_part;
    } else {
        MESHX_LOGE(MODULE_ID_PLATFORM_NVS, "KV Engine init failed: %d", err);
    }
    
    return err;
}

meshx_err_t meshx_nvs_plat_close(uintptr_t p_nvs_handle)
{
    return MESHX_SUCCESS;
}

meshx_err_t meshx_nvs_plat_read(uintptr_t p_nvs_handle, char const *key , uint8_t *p_data, uint16_t len)
{
    return meshx_kv_engine_read(key, p_data, len);
}

meshx_err_t meshx_nvs_plat_write(uintptr_t p_nvs_handle, char const * key, uint8_t const *p_data, uint16_t len)
{
    return meshx_kv_engine_set(key, p_data, len);
}

meshx_err_t meshx_nvs_plat_erase(uintptr_t p_nvs_handle)
{
    return meshx_kv_engine_erase_all();
}

meshx_err_t meshx_nvs_plat_remove(uintptr_t p_nvs_handle, char const* key)
{
    return meshx_kv_engine_remove(key);
}

meshx_err_t meshx_nvs_plat_commit(uintptr_t p_nvs_handle)
{
    return meshx_kv_engine_commit();
}
