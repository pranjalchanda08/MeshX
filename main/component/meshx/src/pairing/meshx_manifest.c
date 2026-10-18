/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_manifest.c
 * @brief Implementation of Capability Manifest Introspection Engine in pure C.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 */

#include <string.h>
#include <stdio.h>

#include "meshx_manifest.h"
#include "meshx_element_lut.h"
#include "meshx_builder_api.h"
#include "meshx_api.h"
#include "meshx_nvs.h"
#include "meshx_common.h"

/*
 * @brief Resolve operational role and semantic trait from element variant using element LUT.
 *
 * @param[in]  variant   Element variant.
 * @param[out] out_role  Pointer to store resolved role.
 * @param[out] out_trait Pointer to store resolved trait.
 */
void meshx_manifest_resolve_role_trait(uint16_t variant, uint8_t *out_role, uint16_t *out_trait)
{
    if (!out_role || !out_trait)
    {
        return;
    }

    *out_role  = MESHX_ROLE_UNKNOWN;
    *out_trait = MESHX_TRAIT_UNKNOWN;

    size_t count = 0;
    const meshx_variant_lut_entry_t *lut = meshx_element_get_variant_lut(&count);
    if (!lut)
    {
        return;
    }

    for (size_t i = 0; i < count; ++i)
    {
        if (lut[i].variant == variant)
        {
            *out_role  = lut[i].role;
            *out_trait = lut[i].trait;
            return;
        }
    }
}

/*
 * @brief Build local capability manifest by introspecting registered elements and models.
 *
 * @param[out] out_manifest Pointer to manifest structure to populate.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_manifest_build_local(meshx_capability_manifest_t *out_manifest)
{
    uint8_t  resolved_role  = 0;
    uint16_t resolved_trait = 0;
    uint16_t local_func_ids[MESHX_MAX_FUNC_IDS_PER_ELEM] = {0};
    if (!out_manifest)
    {
        return MESHX_INVALID_ARG;
    }
    memset(out_manifest, 0, sizeof(*out_manifest));

    out_manifest->unicast_addr = meshx_get_node_addr();
    out_manifest->company_id   = meshx_get_cid();
    out_manifest->product_id   = meshx_get_pid();
    out_manifest->link_epoch   = meshx_nvs_get_link_epoch();

    uint8_t total_elements = meshx_comp_get_element_count();
    if (total_elements > MESHX_MAX_MANIFEST_ELEMENTS)
    {
        total_elements = MESHX_MAX_MANIFEST_ELEMENTS;
    }
    out_manifest->element_count = total_elements;

    for (uint8_t i = 0; i < total_elements; ++i)
    {
        meshx_element_descriptor_t *desc = &out_manifest->elements[i];
        desc->element_idx = i;
        desc->variant     = meshx_comp_get_element_variant(i);

        // Resolve role and trait from variant using LUT
        meshx_manifest_resolve_role_trait(desc->variant, &resolved_role, &resolved_trait);
        desc->role = resolved_role;
        desc->trait = resolved_trait;

        // Populate supported func_ids from active logical models via bridge
        desc->func_count = meshx_comp_get_element_func_ids(i, local_func_ids, MESHX_MAX_FUNC_IDS_PER_ELEM);
        memcpy(desc->func_ids, local_func_ids, sizeof(desc->func_ids));

        // Copy human-readable element name
        snprintf(desc->label, sizeof(desc->label), "%s %u",
                 (desc->role == MESHX_ROLE_PRODUCER) ? "Switch" : "Actuator", (unsigned int)(i + 1));
    }

    return MESHX_SUCCESS;
}
