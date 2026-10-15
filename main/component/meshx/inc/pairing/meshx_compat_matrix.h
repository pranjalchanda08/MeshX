/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_compat_matrix.h
 * @brief Semantic Compatibility Engine.
 *
 * Provides type-safe validation barrier enforcing role rules, func_id support,
 * and element variant compatibility before pairing configuration changes occur.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 */

#ifndef __MESHX_COMPAT_MATRIX_H__
#define __MESHX_COMPAT_MATRIX_H__

#include <stdint.h>
#include <stdbool.h>
#include "meshx_err.h"
#include "meshx_manifest.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Validate semantic compatibility between source and destination element descriptors.
 *
 * Enforces:
 * 1. Role validation (producer -> consumer).
 * 2. func_id availability on both endpoints.
 * 3. Element variant compatibility matrix.
 *
 * @param[in] src_desc Pointer to source (client/producer) element descriptor.
 * @param[in] dst_desc Pointer to destination (server/consumer) element descriptor.
 * @param[in] func_id  Requested function ID to link.
 *
 * @return
 *     - MESHX_SUCCESS: Pair is compatible.
 *     - MESHX_INVALID_ARG: Null pointer passed.
 *     - MESHX_ERR_ROLE_COLLISION: Two producers or two consumers.
 *     - MESHX_ERR_UNSUPPORTED_FUNC_ID: func_id missing on one or both endpoints.
 *     - MESHX_ERR_INCOMPATIBLE_TRAIT: Variant mismatch according to compatibility matrix.
 */
meshx_err_t meshx_compat_validate(const meshx_element_descriptor_t *src_desc,
                                  const meshx_element_descriptor_t *dst_desc,
                                  uint16_t func_id);

/**
 * @brief High-level trait compatibility verification.
 *
 * @param[in] source_trait Semantic trait of source element.
 * @param[in] sink_trait   Semantic trait of destination element.
 * @param[in] func_id      Requested function ID.
 *
 * @return MESHX_SUCCESS if compatible, or MESHX_ERR_INCOMPATIBLE_TRAIT.
 */
meshx_err_t meshx_verify_trait_compatibility(meshx_trait_type_t source_trait,
                                             meshx_trait_type_t sink_trait,
                                             uint16_t func_id);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_COMPAT_MATRIX_H__ */
