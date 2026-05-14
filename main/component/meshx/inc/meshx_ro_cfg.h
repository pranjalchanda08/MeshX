/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_ro_cfg.h
 * @brief Persistent Serialized Configuration Loader (Read-Only)
 */

#ifndef __MESHX_RO_CFG_H__
#define __MESHX_RO_CFG_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "meshx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize and load the MeshX configuration from the meshx_cfg partition.
 *
 * This function locates the `meshx_cfg` partition using FAL, validates the
 * CRC-16-CCITT checksums of the header and payload, decodes the Protobuf
 * payload using nanopb, and applies the configuration (e.g., global IDs, 
 * element composition, and GPIO bindings).
 * @param[out] cid Pointer to store the loaded Company ID.
 * @param[out] pid Pointer to store the loaded Product ID.
 * @param[out] product_name Buffer to store the loaded product name.
 * @param[in]  name_max_len Maximum length of the product_name buffer.
 * @return meshx_err_t MESHX_SUCCESS on success, or an error code on failure.
 */
meshx_err_t meshx_ro_cfg_init(uint16_t *cid, uint16_t *pid, char *product_name, size_t name_max_len);

#ifdef __cplusplus
}
#endif

#endif // __MESHX_RO_CFG_H__
