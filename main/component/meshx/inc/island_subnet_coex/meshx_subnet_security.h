/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_subnet_security.h
 * @brief MitM Authentication, HMAC-SHA256 Envelope, Anti-Replay, Whitelist & Cross-Subnet RPL.
 */

#ifndef MESHX_SUBNET_SECURITY_H
#define MESHX_SUBNET_SECURITY_H

#include "island_subnet_coex/meshx_subnet_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Sliding anti-replay sender slot (16-entry cache) */
typedef struct {
    uint16_t sender_addr;
    uint32_t highest_nonce;
    uint16_t seen_bitmap; /**< Tracks 16 preceding nonces relative to highest_nonce */
    bool     valid;
} meshx_replay_sender_t;

/** Per-subnet Replay Protection Entry (32-entry table: 32 * 16 B) */
typedef struct {
    uint16_t net_idx;
    uint16_t src_addr;
    uint32_t key_epoch;
    uint32_t iv_index;
    uint32_t last_seq_num;
} meshx_subnet_rpl_entry_t;

/** Canonical (CANONICAL_SRC, global_subnet_src) deduplication cache entry */
typedef struct {
    uint32_t global_subnet_src;
    uint32_t iv_index;
    uint32_t last_seq;
    uint16_t canonical_src;
    uint8_t  valid;
} meshx_canonical_rpl_entry_t;

/** Multihop ingress whitelist entry */
typedef struct {
    uint16_t neighbor_bridge_addr;
    uint32_t allowed_global_subnet_src;
    bool     valid;
} meshx_transit_whitelist_entry_t;

/** Callback for persistent checkpoint save */
typedef meshx_err_t (*meshx_sec_ckpt_save_fn_t)(uint32_t nonce_ckpt, void *ctx);

typedef struct {
    uint8_t                         k_bridge[16];
    bool                            k_bridge_valid;
    uint32_t                        cluster_epoch;
    uint32_t                        active_term;
    uint32_t                        tx_bridge_nonce;
    uint32_t                        last_saved_nonce_ckpt;
    uint32_t                        eviction_floor_nonce;
    meshx_replay_sender_t           replay_cache[MESHX_ANTI_REPLAY_CACHE_SIZE];
    meshx_subnet_rpl_entry_t        rpl[MESHX_RPL_CACHE_SIZE];
    meshx_canonical_rpl_entry_t     canonical_rpl[MESHX_NAT_TIER2_CAP];
    meshx_transit_whitelist_entry_t whitelist[MESHX_TRANSIT_WHITELIST_MAX];
    meshx_sec_ckpt_save_fn_t        ckpt_cb;
    void                           *ckpt_ctx;
} meshx_subnet_security_ctx_t;

void meshx_subnet_sec_init(meshx_subnet_security_ctx_t *ctx,
                           uint32_t persisted_nonce_ckpt,
                           meshx_sec_ckpt_save_fn_t ckpt_cb,
                           void *ckpt_ctx);
void meshx_subnet_sec_deinit(meshx_subnet_security_ctx_t *ctx);

/** Provision K_bridge atomically (callers must commit NVS before activating) */
meshx_err_t meshx_subnet_sec_set_k_bridge(meshx_subnet_security_ctx_t *ctx,
                                          const uint8_t k_bridge[16],
                                          uint32_t cluster_epoch,
                                          uint32_t term);
void meshx_subnet_sec_clear_k_bridge(meshx_subnet_security_ctx_t *ctx);

/** Construct & authenticate outbound UVP frame (0xD1..0xDE except 0xD4/0xDA) */
meshx_err_t meshx_subnet_sec_sign_frame(meshx_subnet_security_ctx_t *ctx,
                                        uint8_t opcode,
                                        uint16_t sender_addr,
                                        const uint8_t *payload,
                                        uint16_t payload_len,
                                        uint8_t *out_frame,
                                        uint16_t out_cap,
                                        uint16_t *out_len);

/** Verify inbound authenticated UVP frame (term >= active, 16B HMAC, 16-entry sliding window) */
meshx_err_t meshx_subnet_sec_verify_frame(meshx_subnet_security_ctx_t *ctx,
                                          const uint8_t *frame,
                                          uint16_t frame_len,
                                          uint8_t *out_opcode,
                                          uint32_t *out_term,
                                          uint16_t *out_sender,
                                          const uint8_t **out_payload,
                                          uint16_t *out_payload_len);

/** Encrypt/decrypt DevKey AES-128-CCM commissioning envelopes (0xD4 / 0xDA) */
meshx_err_t meshx_subnet_sec_seal_devkey(const uint8_t dev_key[16],
                                         uint8_t opcode,
                                         uint16_t dst_addr,
                                         uint32_t nonce32,
                                         const uint8_t *plain,
                                         uint16_t plain_len,
                                         uint8_t *out_buf,
                                         uint16_t out_cap,
                                         uint16_t *out_len);

meshx_err_t meshx_subnet_sec_open_devkey(const uint8_t dev_key[16],
                                         uint8_t opcode,
                                         const uint8_t *sealed,
                                         uint16_t sealed_len,
                                         uint8_t *out_plain,
                                         uint16_t out_cap,
                                         uint16_t *out_plain_len);

/** Multihop Transit Ingress Whitelist management & check */
meshx_err_t meshx_subnet_sec_whitelist_add(meshx_subnet_security_ctx_t *ctx,
                                           uint16_t neighbor_bridge_addr,
                                           uint32_t allowed_subnet_src);
bool meshx_subnet_sec_whitelist_check(const meshx_subnet_security_ctx_t *ctx,
                                      uint16_t neighbor_bridge_addr,
                                      uint32_t claimed_subnet_src);

/** Check & update subnet-partitioned RPL (net_idx, src_addr, key_epoch) */
bool meshx_subnet_sec_rpl_check_update(meshx_subnet_security_ctx_t *ctx,
                                       uint16_t net_idx,
                                       uint16_t src_addr,
                                       uint32_t key_epoch,
                                       uint32_t iv_index,
                                       uint32_t seq_num);

/** Check & update Stage 2 Canonical Source RPL (canonical_src, global_subnet_src) */
bool meshx_subnet_sec_canonical_rpl_check(meshx_subnet_security_ctx_t *ctx,
                                          uint32_t global_subnet_src,
                                          uint16_t canonical_src,
                                          uint32_t iv_index,
                                          uint32_t seq_num);

#ifdef __cplusplus
}
#endif

#endif /* MESHX_SUBNET_SECURITY_H */

