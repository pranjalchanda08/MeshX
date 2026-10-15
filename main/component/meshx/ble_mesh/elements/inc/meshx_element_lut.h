/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_element_lut.h
 * @brief Element capability, variant, and semantic compatibility Look-Up Tables.
 *
 * Exposes authoritative Look-Up Tables defined in the element/composition layer
 * for roles, traits, and compatibility validation.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 */

#ifndef __MESHX_ELEMENT_LUT_H__
#define __MESHX_ELEMENT_LUT_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "meshx_common.h"
#include "meshx_manifest.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Variant to role and trait mapping entry.
 */
typedef struct {
    uint16_t variant;     /**< meshx_element_type_t */
    uint8_t  role;        /**< meshx_element_role_t */
    uint16_t trait;       /**< meshx_trait_type_t */
} meshx_variant_lut_entry_t;

/**
 * @brief Variant compatibility entry (1:1 mapping between source and sink).
 */
typedef struct {
    uint16_t src_variant;
    uint16_t dst_variant;
} meshx_variant_compat_entry_t;

/**
 * @brief Trait compatibility entry (1:1 mapping between traits and valid func_id bounds).
 */
typedef struct {
    meshx_trait_type_t source_trait;
    meshx_trait_type_t sink_trait;
    uint16_t           min_func_id;
    uint16_t           max_func_id;
} meshx_trait_compat_entry_t;

/**
 * @brief Get the address of the variant-to-role/trait Look-Up Table.
 *
 * @param[out] out_count Pointer to receive the number of entries in the table.
 * @return Const pointer to the static LUT array.
 */
const meshx_variant_lut_entry_t* meshx_element_get_variant_lut(size_t *out_count);

/**
 * @brief Get the address of the 1:1 variant compatibility Look-Up Table.
 *
 * @param[out] out_count Pointer to receive the number of entries in the table.
 * @return Const pointer to the static LUT array.
 */
const meshx_variant_compat_entry_t* meshx_element_get_variant_compat_lut(size_t *out_count);

/**
 * @brief Get the address of the 1:1 trait compatibility Look-Up Table.
 *
 * @param[out] out_count Pointer to receive the number of entries in the table.
 * @return Const pointer to the static LUT array.
 */
const meshx_trait_compat_entry_t* meshx_element_get_trait_compat_lut(size_t *out_count);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_ELEMENT_LUT_H__ */
