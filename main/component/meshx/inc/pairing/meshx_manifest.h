/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_manifest.h
 * @brief Capability Manifest and Introspection Data Structures.
 *
 * Provides self-describing introspection for MeshX nodes, detailing active
 * baked elements, logical model capabilities, roles, traits, and bound func_ids.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 */

#ifndef __MESHX_MANIFEST_H__
#define __MESHX_MANIFEST_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "meshx_common.h"
#include "meshx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MESHX_MAX_MANIFEST_ELEMENTS     8
#define MESHX_MAX_FUNC_IDS_PER_ELEM     4
#define MESHX_LABEL_MAX_LEN             16

/**
 * @brief Element operational role in the pairing graph.
 */
typedef enum {
    MESHX_ROLE_UNKNOWN       = 0x00,
    MESHX_ROLE_CONSUMER      = 0x01, /**< Server / Actuator / Sink (receives commands) */
    MESHX_ROLE_PRODUCER      = 0x02, /**< Client / Sensor / Source (issues commands/telemetry) */
    MESHX_ROLE_BIDIRECTIONAL = 0x03, /**< Peer / Controller-Actuator hybrid */
} meshx_element_role_t;

/**
 * @brief High-level semantic traits recognized by the compatibility engine.
 */
typedef enum {
    MESHX_TRAIT_UNKNOWN      = 0x00,
    MESHX_TRAIT_RELAY_POWER  = 0x01, /**< On/Off Power switching (func_id 0x00) */
    MESHX_TRAIT_LIGHT_CWWW   = 0x02, /**< On/Off + Color Temperature CTL (func_id 0x00, 0x01) */
    MESHX_TRAIT_LIGHT_HSL    = 0x03, /**< On/Off + Hue-Saturation-Lightness (func_id 0x00, 0x01) */
    MESHX_TRAIT_SENSOR_DATA  = 0x04, /**< Environmental telemetry streams (func_id 0x00) */
} meshx_trait_type_t;

/**
 * @brief Descriptor for a single baked element on the node.
 */
typedef struct {
    uint8_t  element_idx;
    uint8_t  role;                                  /**< meshx_element_role_t */
    uint16_t variant;                               /**< meshx_element_type_t */
    uint16_t trait;                                 /**< meshx_trait_type_t */
    uint8_t  func_count;
    uint16_t func_ids[MESHX_MAX_FUNC_IDS_PER_ELEM];
    char     label[MESHX_LABEL_MAX_LEN];            /**< Human readable label, e.g. "Switch 1" */
} MESHX_PACKED meshx_element_descriptor_t;

/**
 * @brief Full device capability manifest.
 */
typedef struct {
    uint16_t                   unicast_addr;
    uint16_t                   company_id;
    uint16_t                   product_id;
    uint16_t                   link_epoch;          /**< Generation counter for link changes */
    uint8_t                    element_count;
    meshx_element_descriptor_t elements[MESHX_MAX_MANIFEST_ELEMENTS];
} MESHX_PACKED meshx_capability_manifest_t;

/**
 * @brief Resolve operational role and semantic trait from element variant.
 *
 * @param[in]  variant   Element variant (meshx_element_type_t).
 * @param[out] out_role  Pointer to store resolved role (meshx_element_role_t).
 * @param[out] out_trait Pointer to store resolved trait (meshx_trait_type_t).
 */
void meshx_manifest_resolve_role_trait(uint16_t variant, uint8_t *out_role, uint16_t *out_trait);

/**
 * @brief Build local capability manifest by introspecting registered elements and models.
 *
 * @param[out] out_manifest Pointer to manifest structure to populate.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_manifest_build_local(meshx_capability_manifest_t *out_manifest);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_MANIFEST_H__ */
