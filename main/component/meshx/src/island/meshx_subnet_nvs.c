/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_subnet_nvs.c
 * @brief Persistent NVS Storage for Island Subnet Coexistence (< 1.0 KB Total Footprint).
 */

#include <string.h>
#include "island/meshx_subnet_nvs.h"

extern meshx_err_t meshx_nvs_get(char const *key, void *blob, uint16_t blob_size);
extern meshx_err_t meshx_nvs_set(char const *key, void const *blob, uint16_t blob_size, bool arm_timer);
extern meshx_err_t meshx_nvs_commit(void);

#define NVS_KEY_KBRIDGE "sn_kbrg"
#define NVS_KEY_NONCE   "sn_nonc"
#define NVS_KEY_NAT     "sn_nat"
#define NVS_KEY_PAIRS   "sl_pair"
#define NVS_NAT_CHUNK_CAP (MESHX_NAT_TIER2_CAP / 2U)

typedef struct MESHX_PACKED
{
    uint8_t count;
    meshx_nat_entry_t entries[NVS_NAT_CHUNK_CAP];
} nvs_nat_chunk_t;

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(meshx_nvs_kbridge_blob_t) + sizeof(meshx_nvs_nonce_blob_t) +
                       sizeof(meshx_nvs_nat_blob_t) + sizeof(meshx_nvs_pairs_blob_t) <
                   MESHX_SUBNET_NVS_MAX_BUDGET,
               "Total NVS blob footprint must remain under 1 KB");
#endif

/**
 * @brief Default NVS reader delegating to meshx_nvs_get.
 *
 * @param[in]  key  NVS key identifier string.
 * @param[out] buf  Destination buffer.
 * @param[in]  size Number of bytes to read.
 * @return MESHX_SUCCESS on success, or platform error code.
 */
static meshx_err_t default_nvs_read(const char *key, void *buf, uint16_t size)
{
    return meshx_nvs_get(key, buf, size);
}

/**
 * @brief Default NVS writer delegating to meshx_nvs_set with deferred flash timer.
 *
 * @param[in] key  NVS key identifier string.
 * @param[in] buf  Source buffer.
 * @param[in] size Number of bytes to write.
 * @return MESHX_SUCCESS on success, or platform error code.
 */
static meshx_err_t default_nvs_write(const char *key, const void *buf, uint16_t size)
{
    return meshx_nvs_set(key, buf, size, true);
}

static meshx_subnet_nvs_ops_t s_ops =
{
    .read_fn    = default_nvs_read,
    .write_fn   = default_nvs_write,
    .commit_fn  = meshx_nvs_commit,
};

/**
 * @brief Override default NVS operations table (used primarily by host tests).
 *
 * @param[in] ops Pointer to custom NVS operations table, or NULL to reset to defaults.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if @p ops is non-NULL with NULL callbacks.
 */
meshx_err_t meshx_subnet_nvs_set_ops(const meshx_subnet_nvs_ops_t *ops)
{
    if (!ops)
    {
        s_ops.read_fn = default_nvs_read;
        s_ops.write_fn = default_nvs_write;
        s_ops.commit_fn = meshx_nvs_commit;
        return MESHX_SUCCESS;
    }
    if (!ops->read_fn || !ops->write_fn || !ops->commit_fn)
    {
        return MESHX_INVALID_ARG;
    }
    s_ops = *ops;
    return MESHX_SUCCESS;
}

/**
 * @brief Retrieve pointer to current active NVS operations table.
 *
 * @param[out] out_ops Destination for const pointer to active operations structure.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if @p out_ops is NULL.
 */
meshx_err_t meshx_subnet_nvs_get_ops(const meshx_subnet_nvs_ops_t **out_ops)
{
    if (!out_ops)
    {
        return MESHX_INVALID_ARG;
    }
    *out_ops = &s_ops;
    return MESHX_SUCCESS;
}

/**
 * @brief Helper to write a key-value blob and immediately commit to persistent flash.
 *
 * @param[in] key  NVS key identifier string.
 * @param[in] buf  Source payload buffer.
 * @param[in] size Number of bytes to write.
 * @return MESHX_SUCCESS on success, or write/commit error code.
 */
static meshx_err_t nvs_write_commit(const char *key, const void *buf, uint16_t size)
{
    meshx_err_t err = s_ops.write_fn(key, buf, size);
    if (err != MESHX_SUCCESS)
    {
        return err;
    }
    return s_ops.commit_fn();
}

/**
 * @brief Persist Cluster Bridge Key record into NVS.
 *
 * @param[in] blob Pointer to K_bridge record.
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG or NVS error.
 */
meshx_err_t meshx_subnet_nvs_save_kbridge(const meshx_nvs_kbridge_blob_t *blob)
{
    if (!blob)
    {
        return MESHX_INVALID_ARG;
    }
    return nvs_write_commit(NVS_KEY_KBRIDGE, blob, (uint16_t)sizeof(*blob));
}

/**
 * @brief Erase the persisted Cluster Bridge Key (K_bridge) blob from NVS.
 *
 * @return MESHX_SUCCESS on success, or NVS error code.
 */
meshx_err_t meshx_subnet_nvs_erase_kbridge(void)
{
    meshx_nvs_kbridge_blob_t blank;
    memset(&blank, 0, sizeof(blank));
    return nvs_write_commit(NVS_KEY_KBRIDGE, &blank, (uint16_t)sizeof(blank));
}

/**
 * @brief Load Cluster Bridge Key record from NVS.
 *
 * @param[out] blob Destination buffer for K_bridge record.
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG or NVS error.
 */
meshx_err_t meshx_subnet_nvs_load_kbridge(meshx_nvs_kbridge_blob_t *blob)
{
    if (!blob)
    {
        return MESHX_INVALID_ARG;
    }
    return s_ops.read_fn(NVS_KEY_KBRIDGE, blob, (uint16_t)sizeof(*blob));
}

/**
 * @brief Persist bridge anti-replay nonce checkpoint into NVS.
 *
 * @param[in] nonce_ckpt Monotonic nonce checkpoint value.
 * @return MESHX_SUCCESS on success, or NVS write error.
 */
meshx_err_t meshx_subnet_nvs_save_nonce(uint32_t nonce_ckpt)
{
    meshx_nvs_nonce_blob_t blob;
    blob.nonce_checkpoint = nonce_ckpt;
    return nvs_write_commit(NVS_KEY_NONCE, &blob, (uint16_t)sizeof(blob));
}

/**
 * @brief Load bridge anti-replay nonce checkpoint from NVS.
 *
 * @param[out] out_nonce_ckpt Destination for loaded checkpoint value.
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG or NVS error.
 */
meshx_err_t meshx_subnet_nvs_load_nonce(uint32_t *out_nonce_ckpt)
{
    if (!out_nonce_ckpt)
    {
        return MESHX_INVALID_ARG;
    }
    meshx_nvs_nonce_blob_t blob;
    memset(&blob, 0, sizeof(blob));
    meshx_err_t err = s_ops.read_fn(NVS_KEY_NONCE, &blob, (uint16_t)sizeof(blob));
    if (err == MESHX_SUCCESS)
    {
        *out_nonce_ckpt = blob.nonce_checkpoint;
    }
    return err;
}

/**
 * @brief Persist Tier-2 NAT mappings into NVS.
 *
 * @param[in] entries Pointer to array of NAT entries.
 * @param[in] count   Number of active entries to save.
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG or NVS error.
 */
meshx_err_t meshx_subnet_nvs_save_nat_table(const meshx_nat_entry_t *entries, uint8_t count)
{
    if (!entries && count > 0U)
    {
        return MESHX_INVALID_ARG;
    }
    uint8_t total = (count > MESHX_NAT_TIER2_CAP) ? MESHX_NAT_TIER2_CAP : count;
    const char *keys[2] = {"sl_nat0", "sl_nat1"};
    for (uint8_t c = 0U; c < 2U; c++)
    {
        nvs_nat_chunk_t chunk;
        memset(&chunk, 0, sizeof(chunk));
        uint8_t offset = (uint8_t)(c * NVS_NAT_CHUNK_CAP);
        uint8_t rem = (total > offset) ? (uint8_t)(total - offset) : 0U;
        chunk.count = (rem > NVS_NAT_CHUNK_CAP) ? NVS_NAT_CHUNK_CAP : rem;
        if (chunk.count > 0U)
        {
            memcpy(chunk.entries, &entries[offset], (size_t)chunk.count * sizeof(meshx_nat_entry_t));
        }
        meshx_err_t err = nvs_write_commit(keys[c], &chunk, (uint16_t)sizeof(chunk));
        if (err != MESHX_SUCCESS)
        {
            return err;
        }
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Load Tier-2 NAT mappings from NVS.
 *
 * @param[out] entries   Destination array (capacity >= MESHX_NAT_TIER2_CAP).
 * @param[out] out_count Number of entries loaded.
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG or NVS error.
 */
meshx_err_t meshx_subnet_nvs_load_nat_table(meshx_nat_entry_t *entries, uint8_t *out_count)
{
    if (!entries || !out_count)
    {
        return MESHX_INVALID_ARG;
    }
    *out_count = 0U;
    const char *keys[2] = {"sl_nat0", "sl_nat1"};
    uint8_t total = 0U;
    for (uint8_t c = 0U; c < 2U; c++)
    {
        nvs_nat_chunk_t chunk;
        memset(&chunk, 0, sizeof(chunk));
        meshx_err_t err = s_ops.read_fn(keys[c], &chunk, (uint16_t)sizeof(chunk));
        if (err == MESHX_NOT_FOUND)
        {
            continue;
        }
        if (err != MESHX_SUCCESS)
        {
            return err;
        }
        uint8_t copy = (chunk.count > NVS_NAT_CHUNK_CAP) ? NVS_NAT_CHUNK_CAP : chunk.count;
        if (copy > 0U)
        {
            memcpy(&entries[total], chunk.entries, (size_t)copy * sizeof(meshx_nat_entry_t));
            total = (uint8_t)(total + copy);
        }
    }
    *out_count = total;
    return MESHX_SUCCESS;
}

/**
 * @brief Persist cross-subnet pair records into NVS.
 *
 * @param[in] records Pointer to array of pair records.
 * @param[in] count   Number of active records to save.
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG or NVS error.
 */
meshx_err_t meshx_subnet_nvs_save_pair_records(const meshx_pair_record_t *records, uint8_t count)
{
    if (!records && count > 0U)
    {
        return MESHX_INVALID_ARG;
    }
    meshx_nvs_pairs_blob_t blob;
    memset(&blob, 0, sizeof(blob));
    uint8_t copy = (count > MESHX_PAIR_RECORD_MAX) ? MESHX_PAIR_RECORD_MAX : count;
    blob.count = copy;
    if (copy > 0U)
    {
        memcpy(blob.records, records, (size_t)copy * sizeof(meshx_pair_record_t));
    }
    return nvs_write_commit(NVS_KEY_PAIRS, &blob, (uint16_t)sizeof(blob));
}

/**
 * @brief Load cross-subnet pair records from NVS.
 *
 * @param[out] records   Destination array (capacity >= MESHX_PAIR_RECORD_MAX).
 * @param[out] out_count Number of records loaded.
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG or NVS error.
 */
meshx_err_t meshx_subnet_nvs_load_pair_records(meshx_pair_record_t *records, uint8_t *out_count)
{
    if (!records || !out_count)
    {
        return MESHX_INVALID_ARG;
    }
    meshx_nvs_pairs_blob_t blob;
    memset(&blob, 0, sizeof(blob));
    meshx_err_t err = s_ops.read_fn(NVS_KEY_PAIRS, &blob, (uint16_t)sizeof(blob));
    if (err != MESHX_SUCCESS)
    {
        *out_count = 0U;
        return err;
    }
    uint8_t copy = (blob.count > MESHX_PAIR_RECORD_MAX) ? MESHX_PAIR_RECORD_MAX : blob.count;
    if (copy > 0U)
    {
        memcpy(records, blob.records, (size_t)copy * sizeof(meshx_pair_record_t));
    }
    *out_count = copy;
    return MESHX_SUCCESS;
}
