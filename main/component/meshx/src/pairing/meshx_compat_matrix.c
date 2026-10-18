/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_compat_matrix.c
 * @brief Semantic Compatibility Engine Implementation using element-layer LUTs (1:1 mapping).
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 */

#include "meshx_compat_matrix.h"
#include "meshx_element_lut.h"
#include "meshx_common.h"

/*
 * @brief Validate semantic compatibility between source and destination element descriptors.
 *
 * @param[in] src_desc Pointer to source element descriptor.
 * @param[in] dst_desc Pointer to destination element descriptor.
 * @param[in] func_id  Requested function ID to link.
 * @return MESHX_SUCCESS on compatibility, or error code.
 */
meshx_err_t meshx_compat_validate(const meshx_element_descriptor_t *src_desc,
                                  const meshx_element_descriptor_t *dst_desc,
                                  uint16_t func_id)
{
    if (!src_desc || !dst_desc) {
        return MESHX_INVALID_ARG;
    }

    /* 1. Check Roles: Must have a producer and a consumer */
    bool src_can_produce = (src_desc->role == MESHX_ROLE_PRODUCER ||
                            src_desc->role == MESHX_ROLE_BIDIRECTIONAL);
    bool dst_can_consume = (dst_desc->role == MESHX_ROLE_CONSUMER ||
                            dst_desc->role == MESHX_ROLE_BIDIRECTIONAL);

    if (!src_can_produce || !dst_can_consume) {
        MESHX_LOGE(MODULE_ID_COMMON, "Role collision: src_role=%u, dst_role=%u",
                   (unsigned int)src_desc->role, (unsigned int)dst_desc->role);
        return MESHX_ERR_ROLE_COLLISION;
    }

    /* 2. Check func_id availability on both endpoints */
    bool src_has_func = false;
    for (uint8_t i = 0; i < src_desc->func_count; ++i) {
        if (src_desc->func_ids[i] == func_id) {
            src_has_func = true;
            break;
        }
    }
    bool dst_has_func = false;
    for (uint8_t i = 0; i < dst_desc->func_count; ++i) {
        if (dst_desc->func_ids[i] == func_id) {
            dst_has_func = true;
            break;
        }
    }

    if (!src_has_func || !dst_has_func) {
        MESHX_LOGE(MODULE_ID_COMMON, "func_id 0x%04X not mutually supported (src:%d, dst:%d)",
                   func_id, (int)src_has_func, (int)dst_has_func);
        return MESHX_ERR_UNSUPPORTED_FUNC_ID;
    }

    /* 3. Check Variant Compatibility LUT (1:1 strict mapping queried from element layer) */
    size_t var_count = 0;
    const meshx_variant_compat_entry_t *var_lut = meshx_element_get_variant_compat_lut(&var_count);
    if (var_lut) {
        for (size_t i = 0; i < var_count; ++i) {
            if (var_lut[i].src_variant == src_desc->variant &&
                var_lut[i].dst_variant == dst_desc->variant) {
                return MESHX_SUCCESS;
            }
        }
    }

    MESHX_LOGE(MODULE_ID_COMMON, "Variant mismatch: src_variant=0x%04X, dst_variant=0x%04X",
               src_desc->variant, dst_desc->variant);
    return MESHX_ERR_INCOMPATIBLE_TRAIT;
}

/*
 * @brief High-level trait compatibility verification against element layer LUT.
 *
 * @param[in] source_trait Semantic trait of source element.
 * @param[in] sink_trait   Semantic trait of destination element.
 * @param[in] func_id      Requested function ID.
 * @return MESHX_SUCCESS if compatible, or MESHX_ERR_INCOMPATIBLE_TRAIT.
 */
meshx_err_t meshx_verify_trait_compatibility(meshx_trait_type_t source_trait,
                                             meshx_trait_type_t sink_trait,
                                             uint16_t func_id)
{
    /* Check Trait Compatibility LUT (1:1 strict mapping queried from element layer) */
    size_t trait_count = 0;
    const meshx_trait_compat_entry_t *trait_lut = meshx_element_get_trait_compat_lut(&trait_count);
    if (trait_lut)
    {
        for (size_t i = 0; i < trait_count; ++i)
        {
            if (trait_lut[i].source_trait == source_trait &&
                trait_lut[i].sink_trait == sink_trait)
            {
                if (func_id >= trait_lut[i].min_func_id &&
                    func_id <= trait_lut[i].max_func_id)
                {
                    return MESHX_SUCCESS;
                }
            }
        }
    }

    return MESHX_ERR_INCOMPATIBLE_TRAIT;
}
