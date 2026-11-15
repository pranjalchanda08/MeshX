/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_router_core.h
 * @brief 2-Stage Decoupled Store-and-Forward Bridge Router & Canonical Nonce Engine.
 */

#ifndef MESHX_ROUTER_CORE_H
#define MESHX_ROUTER_CORE_H

#include "island/meshx_router_queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Reassembled Stage 1 Upper Transport PDU descriptor presented for bridging admission.
 */
typedef struct {
    uint16_t          net_idx;             /**< Ingress BLE Mesh NetKey Index */
    uint16_t          src_addr;            /**< Ingress source unicast address */
    uint16_t          dst_addr;            /**< Ingress destination unicast/group/virtual address */
    uint32_t          iv_index;            /**< Ingress IV Index */
    uint32_t          seq_auth;            /**< Authenticated original sequence number */
    uint8_t           ttl;                 /**< Ingress Time-to-Live */
    uint8_t           szmic;               /**< Ingress SzMIC flag (0 = 32-bit MIC, 1 = 64-bit MIC) */
    meshx_trans_mic_t trans_mic;           /**< Preserved Upper Transport MIC */
    const uint8_t    *pdu;                 /**< Pointer to Upper Transport PDU payload */
    uint8_t           pdu_len;             /**< Length of PDU payload in bytes */
    bool              is_multihop_transit; /**< true if packet carries a 23B multihop transit header */
    uint32_t          visited_mask;        /**< Multihop visited slot bitmask */
    uint32_t          global_subnet_dst;   /**< Final destination island Global Subnet ID */
} meshx_router_stage1_pdu_t;

/**
 * @brief Transport TX injection callback wired to BLE Mesh stack or test harness.
 *
 * @param[in] entry      Pointer to forwarding queue entry.
 * @param[in] nonce      Pointer to 13-byte canonical nonce.
 * @param[in] tx_net_key Pointer to selected transmit NetKey.
 * @param[in] user_ctx   Opaque user context registered with router core.
 * @return MESHX_SUCCESS on success, or transmission error code.
 */
typedef meshx_err_t (*meshx_router_send_fn_t)(const meshx_router_queue_entry_t *entry,
                                              const meshx_ccm_nonce_t *nonce,
                                              const meshx_subnet_key_t *tx_net_key,
                                              void *user_ctx);

/**
 * @brief Reliable Stage 2 NACK transmission callback.
 *
 * @param[in] net_idx   Return NetKey Index toward origin island.
 * @param[in] dst_addr  Return destination unicast address.
 * @param[in] nack_buf  Pointer to 0xD7 NACK payload buffer.
 * @param[in] nack_len  Length of NACK payload buffer.
 * @param[in] user_ctx  Opaque user context.
 * @return MESHX_SUCCESS on success, or transmission error code.
 */
typedef meshx_err_t (*meshx_router_nack_send_fn_t)(uint16_t net_idx,
                                                   uint16_t dst_addr,
                                                   const uint8_t *nack_buf,
                                                   uint8_t nack_len,
                                                   void *user_ctx);

/**
 * @brief Tracked reliable Stage 2 NACK awaiting 0xD9 ACK.
 */
typedef struct {
    uint32_t nack_id;                                 /**< Unique monotonic NACK identifier */
    uint16_t return_net_idx;                          /**< Return NetKey Index */
    uint16_t return_dst_addr;                         /**< Return destination address */
    uint32_t last_tx_ms;                              /**< Monotonic timestamp of last transmission */
    uint8_t  retry_count;                             /**< Retransmission attempt counter */
    bool     active;                                  /**< true if NACK is currently pending ACK */
    uint8_t  payload[sizeof(meshx_uvp_router_nack_t)];/**< Exact binary 0xD7 NACK frame */
} meshx_pending_nack_t;

/**
 * @brief Single-Epoch Segmented Reassembly Context.
 */
typedef struct {
    bool     in_use;                                  /**< true if reassembly context is currently active */
    uint16_t net_idx;                                 /**< Flow NetKey Index */
    uint16_t src_addr;                                /**< Flow source unicast address */
    uint16_t dst_addr;                                /**< Flow destination address */
    uint16_t epoch_id;                                /**< Flow key rotation epoch */
    uint32_t seq_zero;                                /**< Flow 13-bit SeqZero base */
    uint8_t  seg_n;                                   /**< Total number of segments minus 1 */
    uint32_t rx_mask;                                 /**< Received segment bitmask (bits 0..seg_n) */
    uint8_t  len;                                     /**< Total assembled payload length */
    uint8_t  buf[MESHX_MAX_UPPER_PDU_LEN];            /**< Assembled Upper Transport PDU buffer */
} meshx_reasm_epoch_ctx_t;

/**
 * @brief Main Router Context (static BSS allocated).
 */
typedef struct {
    meshx_router_queue_t        queue;        /**< Bounded 4-flow forwarding queue */
    meshx_reasm_epoch_ctx_t     reasm;        /**< Single-epoch reassembly context */
    meshx_pending_nack_t        pending_nack; /**< Reliable Stage 2 NACK tracker */
    uint32_t                    next_nack_id; /**< Monotonic NACK sequence generator */
    meshx_router_send_fn_t      tx_cb;        /**< Stage 2 TX injection callback */
    meshx_router_nack_send_fn_t nack_cb;      /**< 0xD7 NACK injection callback */
    void                       *cb_ctx;       /**< Opaque callback context */
    bool                        initialized;  /**< true if router core is initialized */
} meshx_router_ctx_t;

/**
 * @brief Initialize router core context and register injection callbacks.
 *
 * @param[out] ctx     Pointer to router context.
 * @param[in]  tx_cb   Stage 2 packet transmission callback.
 * @param[in]  nack_cb Reliable 0xD7 NACK transmission callback.
 * @param[in]  cb_ctx  Opaque context passed to callbacks.
 */
void meshx_router_core_init(meshx_router_ctx_t *ctx,
                            meshx_router_send_fn_t tx_cb,
                            meshx_router_nack_send_fn_t nack_cb,
                            void *cb_ctx);

/**
 * @brief De-initialize router core context and purge all queues and reassembly state.
 *
 * @param[in,out] ctx Pointer to router context.
 */
void meshx_router_core_deinit(meshx_router_ctx_t *ctx);

/**
 * @brief Build the 13-byte Canonical Bridge Nonce from CANONICAL_SRC / CANONICAL_DST / SEQ / IVI.
 *
 * Ensures Stage 2 preserves the original Transport MIC without breaking AppKey/DevKey decryption.
 *
 * @param[in]  canonical_src Canonical origin source address.
 * @param[in]  canonical_dst Canonical destination address.
 * @param[in]  iv_index      Associated IV Index.
 * @param[in]  seq_auth      Authenticated original sequence number.
 * @param[in]  szmic         SzMIC flag (0 = 32-bit MIC, 1 = 64-bit MIC).
 * @param[in]  is_app_nonce  true for Application nonce, false for Device nonce.
 * @param[out] out_nonce     Destination structure for the 13-byte canonical nonce.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG.
 */
meshx_err_t meshx_router_build_canonical_nonce(uint16_t canonical_src,
                                               uint16_t canonical_dst,
                                               uint32_t iv_index,
                                               uint32_t seq_auth,
                                               uint8_t  szmic,
                                               bool     is_app_nonce,
                                               meshx_ccm_nonce_t *out_nonce);

/**
 * @brief Single-Epoch Stage 1 segment collector (isolates pre- vs post-key-rotation segments).
 *
 * @param[in,out] ctx          Pointer to router context.
 * @param[in]     net_idx      Ingress NetKey Index.
 * @param[in]     src_addr     Ingress source unicast address.
 * @param[in]     dst_addr     Ingress destination address.
 * @param[in]     epoch_id     Key rotation epoch identifier.
 * @param[in]     seq_zero     13-bit SeqZero base.
 * @param[in]     seg_o        Segment offset index [0..seg_n].
 * @param[in]     seg_n        Last segment index [0..31].
 * @param[in]     seg_data     Pointer to segment data payload.
 * @param[in]     seg_len      Segment data length (<= 12 bytes).
 * @param[out]    out_complete Set to true if all segments of the block have been received.
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG or MESHX_INVALID_STATE on epoch mismatch.
 */
meshx_err_t meshx_router_reasm_segment(meshx_router_ctx_t *ctx,
                                       uint16_t net_idx,
                                       uint16_t src_addr,
                                       uint16_t dst_addr,
                                       uint16_t epoch_id,
                                       uint32_t seq_zero,
                                       uint8_t  seg_o,
                                       uint8_t  seg_n,
                                       const uint8_t *seg_data,
                                       uint8_t  seg_len,
                                       bool    *out_complete);

/**
 * @brief Stage 1 Ingress validation, NAT/Group translation & Store-and-Forward Queue admission.
 *
 * @param[in,out] ctx                      Pointer to router context.
 * @param[in]     pdu                      Reassembled Stage 1 packet descriptor.
 * @param[in]     now_ms                   Current monotonic timestamp in milliseconds.
 * @param[out]    out_send_stage1_block_ack Set to true if Lower Transport Block ACK should be sent.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_router_stage1_ingress(meshx_router_ctx_t *ctx,
                                        const meshx_router_stage1_pdu_t *pdu,
                                        uint32_t now_ms,
                                        bool *out_send_stage1_block_ack);

/**
 * @brief Pump Stage 2 egress queue (max 3 retries, reliable NACK if all fail).
 *
 * @param[in,out] ctx    Pointer to router context.
 * @param[in]     now_ms Current monotonic timestamp in milliseconds.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_router_stage2_pump(meshx_router_ctx_t *ctx, uint32_t now_ms);

/**
 * @brief Handle Stage 2 completion notification from BLE Mesh Lower SAR.
 *
 * @param[in,out] ctx    Pointer to router context.
 * @param[in]     ack_ok true if Lower Transport Block ACK was received, false on timeout/abort.
 * @param[in]     now_ms Current monotonic timestamp in milliseconds.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_router_stage2_on_tx_complete(meshx_router_ctx_t *ctx,
                                               bool ack_ok,
                                               uint32_t now_ms);

/**
 * @brief Process 0xD9 Router NACK ACK to stop reliable NACK retransmission.
 *
 * @param[in,out] ctx     Pointer to router context.
 * @param[in]     nack_id Acknowledged NACK identifier.
 * @return MESHX_SUCCESS on success, or MESHX_NOT_FOUND.
 */
meshx_err_t meshx_router_on_nack_ack(meshx_router_ctx_t *ctx, uint32_t nack_id);

#ifdef __cplusplus
}
#endif

#endif /* MESHX_ROUTER_CORE_H */
