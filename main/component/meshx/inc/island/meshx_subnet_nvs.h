/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_subnet_nvs.h
 * @brief Persistent NVS Storage for Island Subnet Coexistence (< 1.0 KB Total Footprint).
 */

#ifndef MESHX_SUBNET_NVS_H
#define MESHX_SUBNET_NVS_H

#include "island/meshx_subnet_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exact persisted NVS binary footprint:
 * - "sn_kbrg":           16B key + 4B cluster_epoch + 4B active_term + 1B valid = 25B
 * - "sn_nonc":            4B bridge_nonce_checkpoint                             =  4B
 * - "sl_nat0"/"sl_nat1": 2 * (1B count + 8 * 27B meshx_nat_entry_t)              = 434B
 * - "sl_pair":           1B count + 8 * 20B meshx_pair_record_t                  = 161B
 * Total <= 624 B (< 1,024 B budget) */

/**
 * @brief Persistent K_bridge cluster key record stored in NVS ("sn_kbrg").
 */
typedef struct MESHX_PACKED {
    meshx_subnet_key_t k_bridge;      /**< 16-byte Cluster Bridge Key */
    uint32_t           cluster_epoch; /**< Active cluster epoch counter */
    uint32_t           active_term;   /**< Active failover term */
    uint8_t            valid;         /**< 1 if record is valid, 0 otherwise */
} meshx_nvs_kbridge_blob_t;

/**
 * @brief Persistent bridge nonce checkpoint stored in NVS ("sn_nonc").
 */
typedef struct MESHX_PACKED {
    uint32_t nonce_checkpoint;        /**< Nonce checkpoint advanced in steps of 1024 */
} meshx_nvs_nonce_blob_t;

/**
 * @brief Persistent Tier-2 NAT table container stored in NVS ("sn_nat").
 */
typedef struct MESHX_PACKED {
    uint8_t           count;                            /**< Number of active NAT entries */
    meshx_nat_entry_t entries[MESHX_NAT_TIER2_CAP];     /**< Array of persisted NAT records */
} meshx_nvs_nat_blob_t;

/**
 * @brief Persistent cross-subnet pair record container stored in NVS ("sl_pair").
 */
typedef struct MESHX_PACKED {
    uint8_t             count;                          /**< Number of active pair records */
    meshx_pair_record_t records[MESHX_PAIR_RECORD_MAX]; /**< Array of persisted pair records */
} meshx_nvs_pairs_blob_t;

/**
 * @brief Abstract NVS read/write/commit operations (defaults to meshx_nvs_*, overridable in host tests).
 */
typedef struct {
    meshx_err_t (*read_fn)(const char *key, void *buf, uint16_t size);       /**< Read binary blob from NVS */
    meshx_err_t (*write_fn)(const char *key, const void *buf, uint16_t size); /**< Write binary blob to NVS */
    meshx_err_t (*commit_fn)(void);                                          /**< Commit pending writes to flash */
} meshx_subnet_nvs_ops_t;

/**
 * @brief Override default NVS operations (used primarily by host tests).
 *
 * @param[in] ops Pointer to custom NVS operations table, or NULL to reset to defaults.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if @p ops is non-NULL with NULL callbacks.
 */
meshx_err_t meshx_subnet_nvs_set_ops(const meshx_subnet_nvs_ops_t *ops);

/**
 * @brief Get current active NVS operations table.
 *
 * @param[out] out_ops Destination for pointer to active operations structure.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if @p out_ops is NULL.
 */
meshx_err_t meshx_subnet_nvs_get_ops(const meshx_subnet_nvs_ops_t **out_ops);

/**
 * @brief Save Cluster Bridge Key blob to NVS.
 *
 * @param[in] blob Pointer to K_bridge record to persist.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_nvs_save_kbridge(const meshx_nvs_kbridge_blob_t *blob);

/**
 * @brief Erase Cluster Bridge Key record from NVS on revocation.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_nvs_erase_kbridge(void);

/**
 * @brief Load Cluster Bridge Key blob from NVS.
 *
 * @param[out] blob Destination buffer for loaded record.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_nvs_load_kbridge(meshx_nvs_kbridge_blob_t *blob);

/**
 * @brief Save monotonic bridge nonce checkpoint to NVS.
 *
 * @param[in] nonce_ckpt Current checkpoint counter.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_nvs_save_nonce(uint32_t nonce_ckpt);

/**
 * @brief Load monotonic bridge nonce checkpoint from NVS.
 *
 * @param[out] out_nonce_ckpt Destination for loaded checkpoint counter.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_nvs_load_nonce(uint32_t *out_nonce_ckpt);

/**
 * @brief Save Tier-2 NAT table entries to NVS.
 *
 * @param[in] entries Pointer to array of NAT entries.
 * @param[in] count   Number of entries to persist.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_nvs_save_nat_table(const meshx_nat_entry_t *entries, uint8_t count);

/**
 * @brief Load Tier-2 NAT table entries from NVS.
 *
 * @param[out] entries   Destination array (capacity at least MESHX_NAT_TIER2_CAP).
 * @param[out] out_count Number of entries loaded.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_nvs_load_nat_table(meshx_nat_entry_t *entries, uint8_t *out_count);

/**
 * @brief Save cross-subnet pair records to NVS.
 *
 * @param[in] records Pointer to array of pair records.
 * @param[in] count   Number of records to persist.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_nvs_save_pair_records(const meshx_pair_record_t *records, uint8_t count);

/**
 * @brief Load cross-subnet pair records from NVS.
 *
 * @param[out] records   Destination array (capacity at least MESHX_PAIR_RECORD_MAX).
 * @param[out] out_count Number of records loaded.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_nvs_load_pair_records(meshx_pair_record_t *records, uint8_t *out_count);

#ifdef __cplusplus
}
#endif

#endif /* MESHX_SUBNET_NVS_H */
