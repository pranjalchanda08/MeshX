/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_kv_engine.c
 * @brief Enhanced implementation for MeshX Portable Key-Value Engine with GC.
 *
 * @author Pranjal Chanda
 */

#include "meshx_kv_engine.h"
#include "interface/logging/meshx_log.h"
#include <string.h>
#include <stdlib.h>

#define KV_MAGIC            0x4D58  /**< "MX" */
#define KV_STATUS_VALID     0xEE
#define KV_STATUS_OBSOLETE  0x00
#define KV_STATUS_EMPTY     0xFF

#define KV_MAX_KEY_LEN      32
#define KV_ALIGN_SIZE       4

/**
 * @struct kv_record_header
 * @brief Header for each record stored in flash.
 */
typedef struct {
    uint16_t magic;
    uint8_t  status;
    uint8_t  key_len;
    uint16_t val_len;
    uint32_t seq_id;
    uint16_t crc;
} __attribute__((packed)) kv_header_t;

typedef struct kv_pending_node {
    char key[KV_MAX_KEY_LEN];
    uint8_t *data;
    uint16_t len;
    struct kv_pending_node *next;
} kv_pending_t;

static const meshx_fal_partition_t *kv_part = NULL;
static uint32_t kv_write_pos = 0;
static uint32_t kv_next_seq_id = 0;
static kv_pending_t *pending_list = NULL;

static uint16_t kv_calc_crc(const uint8_t *data, uint32_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xA001;
            else crc >>= 1;
        }
    }
    return crc;
}

static uint32_t kv_align(uint32_t size)
{
    return (size + (KV_ALIGN_SIZE - 1)) & ~(KV_ALIGN_SIZE - 1);
}

meshx_err_t meshx_kv_engine_init(const meshx_fal_partition_t *part)
{
    if (!part) return MESHX_INVALID_ARG;
    kv_part = part;
    kv_write_pos = 0;
    kv_next_seq_id = 1;

    // Scan to find write head and max seq_id
    kv_header_t header;
    while (kv_write_pos + sizeof(header) <= kv_part->size) {
        meshx_err_t err = meshx_fal_read(kv_part, kv_write_pos, &header, sizeof(header));
        if (err) return err;

        if (header.magic != KV_MAGIC || header.status == KV_STATUS_EMPTY) {
            break;
        }

        if (header.key_len > KV_MAX_KEY_LEN || header.val_len > 256) {
            MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "KV Init: Invalid lengths at pos %u (key:%d, val:%d)", kv_write_pos, header.key_len, header.val_len);
            break;
        }

        if (header.seq_id >= kv_next_seq_id) {
            kv_next_seq_id = header.seq_id + 1;
        }

        char kbuf[KV_MAX_KEY_LEN];
        meshx_fal_read(kv_part, kv_write_pos + sizeof(header), kbuf, header.key_len < KV_MAX_KEY_LEN ? header.key_len : KV_MAX_KEY_LEN-1);
        kbuf[header.key_len < KV_MAX_KEY_LEN ? header.key_len : KV_MAX_KEY_LEN-1] = '\0';
#ifdef KV_DEBUG_EN
        MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_NVS, "  Found Record: key=%s, seq=%u, pos=%u", kbuf, header.seq_id, kv_write_pos);
#endif /* KV_DEBUG_EN */
        kv_write_pos += kv_align(sizeof(header) + header.key_len + header.val_len);
    }

    MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_NVS, "KV Engine Init: Pos=%u, SeqID=%u", kv_write_pos, kv_next_seq_id);
    return MESHX_SUCCESS;
}

meshx_err_t meshx_kv_engine_read(const char *key, void *buf, uint16_t len)
{
    if (!kv_part || !key || !buf) return MESHX_INVALID_ARG;

    // Check pending writes first (most recent)
    kv_pending_t *pending = pending_list;
    while (pending) {
        if (strncmp(pending->key, key, KV_MAX_KEY_LEN - 1) == 0) {
            uint16_t to_copy = (len < pending->len) ? len : pending->len;
            memcpy(buf, pending->data, to_copy);
            return MESHX_SUCCESS;
        }
        pending = pending->next;
    }

    uint32_t pos = 0;
    kv_header_t header;
    uint32_t best_pos = 0xFFFFFFFF;
    uint32_t max_seq = 0;
    uint16_t found_val_len = 0;

    while (pos < kv_write_pos) {
        if (meshx_fal_read(kv_part, pos, &header, sizeof(header)) != MESHX_SUCCESS) break;
        if (header.magic != KV_MAGIC) break;

        if (header.status == KV_STATUS_VALID && header.key_len == strlen(key)) {
            char found_key[KV_MAX_KEY_LEN];
            meshx_fal_read(kv_part, pos + sizeof(header), found_key, header.key_len);
            if (strncmp(found_key, key, header.key_len) == 0) {
                if (header.seq_id >= max_seq) {
                    max_seq = header.seq_id;
                    best_pos = pos;
                    found_val_len = header.val_len;
                }
            }
        }
        pos += kv_align(sizeof(header) + header.key_len + header.val_len);
    }

    if (best_pos != 0xFFFFFFFF) {
        uint16_t to_read = (len < found_val_len) ? len : found_val_len;
        kv_header_t h;
        meshx_fal_read(kv_part, best_pos, &h, sizeof(h));
        meshx_fal_read(kv_part, best_pos + sizeof(h) + h.key_len, buf, to_read);
        MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_NVS, "KV Read Success: key=%s, seq=%u, len=%d", key, max_seq, to_read);
        return MESHX_SUCCESS;
    }

    MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_NVS, "KV Read Not Found: key=%s", key);
    return MESHX_NOT_FOUND;
}

meshx_err_t meshx_kv_engine_set(const char *key, const void *buf, uint16_t len)
{
    kv_pending_t *node = malloc(sizeof(kv_pending_t));
    if (!node) return MESHX_NO_MEM;
    strncpy(node->key, key, KV_MAX_KEY_LEN - 1);
    node->data = malloc(len);
    memcpy(node->data, buf, len);
    node->len = len;
    node->next = pending_list;
    pending_list = node;
    return MESHX_SUCCESS;
}

static meshx_err_t kv_engine_gc(void)
{
    // Simplified GC: Erase all and re-write only the latest values
    // In a production system, this would move data sector by sector
    MESHX_LOGW(MODULE_ID_COMPONENT_MESHX_NVS, "KV GC triggered (Simplified)");

    // 1. Scan and find latest version of all unique keys (requires RAM or multiple scans)
    // For now, we'll just return error to signify "Partition Full" until we implement full GC
    return MESHX_FAIL;
}

meshx_err_t meshx_kv_engine_commit(void)
{
    kv_pending_t *cur = pending_list;
    while (cur) {
        kv_header_t header = {
            .magic = KV_MAGIC,
            .status = KV_STATUS_VALID,
            .key_len = (uint8_t)strlen(cur->key),
            .val_len = cur->len,
            .seq_id = kv_next_seq_id++,
            .crc = kv_calc_crc(cur->data, cur->len)
        };

        uint32_t size = kv_align(sizeof(header) + header.key_len + header.val_len);
        if (kv_write_pos + size > kv_part->size) {
            meshx_err_t err = kv_engine_gc();
            if (err != MESHX_SUCCESS) return err;
        }

        meshx_fal_write(kv_part, kv_write_pos, &header, sizeof(header));
        meshx_fal_write(kv_part, kv_write_pos + sizeof(header), cur->key, header.key_len);
        meshx_fal_write(kv_part, kv_write_pos + sizeof(header) + header.key_len, cur->data, header.val_len);

        // Verification
        kv_header_t v_header;
        meshx_fal_read(kv_part, kv_write_pos, &v_header, sizeof(v_header));
        if (v_header.magic != KV_MAGIC) {
            MESHX_LOGE(MODULE_ID_COMPONENT_MESHX_NVS, "KV Verify FAILED at pos %u! (read: 0x%04x)", kv_write_pos, v_header.magic);
        }

        MESHX_LOGD(MODULE_ID_COMPONENT_MESHX_NVS, "KV Commit: key=%s, seq=%u, pos=%u", cur->key, header.seq_id, kv_write_pos);
        kv_write_pos += size;

        kv_pending_t *prev = cur;
        cur = cur->next;
        free(prev->data);
        free(prev);
    }
    pending_list = NULL;
    return MESHX_SUCCESS;
}

meshx_err_t meshx_kv_engine_remove(const char *key)
{
    // Removing in log-structured means appending a "Deleted" record or marking status
    // Here we append a record with status OBSOLETE or just set length to 0
    uint8_t dummy = 0;
    return meshx_kv_engine_set(key, &dummy, 0);
    // The next read will find this (seq_id will be higher) and we can treat len=0 as deleted
}

meshx_err_t meshx_kv_engine_erase_all(void)
{
    meshx_err_t err = meshx_fal_erase(kv_part, 0, kv_part->size);
    if (err == MESHX_SUCCESS) {
        kv_write_pos = 0;
        kv_next_seq_id = 1;
    }
    return err;
}
