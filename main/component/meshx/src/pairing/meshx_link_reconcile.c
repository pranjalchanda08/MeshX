/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_link_reconcile.c
 * @brief Boot-time autonomous link self-audit and reconciliation in pure C.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 */

#include <string.h>
#include <stdbool.h>

#include "meshx_nvs.h"
#include "meshx_builder_api.h"
#include "meshx_common.h"

/**
 * @brief Self-audit and reconcile persistent link table against active composition.
 *
 * Scans all persisted links on boot. Prunes any stale links where the local element
 * index, variant, or functional model no longer exists in the baked composition.
 * Increments link_epoch if any stale links are purged.
 *
 * @return MESHX_SUCCESS on completion, or MESHX_FAIL if NVS read error.
 */
meshx_err_t meshx_link_reconcile(void)
{
    meshx_nvs_link_table_t table;
    if (meshx_nvs_link_table_get(&table) != MESHX_SUCCESS)
    {
        return MESHX_FAIL;
    }

    uint8_t total_elements = meshx_comp_get_element_count();
    bool table_modified = false;

    for (uint8_t i = 0; i < MESHX_MAX_PERSISTED_LINKS; ++i)
    {
        meshx_nvs_link_entry_t *link = &table.links[i];
        if (!link->active)
            continue;

        bool link_valid = false;
        if (link->local_element_idx < total_elements)
        {
            uint16_t variant = meshx_comp_get_element_variant(link->local_element_idx);
            if (variant == link->bound_variant)
            {
                // Verify func_id support against active logical models
                if (meshx_comp_element_has_func_id(link->local_element_idx, link->func_id))
                {
                    link_valid = true;
                }
            }
        }

        if (!link_valid)
        {
            MESHX_LOGW(MODULE_ID_COMMON, "Pruning stale link slot %u (el=%u, variant=0x%04X, func=0x%04X)",
                       i, link->local_element_idx, link->bound_variant, link->func_id);
            memset(link, 0, sizeof(*link));
            if (table.link_count > 0)
            {
                table.link_count--;
            }
            table_modified = true;
        }
    }

    if (table_modified)
    {
        table.link_epoch++;
        meshx_nvs_link_table_set(&table);
        MESHX_LOGI(MODULE_ID_COMMON, "Link table sanitized. New epoch: %u", table.link_epoch);
    }

    return MESHX_SUCCESS;
}
