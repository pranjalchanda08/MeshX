/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_router_core.h
 * @brief 2-Stage Decoupled Store-and-Forward Bridge Router & Canonical Nonce Engine.
 */

#ifndef MESHX_ROUTER_CORE_H
#define MESHX_ROUTER_CORE_H

#include "island_subnet_coex/meshx_router_queue.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t net_idx;
    uint16_t src_addr;
    uint16_t dst_addr;
    uint32_t iv_index;
    uint32_t seq_auth;
    uint8_t  ttl;
    uint8_t  szmic;
    uint8_t  trans_mic[8];
    const uint8_t *pdu;
    uint8_t  pdu_len;
    bool     is_multihop_transit;
    uint32_t visited_mask;
    uint32_t global_subnet_dst;
} meshx_router_stage1_pdu_t;

/** Transport TX injection callbacks (wired to BLE Mesh HAL or host test doubles) */
typedef meshx_err_t (*meshx_router_send_fn_t)(const meshx_router_queue_entry_t *entry,
                                              const uint8_t nonce13[13],
                                              const uint8_t *tx_net_key,
                                              void *user_ctx);

typedef meshx_err_t (*meshx_router_nack_send_fn_t)(uint16_t net_idx,
                                                   uint16_t dst_addr,
                                                   const uint8_t *nack_buf,
                                                   uint8_t nack_len,
                                                   void *user_ctx);

/** Tracked reliable Stage 2 NACK awaiting 0xD9 ACK */
typedef struct {
    uint32_t nack_id;
    uint16_t return_net_idx;
    uint16_t return_dst_addr;
    uint32_t last_tx_ms;
    uint8_t  retry_count;
    bool     active;
    uint8_t  payload[sizeof(meshx_uvp_router_nack_t)];
} meshx_pending_nack_t;

/** Single-Epoch Segmented Reassembly Context */
typedef struct {
    bool     in_use;
    uint16_t net_idx;
    uint16_t src_addr;
    uint16_t dst_addr;
    uint16_t epoch_id;
    uint32_t seq_zero;
    uint8_t  seg_n;
    uint32_t rx_mask;
    uint8_t  len;
    uint8_t  buf[MESHX_MAX_UPPER_PDU_LEN];
} meshx_reasm_epoch_ctx_t;

/** Main Router Context (static BSS allocated) */
typedef struct {
    meshx_router_queue_t       queue;
    meshx_reasm_epoch_ctx_t    reasm;
    meshx_pending_nack_t       pending_nack;
    uint32_t                   next_nack_id;
    meshx_router_send_fn_t     tx_cb;
    meshx_router_nack_send_fn_t nack_cb;
    void                      *cb_ctx;
    bool                       initialized;
} meshx_router_ctx_t;

void meshx_router_core_init(meshx_router_ctx_t *ctx,
                            meshx_router_send_fn_t tx_cb,
                            meshx_router_nack_send_fn_t nack_cb,
                            void *cb_ctx);
void meshx_router_core_deinit(meshx_router_ctx_t *ctx);

/**
 * @brief Build the 13-byte Canonical Bridge Nonce from CANONICAL_SRC / CANONICAL_DST / SEQ / IVI
 *        so Stage 2 preserves the original Transport MIC without breaking AppKey/DevKey decrypt.
 */
meshx_err_t meshx_router_build_canonical_nonce(uint16_t canonical_src,
                                               uint16_t canonical_dst,
                                               uint32_t iv_index,
                                               uint32_t seq_auth,
                                               uint8_t  szmic,
                                               bool     is_app_nonce,
                                               uint8_t  out_nonce[13]);

/**
 * @brief Single-Epoch Stage 1 segment collector (isolates pre- vs post-key-rotation segments)
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
 *        Returns MESHX_SUCCESS and optionally triggers NACK / drops backpressure pre-ACK.
 */
meshx_err_t meshx_router_stage1_ingress(meshx_router_ctx_t *ctx,
                                        const meshx_router_stage1_pdu_t *pdu,
                                        uint32_t now_ms,
                                        bool *out_send_stage1_block_ack);

/**
 * @brief Pump Stage 2 egress queue (max 3 retries, reliable NACK if all fail).
 */
meshx_err_t meshx_router_stage2_pump(meshx_router_ctx_t *ctx, uint32_t now_ms);

/**
 * @brief Handle Stage 2 completion notification from BLE Mesh Lower SAR (status == 0 means ACKed).
 */
meshx_err_t meshx_router_stage2_on_tx_complete(meshx_router_ctx_t *ctx,
                                               bool ack_ok,
                                               uint32_t now_ms);

/**
 * @brief Process 0xD9 Router NACK ACK to stop reliable NACK retransmission.
 */
meshx_err_t meshx_router_on_nack_ack(meshx_router_ctx_t *ctx, uint32_t nack_id);

#ifdef __cplusplus
}
#endif

#endif /* MESHX_ROUTER_CORE_H */

