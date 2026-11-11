/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_subnet_nvs.h
 * @brief Persistent NVS Storage for Island Subnet Coexistence (< 1.0 KB Total Footprint).
 */

#ifndef MESHX_SUBNET_NVS_H
#define MESHX_SUBNET_NVS_H

#include "island_subnet_coex/meshx_subnet_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exact persisted NVS binary footprint:
 * - "sn_kbrg":  16B key + 4B cluster_epoch + 4B active_term = 24B
 * - "sn_nonc":   4B bridge_nonce_checkpoint                   =  4B
 * - "sn_nat":   16 * 19B meshx_nat_entry_t                    = 304B
 * - "sl_pair":   8 * 20B meshx_pair_record_t                  = 160B
 * Total <= 492 B (plus metadata/slot ids < 796 B < 1,024 B budget) */

typedef struct MESHX_PACKED {
    uint8_t  k_bridge[16];
    uint32_t cluster_epoch;
    uint32_t active_term;
    uint8_t  valid;
} meshx_nvs_kbridge_blob_t;

typedef struct MESHX_PACKED {
    uint32_t nonce_checkpoint;
} meshx_nvs_nonce_blob_t;

typedef struct MESHX_PACKED {
    uint8_t           count;
    meshx_nat_entry_t entries[MESHX_NAT_TIER2_CAP];
} meshx_nvs_nat_blob_t;

typedef struct MESHX_PACKED {
    uint8_t             count;
    meshx_pair_record_t records[MESHX_PAIR_RECORD_MAX];
} meshx_nvs_pairs_blob_t;

/** Abstract NVS read/write/commit operations (defaults to meshx_nvs_*, overridable in host tests) */
typedef struct {
    meshx_err_t (*read_fn)(const char *key, void *buf, uint16_t size);
    meshx_err_t (*write_fn)(const char *key, const void *buf, uint16_t size);
    meshx_err_t (*commit_fn)(void);
} meshx_subnet_nvs_ops_t;

void meshx_subnet_nvs_set_ops(const meshx_subnet_nvs_ops_t *ops);
const meshx_subnet_nvs_ops_t *meshx_subnet_nvs_get_ops(void);

meshx_err_t meshx_subnet_nvs_save_kbridge(const meshx_nvs_kbridge_blob_t *blob);
meshx_err_t meshx_subnet_nvs_load_kbridge(meshx_nvs_kbridge_blob_t *blob);

meshx_err_t meshx_subnet_nvs_save_nonce(uint32_t nonce_ckpt);
meshx_err_t meshx_subnet_nvs_load_nonce(uint32_t *out_nonce_ckpt);

meshx_err_t meshx_subnet_nvs_save_nat_table(const meshx_nat_entry_t *entries, uint8_t count);
meshx_err_t meshx_subnet_nvs_load_nat_table(meshx_nat_entry_t *entries, uint8_t *out_count);

meshx_err_t meshx_subnet_nvs_save_pair_records(const meshx_pair_record_t *records, uint8_t count);
meshx_err_t meshx_subnet_nvs_load_pair_records(meshx_pair_record_t *records, uint8_t *out_count);

#ifdef __cplusplus
}
#endif

#endif /* MESHX_SUBNET_NVS_H */

