/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_subnet_security.h
 * @brief MitM Authentication, HMAC-SHA256 Envelope, Anti-Replay, Whitelist & Cross-Subnet RPL.
 */

#ifndef MESHX_SUBNET_SECURITY_H
#define MESHX_SUBNET_SECURITY_H

#include "island/meshx_subnet_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sliding anti-replay sender slot (16-entry cache, 32-bit sliding window).
 */
typedef struct {
    uint32_t boundary_id;   /**< Boundary scope for this sender */
    uint16_t sender_addr;   /**< Sender unicast address */
    uint32_t highest_nonce; /**< Highest verified bridge nonce seen from sender */
    uint32_t seen_bitmap;   /**< Bitmask tracking 32 preceding nonces relative to highest_nonce */
    bool     valid;         /**< true if entry is active */
} meshx_replay_sender_t;

/**
 * @brief Per-subnet Replay Protection Entry (32-entry table with 32-bit sliding window).
 */
typedef struct {
    uint16_t net_idx;      /**< BLE Mesh NetKey Index */
    uint16_t src_addr;     /**< Origin physical address */
    uint32_t key_epoch;    /**< Subnet key rotation epoch */
    uint32_t iv_index;     /**< Associated IV Index */
    uint32_t last_seq_num; /**< Highest sequence number recorded */
    uint32_t seq_window;   /**< 32-bit sliding window relative to last_seq_num */
} meshx_subnet_rpl_entry_t;

/**
 * @brief Canonical (CANONICAL_SRC, global_subnet_src) deduplication cache entry.
 */
typedef struct {
    uint32_t global_subnet_src; /**< Origin island Global Subnet ID */
    uint32_t iv_index;          /**< IV Index */
    uint32_t last_seq;          /**< Highest sequence number recorded */
    uint16_t canonical_src;     /**< Canonical source address */
    uint8_t  valid;             /**< 1 if valid, 0 if free */
} meshx_canonical_rpl_entry_t;

/**
 * @brief Multihop ingress whitelist entry.
 */
typedef struct {
    uint16_t neighbor_bridge_addr;      /**< Physical address of immediate neighbor bridge */
    uint32_t allowed_global_subnet_src; /**< Authorized origin Global Subnet ID */
    bool     valid;                     /**< true if whitelist rule is active */
} meshx_transit_whitelist_entry_t;

/**
 * @brief Callback for persistent checkpoint save.
 */
typedef meshx_err_t (*meshx_sec_ckpt_save_fn_t)(uint32_t nonce_ckpt, void *ctx);

/**
 * @brief Security manager state context.
 */
typedef struct {
    meshx_subnet_key_t              k_bridge;                                  /**< 16-byte Cluster Bridge Key */
    bool                            k_bridge_valid;                            /**< true if K_bridge is provisioned and valid */
    uint32_t                        cluster_epoch;                             /**< Active cluster epoch */
    uint32_t                        active_term;                               /**< Active failover term */
    uint32_t                        tx_bridge_nonce;                           /**< Monotonic outbound bridge nonce */
    uint32_t                        last_saved_nonce_ckpt;                     /**< Nonce checkpoint last committed to NVS */
    uint32_t                        eviction_floor_nonce;                      /**< Eviction floor for anti-replay cache */
    meshx_replay_sender_t           replay_cache[MESHX_ANTI_REPLAY_CACHE_SIZE];/**< 16-entry sliding anti-replay window */
    meshx_subnet_rpl_entry_t        rpl[MESHX_RPL_CACHE_SIZE];                 /**< 32-entry subnet-partitioned RPL */
    meshx_canonical_rpl_entry_t     canonical_rpl[MESHX_NAT_TIER2_CAP];        /**< Canonical source deduplication RPL */
    meshx_transit_whitelist_entry_t whitelist[MESHX_TRANSIT_WHITELIST_MAX];    /**< Multihop transit ingress whitelist */
    meshx_sec_ckpt_save_fn_t        ckpt_cb;                                   /**< Checkpoint save callback */
    void                           *ckpt_ctx;                                  /**< Checkpoint callback context */
} meshx_subnet_security_ctx_t;

/**
 * @brief Initialize security subsystem with persisted nonce checkpoint.
 *
 * @param[out] ctx                  Pointer to security context.
 * @param[in]  persisted_nonce_ckpt Nonce checkpoint loaded from NVS.
 * @param[in]  ckpt_cb              Checkpoint commit callback.
 * @param[in]  ckpt_ctx             Callback context pointer.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if ctx is NULL.
 */
meshx_err_t meshx_subnet_sec_init(meshx_subnet_security_ctx_t *ctx,
                           uint32_t persisted_nonce_ckpt,
                           meshx_sec_ckpt_save_fn_t ckpt_cb,
                           void *ckpt_ctx);

/**
 * @brief De-initialize security context and scrub key material.
 *
 * @param[in,out] ctx Pointer to security context.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if @p ctx is NULL.
 */
meshx_err_t meshx_subnet_sec_deinit(meshx_subnet_security_ctx_t *ctx);

/**
 * @brief Provision K_bridge atomically (callers must commit NVS before activating).
 *
 * @param[in,out] ctx           Pointer to security context.
 * @param[in]     k_bridge      Pointer to 16-byte Cluster Bridge Key.
 * @param[in]     cluster_epoch Cluster epoch counter.
 * @param[in]     term          Failover term.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG.
 */
meshx_err_t meshx_subnet_sec_set_k_bridge(meshx_subnet_security_ctx_t *ctx,
                                          const meshx_subnet_key_t *k_bridge,
                                          uint32_t cluster_epoch,
                                          uint32_t term);

/**
 * @brief Wipe Cluster Bridge Key from memory.
 *
 * @param[in,out] ctx Pointer to security context.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if @p ctx is NULL.
 */
meshx_err_t meshx_subnet_sec_clear_k_bridge(meshx_subnet_security_ctx_t *ctx);

/**
 * @brief Request descriptor for meshx_subnet_sec_sign_frame().
 */
typedef struct {
    uint8_t        opcode;      /**< UVP opcode */
    uint16_t       sender_addr; /**< Local sender unicast address */
    const uint8_t *payload;     /**< Payload to sign */
    uint16_t       payload_len; /**< Length of payload in bytes */
} meshx_sec_sign_req_t;

/**
 * @brief Response buffer descriptor for meshx_subnet_sec_sign_frame().
 */
typedef struct {
    uint8_t  *out_frame; /**< Destination buffer for signed frame */
    uint16_t  out_cap;   /**< Capacity of out_frame buffer */
    uint16_t *out_len;   /**< Total length of signed frame */
} meshx_sec_sign_rsp_t;

/**
 * @brief Output descriptor for meshx_subnet_sec_verify_frame().
 */
typedef struct {
    uint8_t        opcode;      /**< Extracted opcode */
    uint32_t       term;        /**< Extracted term */
    uint16_t       sender;      /**< Extracted sender address */
    const uint8_t *payload;     /**< Pointer set to start of verified inner payload */
    uint16_t       payload_len; /**< Length of inner payload */
} meshx_sec_verify_rsp_t;

/**
 * @brief Request descriptor for meshx_subnet_sec_seal_devkey().
 */
typedef struct {
    const meshx_subnet_key_t *dev_key;   /**< Pointer to 16-byte Device Key */
    uint8_t                   opcode;    /**< UVP opcode being sealed */
    uint16_t                  dst_addr;  /**< Target destination address */
    uint32_t                  nonce32;   /**< 32-bit transaction nonce */
    const uint8_t            *plain;     /**< Plaintext payload */
    uint16_t                  plain_len; /**< Plaintext payload length */
} meshx_sec_devkey_seal_req_t;

/**
 * @brief Response buffer descriptor for meshx_subnet_sec_seal_devkey().
 */
typedef struct {
    uint8_t  *out_buf; /**< Destination buffer for sealed envelope */
    uint16_t  out_cap; /**< Capacity of destination buffer */
    uint16_t *out_len; /**< Length of generated sealed envelope */
} meshx_sec_devkey_seal_rsp_t;

/**
 * @brief Sign outbound UVP frame with K_bridge (HMAC-SHA256 truncated to 16B).
 *
 * @param[in,out] ctx Pointer to security context.
 * @param[in]     req Pointer to sign request descriptor.
 * @param[out]    rsp Pointer to sign response buffer descriptor.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_sec_sign_frame(meshx_subnet_security_ctx_t *ctx,
                                        const meshx_sec_sign_req_t *req,
                                        meshx_sec_sign_rsp_t *rsp);

/**
 * @brief Verify inbound authenticated UVP frame (term >= active, 16B HMAC, 16-entry sliding window).
 *
 * @param[in,out] ctx       Pointer to security context.
 * @param[in]     frame     Raw inbound signed frame.
 * @param[in]     frame_len Length of signed frame.
 * @param[out]    rsp       Pointer to verification output descriptor.
 * @return MESHX_SUCCESS on success, MESHX_SUBNET_ERR_AUTH on tag mismatch, MESHX_SUBNET_ERR_REPLAY on replay.
 */
meshx_err_t meshx_subnet_sec_verify_frame(meshx_subnet_security_ctx_t *ctx,
                                          const uint8_t *frame,
                                          uint16_t frame_len,
                                          meshx_sec_verify_rsp_t *rsp);

/**
 * @brief Verify inbound authenticated UVP frame scoped to a boundary's active term.
 *
 * @param[in,out] ctx           Pointer to security context.
 * @param[in]     boundary_id   Boundary identifier for replay/term scoping (0 = global).
 * @param[in,out] boundary_term Pointer to boundary-scoped active term (NULL = use ctx->active_term).
 * @param[in]     frame         Raw inbound signed frame.
 * @param[in]     frame_len     Length of signed frame.
 * @param[out]    rsp           Pointer to verification output descriptor.
 * @return MESHX_SUCCESS on success, MESHX_SUBNET_ERR_AUTH on tag/term mismatch, MESHX_SUBNET_ERR_REPLAY on replay.
 */
meshx_err_t meshx_subnet_sec_verify_frame_scoped(meshx_subnet_security_ctx_t *ctx,
                                                 uint32_t boundary_id,
                                                 uint32_t *boundary_term,
                                                 const uint8_t *frame,
                                                 uint16_t frame_len,
                                                 meshx_sec_verify_rsp_t *rsp);

/**
 * @brief Encrypt DevKey AES-128-CCM commissioning envelopes (0xD4 Bridge Invite).
 *
 * @param[in]  req Pointer to DevKey seal request descriptor.
 * @param[out] rsp Pointer to DevKey seal response buffer descriptor.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_sec_seal_devkey(const meshx_sec_devkey_seal_req_t *req,
                                         meshx_sec_devkey_seal_rsp_t *rsp);

/**
 * @brief Decrypt DevKey AES-128-CCM commissioning envelopes (0xD4 Bridge Invite / 0xDA Invite ACK).
 *
 * @param[in]  dev_key       Pointer to 16-byte Device Key.
 * @param[in]  opcode        Expected UVP opcode.
 * @param[in]  sealed        Sealed input buffer.
 * @param[in]  sealed_len    Length of sealed input buffer.
 * @param[out] out_plain     Destination buffer for decrypted plaintext.
 * @param[in]  out_cap       Capacity of out_plain buffer.
 * @param[out] out_plain_len Destination for length of decrypted plaintext.
 * @return MESHX_SUCCESS on success, MESHX_SUBNET_ERR_AUTH on authentication failure.
 */
meshx_err_t meshx_subnet_sec_open_devkey(const meshx_subnet_key_t *dev_key,
                                         uint8_t opcode,
                                         const uint8_t *sealed,
                                         uint16_t sealed_len,
                                         uint8_t *out_plain,
                                         uint16_t out_cap,
                                         uint16_t *out_plain_len);

/**
 * @brief Add rule to Multihop Transit Ingress Whitelist.
 *
 * @param[in,out] ctx                  Pointer to security context.
 * @param[in]     neighbor_bridge_addr Physical address of authorized neighbor bridge.
 * @param[in]     allowed_subnet_src   Authorized origin Global Subnet ID.
 * @return MESHX_SUCCESS on success, or MESHX_NO_MEM.
 */
meshx_err_t meshx_subnet_sec_whitelist_add(meshx_subnet_security_ctx_t *ctx,
                                           uint16_t neighbor_bridge_addr,
                                           uint32_t allowed_subnet_src);

/**
 * @brief Check whether transit ingress packet from neighbor bridge is authorized for claimed subnet.
 *
 * @param[in]  ctx                  Pointer to security context.
 * @param[in]  neighbor_bridge_addr Immediate transmitter address.
 * @param[in]  claimed_subnet_src   Origin Global Subnet ID claimed in transit header.
 * @param[out] out_allowed          Set to true if authorized, false otherwise.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if @p ctx or @p out_allowed is NULL.
 */
meshx_err_t meshx_subnet_sec_whitelist_check(const meshx_subnet_security_ctx_t *ctx,
                                             uint16_t neighbor_bridge_addr,
                                             uint32_t claimed_subnet_src,
                                             bool *out_allowed);

/**
 * @brief Check and update subnet-partitioned Replay Protection List.
 *
 * @param[in,out] ctx       Pointer to security context.
 * @param[in]     net_idx   BLE Mesh NetKey Index.
 * @param[in]     src_addr  Source unicast address.
 * @param[in]     key_epoch Subnet key rotation epoch.
 * @param[in]     iv_index  Current IV Index.
 * @param[in]     seq_num   Received packet sequence number.
 * @param[out]    out_valid Set to true if packet is fresh and recorded, false if replayed.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if @p ctx or @p out_valid is NULL or @p src_addr is 0.
 */
meshx_err_t meshx_subnet_sec_rpl_check_update(meshx_subnet_security_ctx_t *ctx,
                                              uint16_t net_idx,
                                              uint16_t src_addr,
                                              uint32_t key_epoch,
                                              uint32_t iv_index,
                                              uint32_t seq_num,
                                              bool *out_valid);

/**
 * @brief Check and update Stage 2 Canonical Source RPL (canonical_src, global_subnet_src).
 *
 * @param[in,out] ctx               Pointer to security context.
 * @param[in]     global_subnet_src Origin island Global Subnet ID.
 * @param[in]     canonical_src     Canonical source unicast address.
 * @param[in]     iv_index          Current IV Index.
 * @param[in]     seq_num           Sequence number.
 * @param[out]    out_valid         Set to true if fresh and recorded, false if replayed.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if @p ctx or @p out_valid is NULL or @p canonical_src is 0.
 */
meshx_err_t meshx_subnet_sec_canonical_rpl_check(meshx_subnet_security_ctx_t *ctx,
                                                 uint32_t global_subnet_src,
                                                 uint16_t canonical_src,
                                                 uint32_t iv_index,
                                                 uint32_t seq_num,
                                                 bool *out_valid);

#ifdef __cplusplus
}
#endif

#endif /* MESHX_SUBNET_SECURITY_H */
