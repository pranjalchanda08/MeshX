/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_subnet_security.c
 * @brief MitM Authentication, HMAC-SHA256 Envelope, Anti-Replay, Whitelist & Cross-Subnet RPL.
 */

#include <string.h>
#include "island_subnet_coex/meshx_subnet_security.h"
#include "interface/crypto/meshx_crypto.h"

static void wipe_sec_bytes(uint8_t *buf, size_t len)
{
    volatile uint8_t *vp = (volatile uint8_t *)buf;
    for (size_t i = 0; i < len; i++) {
        vp[i] = 0U;
    }
}

static bool ct_memcmp16(const uint8_t *a, const uint8_t *b)
{
    uint8_t diff = 0U;
    for (size_t i = 0; i < MESHX_HMAC_TAG_LEN; i++) {
        diff |= (uint8_t)(a[i] ^ b[i]);
    }
    return diff == 0U;
}

static void build_devkey_ccm_nonce(uint8_t opcode, uint16_t dst_addr, uint32_t nonce32, uint8_t out_nonce[13])
{
    memset(out_nonce, 0, 13U);
    out_nonce[0] = 0x02U;
    out_nonce[1] = opcode;
    out_nonce[2] = (uint8_t)((nonce32 >> 24) & 0xFFU);
    out_nonce[3] = (uint8_t)((nonce32 >> 16) & 0xFFU);
    out_nonce[4] = (uint8_t)((nonce32 >> 8) & 0xFFU);
    out_nonce[5] = (uint8_t)(nonce32 & 0xFFU);
    out_nonce[6] = (uint8_t)((dst_addr >> 8) & 0xFFU);
    out_nonce[7] = (uint8_t)(dst_addr & 0xFFU);
}

void meshx_subnet_sec_init(meshx_subnet_security_ctx_t *ctx,
                           uint32_t persisted_nonce_ckpt,
                           meshx_sec_ckpt_save_fn_t ckpt_cb,
                           void *ckpt_ctx)
{
    if (!ctx) {
        return;
    }
    memset(ctx, 0, sizeof(*ctx));
    ctx->ckpt_cb  = ckpt_cb;
    ctx->ckpt_ctx = ckpt_ctx;
    ctx->last_saved_nonce_ckpt = persisted_nonce_ckpt;
    if (persisted_nonce_ckpt <= (0xFFFFFFFFUL - MESHX_BRIDGE_NONCE_BOOT_MARGIN)) {
        ctx->tx_bridge_nonce = persisted_nonce_ckpt + MESHX_BRIDGE_NONCE_BOOT_MARGIN;
    } else {
        ctx->tx_bridge_nonce = MESHX_BRIDGE_NONCE_WRAP_LIMIT;
    }
}

void meshx_subnet_sec_deinit(meshx_subnet_security_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }
    wipe_sec_bytes(ctx->k_bridge, sizeof(ctx->k_bridge));
    memset(ctx, 0, sizeof(*ctx));
}

meshx_err_t meshx_subnet_sec_set_k_bridge(meshx_subnet_security_ctx_t *ctx,
                                          const uint8_t k_bridge[16],
                                          uint32_t cluster_epoch,
                                          uint32_t term)
{
    if (!ctx || !k_bridge) {
        return MESHX_INVALID_ARG;
    }
    memcpy(ctx->k_bridge, k_bridge, 16U);
    ctx->k_bridge_valid = true;
    ctx->cluster_epoch  = cluster_epoch;
    if (term > ctx->active_term) {
        ctx->active_term = term;
    }
    return MESHX_SUCCESS;
}

void meshx_subnet_sec_clear_k_bridge(meshx_subnet_security_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }
    wipe_sec_bytes(ctx->k_bridge, sizeof(ctx->k_bridge));
    ctx->k_bridge_valid = false;
}

meshx_err_t meshx_subnet_sec_sign_frame(meshx_subnet_security_ctx_t *ctx,
                                        uint8_t opcode,
                                        uint16_t sender_addr,
                                        const uint8_t *payload,
                                        uint16_t payload_len,
                                        uint8_t *out_frame,
                                        uint16_t out_cap,
                                        uint16_t *out_len)
{
    if (!ctx || !out_frame || !out_len || (payload_len > 0U && !payload)) {
        return MESHX_INVALID_ARG;
    }
    if (!ctx->k_bridge_valid) {
        return (meshx_err_t)MESHX_SUBNET_ERR_AUTH;
    }
    if (ctx->tx_bridge_nonce >= MESHX_BRIDGE_NONCE_WRAP_LIMIT) {
        return (meshx_err_t)MESHX_SUBNET_ERR_REPLAY;
    }
    uint16_t needed = (uint16_t)(MESHX_CLUSTER_AUTH_OVERHEAD + payload_len);
    if (out_cap < needed) {
        return MESHX_NO_MEM;
    }

    ctx->tx_bridge_nonce++;
    meshx_uvp_auth_hdr_t hdr;
    hdr.opcode       = opcode;
    hdr.term         = ctx->active_term;
    hdr.sender_addr  = sender_addr;
    hdr.bridge_nonce = ctx->tx_bridge_nonce;

    memcpy(out_frame, &hdr, sizeof(hdr));
    if (payload_len > 0U) {
        memcpy(out_frame + sizeof(hdr), payload, payload_len);
    }

    uint16_t mac_in_len = (uint16_t)(sizeof(hdr) + payload_len);
    uint8_t hmac_full[32];
    meshx_err_t err = meshx_crypto_hmac_sha256(ctx->k_bridge,
                                               sizeof(ctx->k_bridge),
                                               out_frame,
                                               mac_in_len,
                                               hmac_full);
    if (err != MESHX_SUCCESS) {
        return err;
    }
    memcpy(out_frame + mac_in_len, hmac_full, MESHX_HMAC_TAG_LEN);
    wipe_sec_bytes(hmac_full, sizeof(hmac_full));

    if ((ctx->tx_bridge_nonce - ctx->last_saved_nonce_ckpt) >= MESHX_BRIDGE_NONCE_CHKPT_STEP) {
        ctx->last_saved_nonce_ckpt = ctx->tx_bridge_nonce;
        if (ctx->ckpt_cb) {
            (void)ctx->ckpt_cb(ctx->tx_bridge_nonce, ctx->ckpt_ctx);
        }
    }

    *out_len = needed;
    return MESHX_SUCCESS;
}

static bool replay_check_and_accept(meshx_subnet_security_ctx_t *ctx,
                                    uint16_t sender_addr,
                                    uint32_t nonce)
{
    if (nonce == 0UL || nonce <= ctx->eviction_floor_nonce) {
        return false;
    }

    int match_idx = -1;
    int free_idx  = -1;
    int oldest_idx = 0;

    for (int i = 0; i < (int)MESHX_ANTI_REPLAY_CACHE_SIZE; i++) {
        const meshx_replay_sender_t *s = &ctx->replay_cache[i];
        if (s->valid) {
            if (s->sender_addr == sender_addr) {
                match_idx = i;
                break;
            }
            if (s->highest_nonce < ctx->replay_cache[oldest_idx].highest_nonce) {
                oldest_idx = i;
            }
        } else if (free_idx < 0) {
            free_idx = i;
        }
    }

    if (match_idx >= 0) {
        meshx_replay_sender_t *slot = &ctx->replay_cache[match_idx];
        if (nonce > slot->highest_nonce) {
            uint32_t delta = nonce - slot->highest_nonce;
            slot->seen_bitmap = (delta >= 16UL) ? 1U : (uint16_t)((slot->seen_bitmap << delta) | 1U);
            slot->highest_nonce = nonce;
            return true;
        }
        uint32_t diff = slot->highest_nonce - nonce;
        if (diff >= 16UL || (slot->seen_bitmap & (uint16_t)(1U << diff)) != 0U) {
            return false;
        }
        slot->seen_bitmap |= (uint16_t)(1U << diff);
        return true;
    }

    int dst_idx = free_idx;
    if (dst_idx < 0) {
        dst_idx = oldest_idx;
        if (ctx->replay_cache[dst_idx].highest_nonce > ctx->eviction_floor_nonce) {
            ctx->eviction_floor_nonce = ctx->replay_cache[dst_idx].highest_nonce;
        }
    }

    ctx->replay_cache[dst_idx].sender_addr   = sender_addr;
    ctx->replay_cache[dst_idx].highest_nonce = nonce;
    ctx->replay_cache[dst_idx].seen_bitmap   = 1U;
    ctx->replay_cache[dst_idx].valid         = true;
    return true;
}

meshx_err_t meshx_subnet_sec_verify_frame(meshx_subnet_security_ctx_t *ctx,
                                          const uint8_t *frame,
                                          uint16_t frame_len,
                                          uint8_t *out_opcode,
                                          uint32_t *out_term,
                                          uint16_t *out_sender,
                                          const uint8_t **out_payload,
                                          uint16_t *out_payload_len)
{
    if (!ctx || !frame || !out_opcode || !out_term || !out_sender ||
        !out_payload || !out_payload_len) {
        return MESHX_INVALID_ARG;
    }
    if (!ctx->k_bridge_valid || frame_len < MESHX_CLUSTER_AUTH_OVERHEAD) {
        return (meshx_err_t)MESHX_SUBNET_ERR_AUTH;
    }

    meshx_uvp_auth_hdr_t hdr;
    memcpy(&hdr, frame, sizeof(hdr));
    if (hdr.term < ctx->active_term) {
        return (meshx_err_t)MESHX_SUBNET_ERR_AUTH;
    }

    uint16_t mac_in_len = (uint16_t)(frame_len - MESHX_HMAC_TAG_LEN);
    uint8_t hmac_full[32];
    meshx_err_t err = meshx_crypto_hmac_sha256(ctx->k_bridge,
                                               sizeof(ctx->k_bridge),
                                               frame,
                                               mac_in_len,
                                               hmac_full);
    if (err != MESHX_SUCCESS) {
        return err;
    }
    bool tag_ok = ct_memcmp16(hmac_full, frame + mac_in_len);
    wipe_sec_bytes(hmac_full, sizeof(hmac_full));
    if (!tag_ok) {
        return (meshx_err_t)MESHX_SUBNET_ERR_AUTH;
    }

    if (!replay_check_and_accept(ctx, hdr.sender_addr, hdr.bridge_nonce)) {
        return (meshx_err_t)MESHX_SUBNET_ERR_REPLAY;
    }

    if (hdr.term > ctx->active_term) {
        ctx->active_term = hdr.term;
    }

    *out_opcode      = hdr.opcode;
    *out_term        = hdr.term;
    *out_sender      = hdr.sender_addr;
    *out_payload     = frame + sizeof(hdr);
    *out_payload_len = (uint16_t)(frame_len - MESHX_CLUSTER_AUTH_OVERHEAD);
    return MESHX_SUCCESS;
}

meshx_err_t meshx_subnet_sec_seal_devkey(const uint8_t dev_key[16],
                                         uint8_t opcode,
                                         uint16_t dst_addr,
                                         uint32_t nonce32,
                                         const uint8_t *plain,
                                         uint16_t plain_len,
                                         uint8_t *out_buf,
                                         uint16_t out_cap,
                                         uint16_t *out_len)
{
    if (!dev_key || !out_buf || !out_len || (plain_len > 0U && !plain)) {
        return MESHX_INVALID_ARG;
    }
    uint16_t needed = (uint16_t)(13U + plain_len + 4U);
    if (out_cap < needed) {
        return MESHX_NO_MEM;
    }

    uint8_t nonce13[13];
    build_devkey_ccm_nonce(opcode, dst_addr, nonce32, nonce13);
    memcpy(out_buf, nonce13, 13U);

    meshx_err_t err = meshx_crypto_aes_ccm_encrypt(dev_key,
                                                   nonce13,
                                                   13U,
                                                   plain,
                                                   plain_len,
                                                   out_buf + 13U,
                                                   out_buf + 13U + plain_len,
                                                   4U);
    if (err != MESHX_SUCCESS) {
        return err;
    }
    *out_len = needed;
    return MESHX_SUCCESS;
}

meshx_err_t meshx_subnet_sec_open_devkey(const uint8_t dev_key[16],
                                         uint8_t opcode,
                                         const uint8_t *sealed,
                                         uint16_t sealed_len,
                                         uint8_t *out_plain,
                                         uint16_t out_cap,
                                         uint16_t *out_plain_len)
{
    if (!dev_key || !sealed || !out_plain || !out_plain_len || sealed_len < 17U) {
        return MESHX_INVALID_ARG;
    }
    uint16_t cipher_len = (uint16_t)(sealed_len - 17U);
    if (out_cap < cipher_len || sealed[0] != 0x02U || sealed[1] != opcode) {
        return (meshx_err_t)MESHX_SUBNET_ERR_AUTH;
    }

    meshx_err_t err = meshx_crypto_aes_ccm_decrypt(dev_key,
                                                   sealed,
                                                   13U,
                                                   sealed + 13U,
                                                   cipher_len,
                                                   sealed + 13U + cipher_len,
                                                   4U,
                                                   out_plain);
    if (err != MESHX_SUCCESS) {
        return (meshx_err_t)MESHX_SUBNET_ERR_AUTH;
    }
    *out_plain_len = cipher_len;
    return MESHX_SUCCESS;
}

meshx_err_t meshx_subnet_sec_whitelist_add(meshx_subnet_security_ctx_t *ctx,
                                           uint16_t neighbor_bridge_addr,
                                           uint32_t allowed_subnet_src)
{
    if (!ctx || neighbor_bridge_addr == 0U || allowed_subnet_src == MESHX_GLOBAL_SUBNET_INVALID) {
        return MESHX_INVALID_ARG;
    }
    int free_idx = -1;
    for (int i = 0; i < (int)MESHX_TRANSIT_WHITELIST_MAX; i++) {
        meshx_transit_whitelist_entry_t *w = &ctx->whitelist[i];
        if (w->valid) {
            if (w->neighbor_bridge_addr == neighbor_bridge_addr &&
                w->allowed_global_subnet_src == allowed_subnet_src) {
                return MESHX_SUCCESS;
            }
        } else if (free_idx < 0) {
            free_idx = i;
        }
    }
    if (free_idx < 0) {
        return MESHX_NO_MEM;
    }
    ctx->whitelist[free_idx].neighbor_bridge_addr      = neighbor_bridge_addr;
    ctx->whitelist[free_idx].allowed_global_subnet_src = allowed_subnet_src;
    ctx->whitelist[free_idx].valid                     = true;
    return MESHX_SUCCESS;
}

bool meshx_subnet_sec_whitelist_check(const meshx_subnet_security_ctx_t *ctx,
                                      uint16_t neighbor_bridge_addr,
                                      uint32_t claimed_subnet_src)
{
    if (!ctx) {
        return false;
    }
    for (size_t i = 0; i < MESHX_TRANSIT_WHITELIST_MAX; i++) {
        const meshx_transit_whitelist_entry_t *w = &ctx->whitelist[i];
        if (w->valid &&
            w->neighbor_bridge_addr == neighbor_bridge_addr &&
            w->allowed_global_subnet_src == claimed_subnet_src) {
            return true;
        }
    }
    return false;
}

bool meshx_subnet_sec_rpl_check_update(meshx_subnet_security_ctx_t *ctx,
                                       uint16_t net_idx,
                                       uint16_t src_addr,
                                       uint32_t key_epoch,
                                       uint32_t iv_index,
                                       uint32_t seq_num)
{
    if (!ctx || src_addr == 0U) {
        return false;
    }
    int free_idx = -1;
    int lru_idx  = 0;

    for (int i = 0; i < (int)MESHX_RPL_CACHE_SIZE; i++) {
        meshx_subnet_rpl_entry_t *e = &ctx->rpl[i];
        if (e->src_addr != 0U) {
            if (e->net_idx == net_idx && e->src_addr == src_addr && e->key_epoch == key_epoch) {
                if (iv_index < e->iv_index) {
                    return false;
                }
                if (iv_index == e->iv_index && seq_num <= e->last_seq_num) {
                    return false;
                }
                e->iv_index     = iv_index;
                e->last_seq_num = seq_num;
                return true;
            }
            if (e->iv_index < ctx->rpl[lru_idx].iv_index ||
                (e->iv_index == ctx->rpl[lru_idx].iv_index &&
                 e->last_seq_num < ctx->rpl[lru_idx].last_seq_num)) {
                lru_idx = i;
            }
        } else if (free_idx < 0) {
            free_idx = i;
        }
    }

    int idx = (free_idx >= 0) ? free_idx : lru_idx;
    ctx->rpl[idx].net_idx      = net_idx;
    ctx->rpl[idx].src_addr     = src_addr;
    ctx->rpl[idx].key_epoch    = key_epoch;
    ctx->rpl[idx].iv_index     = iv_index;
    ctx->rpl[idx].last_seq_num = seq_num;
    return true;
}

bool meshx_subnet_sec_canonical_rpl_check(meshx_subnet_security_ctx_t *ctx,
                                          uint32_t global_subnet_src,
                                          uint16_t canonical_src,
                                          uint32_t iv_index,
                                          uint32_t seq_num)
{
    if (!ctx || canonical_src == 0U) {
        return false;
    }
    int free_idx = -1;
    int lru_idx  = 0;

    for (int i = 0; i < (int)MESHX_NAT_TIER2_CAP; i++) {
        meshx_canonical_rpl_entry_t *e = &ctx->canonical_rpl[i];
        if (e->valid) {
            if (e->global_subnet_src == global_subnet_src && e->canonical_src == canonical_src) {
                if (iv_index < e->iv_index) {
                    return false;
                }
                if (iv_index == e->iv_index && seq_num <= e->last_seq) {
                    return false;
                }
                e->iv_index = iv_index;
                e->last_seq = seq_num;
                return true;
            }
            if (e->last_seq < ctx->canonical_rpl[lru_idx].last_seq) {
                lru_idx = i;
            }
        } else if (free_idx < 0) {
            free_idx = i;
        }
    }

    int idx = (free_idx >= 0) ? free_idx : lru_idx;
    ctx->canonical_rpl[idx].global_subnet_src = global_subnet_src;
    ctx->canonical_rpl[idx].canonical_src     = canonical_src;
    ctx->canonical_rpl[idx].iv_index          = iv_index;
    ctx->canonical_rpl[idx].last_seq          = seq_num;
    ctx->canonical_rpl[idx].valid             = 1U;
    return true;
}

