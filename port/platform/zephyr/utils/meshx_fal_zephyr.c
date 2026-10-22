/**
 * Copyright (c) 2024 - 2026 MeshX
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file meshx_fal_zephyr.c
 * @brief Flash Abstraction Layer (FAL) implementation using Zephyr flash_map API.
 */

#include "interface/utils/meshx_fal_interface.h"
#include "interface/logging/meshx_log.h"
#include <zephyr/storage/flash_map.h>
#include <string.h>

#define ZEPHYR_FAL_SECTOR_SIZE (4096)

meshx_err_t meshx_fal_read(const meshx_fal_partition_t *part, uint32_t offset, void *buf, size_t len)
{
    if (!part || !buf) {
        return MESHX_INVALID_ARG;
    }

    const struct flash_area *fa = (const struct flash_area *)part->priv;
    if (!fa) {
        return MESHX_ERR_PLAT;
    }

    int rc = flash_area_read(fa, offset, buf, len);
    return (rc == 0) ? MESHX_SUCCESS : MESHX_ERR_PLAT;
}

meshx_err_t meshx_fal_write(const meshx_fal_partition_t *part, uint32_t offset, const void *buf, size_t len)
{
    if (!part || !buf) {
        return MESHX_INVALID_ARG;
    }

    const struct flash_area *fa = (const struct flash_area *)part->priv;
    if (!fa) {
        return MESHX_ERR_PLAT;
    }

    int rc = flash_area_write(fa, offset, buf, len);
    return (rc == 0) ? MESHX_SUCCESS : MESHX_ERR_PLAT;
}

meshx_err_t meshx_fal_erase(const meshx_fal_partition_t *part, uint32_t offset, size_t len)
{
    if (!part) {
        return MESHX_INVALID_ARG;
    }

    const struct flash_area *fa = (const struct flash_area *)part->priv;
    if (!fa) {
        return MESHX_ERR_PLAT;
    }

    int rc = flash_area_erase(fa, offset, len);
    return (rc == 0) ? MESHX_SUCCESS : MESHX_ERR_PLAT;
}

meshx_err_t meshx_fal_find_partition(const char *name, meshx_fal_partition_t *part)
{
    if (!name || !part) {
        return MESHX_INVALID_ARG;
    }

    uint8_t area_id = 0xFF;
    if (strcmp(name, "meshx-cfg") == 0 || strcmp(name, "meshx_cfg") == 0) {
#if FIXED_PARTITION_EXISTS(meshx_cfg_partition)
        area_id = FIXED_PARTITION_ID(meshx_cfg_partition);
#endif
    } else if (strcmp(name, "meshx-nvs") == 0 || strcmp(name, "meshx_nvs") == 0) {
#if FIXED_PARTITION_EXISTS(meshx_nvs_partition)
        area_id = FIXED_PARTITION_ID(meshx_nvs_partition);
#endif
    } else if (strcmp(name, "storage") == 0) {
#if FIXED_PARTITION_EXISTS(storage_partition)
        area_id = FIXED_PARTITION_ID(storage_partition);
#endif
    }

    if (area_id == 0xFF) {
        return MESHX_NOT_FOUND;
    }

    const struct flash_area *fa_ptr = NULL;
    int rc = flash_area_open(area_id, &fa_ptr);
    if (rc != 0 || !fa_ptr) {
        return MESHX_NOT_FOUND;
    }

    part->name = name;
    part->base_addr = fa_ptr->fa_off;
    part->size = fa_ptr->fa_size;
    part->sector_size = ZEPHYR_FAL_SECTOR_SIZE;
    part->priv = (void *)fa_ptr;

    return MESHX_SUCCESS;
}
