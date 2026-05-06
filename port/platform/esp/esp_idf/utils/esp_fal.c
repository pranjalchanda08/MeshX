/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file esp_fal.c
 * @brief ESP-IDF implementation for Flash Abstraction Layer (FAL).
 *
 * @author Pranjal Chanda
 */

#include "interface/utils/meshx_fal_interface.h"
#include "esp_partition.h"
#include "esp_err.h"

meshx_err_t meshx_fal_read(const meshx_fal_partition_t *part, uint32_t offset, void *buf, size_t len)
{
    if (!part || !buf) {
        return MESHX_INVALID_ARG;
    }

    const esp_partition_t *handle = (const esp_partition_t *)part->priv;
    esp_err_t err = esp_partition_read(handle, offset, buf, len);
    
    if (err != ESP_OK) {
        return MESHX_ERR_PLAT;
    }

    return MESHX_SUCCESS;
}

meshx_err_t meshx_fal_write(const meshx_fal_partition_t *part, uint32_t offset, const void *buf, size_t len)
{
    if (!part || !buf) {
        return MESHX_INVALID_ARG;
    }

    const esp_partition_t *handle = (const esp_partition_t *)part->priv;
    esp_err_t err = esp_partition_write(handle, offset, buf, len);
    
    if (err != ESP_OK) {
        return MESHX_ERR_PLAT;
    }

    return MESHX_SUCCESS;
}

meshx_err_t meshx_fal_erase(const meshx_fal_partition_t *part, uint32_t offset, size_t len)
{
    if (!part) {
        return MESHX_INVALID_ARG;
    }

    const esp_partition_t *handle = (const esp_partition_t *)part->priv;
    esp_err_t err = esp_partition_erase_range(handle, offset, len);
    
    if (err != ESP_OK) {
        return MESHX_ERR_PLAT;
    }

    return MESHX_SUCCESS;
}
