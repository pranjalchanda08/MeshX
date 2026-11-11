/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_subnet_nvs.c
 * @brief Persistent NVS Storage for Island Subnet Coexistence (< 1.0 KB Total Footprint).
 */

#include <string.h>
#include "island_subnet_coex/meshx_subnet_nvs.h"

extern meshx_err_t meshx_nvs_get(char const *key, void *blob, uint16_t blob_size);
extern meshx_err_t meshx_nvs_set(char const *key, void const *blob, uint16_t blob_size, bool arm_timer);
extern meshx_err_t meshx_nvs_commit(void);

#define NVS_KEY_KBRIDGE "sn_kbrg"
#define NVS_KEY_NONCE   "sn_nonc"
#define NVS_KEY_NAT     "sn_nat"
#define NVS_KEY_PAIRS   "sl_pair"

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(meshx_nvs_kbridge_blob_t) + sizeof(meshx_nvs_nonce_blob_t) +
               sizeof(meshx_nvs_nat_blob_t) + sizeof(meshx_nvs_pairs_blob_t) < 1024U,
               "Total NVS blob footprint must remain under 1 KB");
#endif

static meshx_err_t default_nvs_read(const char *key, void *buf, uint16_t size)
{
    return meshx_nvs_get(key, buf, size);
}

static meshx_err_t default_nvs_write(const char *key, const void *buf, uint16_t size)
{
    return meshx_nvs_set(key, buf, size, true);
}

static meshx_subnet_nvs_ops_t s_ops = {
    .read_fn   = default_nvs_read,
    .write_fn  = default_nvs_write,
    .commit_fn = meshx_nvs_commit,
};

void meshx_subnet_nvs_set_ops(const meshx_subnet_nvs_ops_t *ops)
{
    if (ops && ops->read_fn && ops->write_fn && ops->commit_fn) {
        s_ops = *ops;
    } else {
        s_ops.read_fn   = default_nvs_read;
        s_ops.write_fn  = default_nvs_write;
        s_ops.commit_fn = meshx_nvs_commit;
    }
}

const meshx_subnet_nvs_ops_t *meshx_subnet_nvs_get_ops(void)
{
    return &s_ops;
}

static meshx_err_t nvs_write_commit(const char *key, const void *buf, uint16_t size)
{
    meshx_err_t err = s_ops.write_fn(key, buf, size);
    if (err != MESHX_SUCCESS) {
        return err;
    }
    return s_ops.commit_fn();
}

meshx_err_t meshx_subnet_nvs_save_kbridge(const meshx_nvs_kbridge_blob_t *blob)
{
    if (!blob) {
        return MESHX_INVALID_ARG;
    }
    return nvs_write_commit(NVS_KEY_KBRIDGE, blob, (uint16_t)sizeof(*blob));
}

meshx_err_t meshx_subnet_nvs_load_kbridge(meshx_nvs_kbridge_blob_t *blob)
{
    if (!blob) {
        return MESHX_INVALID_ARG;
    }
    return s_ops.read_fn(NVS_KEY_KBRIDGE, blob, (uint16_t)sizeof(*blob));
}

meshx_err_t meshx_subnet_nvs_save_nonce(uint32_t nonce_ckpt)
{
    meshx_nvs_nonce_blob_t blob;
    blob.nonce_checkpoint = nonce_ckpt;
    return nvs_write_commit(NVS_KEY_NONCE, &blob, (uint16_t)sizeof(blob));
}

meshx_err_t meshx_subnet_nvs_load_nonce(uint32_t *out_nonce_ckpt)
{
    if (!out_nonce_ckpt) {
        return MESHX_INVALID_ARG;
    }
    meshx_nvs_nonce_blob_t blob;
    memset(&blob, 0, sizeof(blob));
    meshx_err_t err = s_ops.read_fn(NVS_KEY_NONCE, &blob, (uint16_t)sizeof(blob));
    if (err == MESHX_SUCCESS) {
        *out_nonce_ckpt = blob.nonce_checkpoint;
    }
    return err;
}

meshx_err_t meshx_subnet_nvs_save_nat_table(const meshx_nat_entry_t *entries, uint8_t count)
{
    if (!entries && count > 0U) {
        return MESHX_INVALID_ARG;
    }
    meshx_nvs_nat_blob_t blob;
    memset(&blob, 0, sizeof(blob));
    uint8_t copy = (count > MESHX_NAT_TIER2_CAP) ? MESHX_NAT_TIER2_CAP : count;
    blob.count = copy;
    if (copy > 0U) {
        memcpy(blob.entries, entries, (size_t)copy * sizeof(meshx_nat_entry_t));
    }
    return nvs_write_commit(NVS_KEY_NAT, &blob, (uint16_t)sizeof(blob));
}

meshx_err_t meshx_subnet_nvs_load_nat_table(meshx_nat_entry_t *entries, uint8_t *out_count)
{
    if (!entries || !out_count) {
        return MESHX_INVALID_ARG;
    }
    meshx_nvs_nat_blob_t blob;
    memset(&blob, 0, sizeof(blob));
    meshx_err_t err = s_ops.read_fn(NVS_KEY_NAT, &blob, (uint16_t)sizeof(blob));
    if (err != MESHX_SUCCESS) {
        *out_count = 0U;
        return err;
    }
    uint8_t copy = (blob.count > MESHX_NAT_TIER2_CAP) ? MESHX_NAT_TIER2_CAP : blob.count;
    if (copy > 0U) {
        memcpy(entries, blob.entries, (size_t)copy * sizeof(meshx_nat_entry_t));
    }
    *out_count = copy;
    return MESHX_SUCCESS;
}

meshx_err_t meshx_subnet_nvs_save_pair_records(const meshx_pair_record_t *records, uint8_t count)
{
    if (!records && count > 0U) {
        return MESHX_INVALID_ARG;
    }
    meshx_nvs_pairs_blob_t blob;
    memset(&blob, 0, sizeof(blob));
    uint8_t copy = (count > MESHX_PAIR_RECORD_MAX) ? MESHX_PAIR_RECORD_MAX : count;
    blob.count = copy;
    if (copy > 0U) {
        memcpy(blob.records, records, (size_t)copy * sizeof(meshx_pair_record_t));
    }
    return nvs_write_commit(NVS_KEY_PAIRS, &blob, (uint16_t)sizeof(blob));
}

meshx_err_t meshx_subnet_nvs_load_pair_records(meshx_pair_record_t *records, uint8_t *out_count)
{
    if (!records || !out_count) {
        return MESHX_INVALID_ARG;
    }
    meshx_nvs_pairs_blob_t blob;
    memset(&blob, 0, sizeof(blob));
    meshx_err_t err = s_ops.read_fn(NVS_KEY_PAIRS, &blob, (uint16_t)sizeof(blob));
    if (err != MESHX_SUCCESS) {
        *out_count = 0U;
        return err;
    }
    uint8_t copy = (blob.count > MESHX_PAIR_RECORD_MAX) ? MESHX_PAIR_RECORD_MAX : blob.count;
    if (copy > 0U) {
        memcpy(records, blob.records, (size_t)copy * sizeof(meshx_pair_record_t));
    }
    *out_count = copy;
    return MESHX_SUCCESS;
}

