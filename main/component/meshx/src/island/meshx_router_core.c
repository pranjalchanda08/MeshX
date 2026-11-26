/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_router_core.c
 * @brief 2-Stage Decoupled Store-and-Forward Bridge Router & Canonical Nonce Engine.
 */

#include <stdint.h>
#include <string.h>
#include "island/meshx_router_core.h"

/**
 * @brief Construct binary 0xD7 Router NACK payload from queue entry metadata.
 *
 * @param[in]  nack_id     Unique NACK transaction identifier.
 * @param[in]  entry       Pointer to forwarding queue entry being rejected.
 * @param[in]  reason_code Reject reason code (MESHX_NACK_REASON_*).
 * @param[out] out_nack    Destination structure for constructed NACK.
 */
static meshx_err_t build_nack_payload(uint32_t nack_id,
                                      const meshx_router_queue_entry_t *entry,
                                      uint8_t reason_code,
                                      meshx_uvp_router_nack_t *out_nack)
{
    if (!entry || !out_nack) {
        return MESHX_INVALID_ARG;
    }
    memset(out_nack, 0, sizeof(*out_nack));
    out_nack->nack_id           = nack_id;
    out_nack->orig_seq_auth     = entry->seq_auth;
    out_nack->global_subnet_src = entry->ingress_boundary_id;
    out_nack->global_subnet_dst = entry->egress_boundary_id;
    out_nack->orig_src_addr     = entry->orig_src_addr;
    out_nack->orig_dst_addr     = entry->orig_dst_addr;
    out_nack->reason_code       = reason_code;
    return MESHX_SUCCESS;
}

/**
 * @brief Format canonical nonce and inject head queue entry into Stage 2 lower transport.
 *
 * @param[in,out] ctx   Pointer to router context.
 * @param[in,out] entry Queue entry selected for dispatch.
 * @return MESHX_SUCCESS on success, or transmission error code.
 */
static meshx_err_t dispatch_entry_stage2(meshx_router_ctx_t *ctx,
                                         meshx_router_queue_entry_t *entry)
{
    if (!ctx->tx_cb) {
        return MESHX_NOT_SUPPORTED;
    }
    meshx_ccm_nonce_t nonce13;
    uint8_t szmic = (entry->trans_mic.len >= MESHX_TRANS_MIC_MAX_LEN) ? 1U : 0U;
    meshx_err_t err = meshx_router_build_canonical_nonce(entry->canonical_src,
                                                         entry->canonical_dst,
                                                         entry->orig_iv_index,
                                                         entry->seq_auth,
                                                         szmic,
                                                         true,
                                                         &nonce13);
    if (err != MESHX_SUCCESS) {
        return err;
    }

    err = meshx_router_queue_mark_tx_inflight(&ctx->queue, entry);
    if (err != MESHX_SUCCESS) {
        return err;
    }
    return ctx->tx_cb(entry, &nonce13, NULL, ctx->cb_ctx);
}

/**
 * @brief Initialize router core context and register injection callbacks.
 *
 * @param[out] ctx     Pointer to router context.
 * @param[in]  tx_cb   Stage 2 packet transmission callback.
 * @param[in]  nack_cb Reliable 0xD7 NACK transmission callback.
 * @param[in]  cb_ctx  Opaque context passed to callbacks.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if @p ctx is NULL.
 */
meshx_err_t meshx_router_core_init(meshx_router_ctx_t *ctx,
                                   meshx_router_send_fn_t tx_cb,
                                   meshx_router_nack_send_fn_t nack_cb,
                                   void *cb_ctx)
{
    if (!ctx) {
        return MESHX_INVALID_ARG;
    }
    memset(ctx, 0, sizeof(*ctx));
    meshx_err_t qerr = meshx_router_queue_init(&ctx->queue);
    if (qerr != MESHX_SUCCESS) {
        return qerr;
    }
    ctx->next_nack_id = 1UL;
    ctx->tx_cb        = tx_cb;
    ctx->nack_cb      = nack_cb;
    ctx->cb_ctx       = cb_ctx;
    ctx->initialized  = true;
    return MESHX_SUCCESS;
}

/**
 * @brief De-initialize router core context and purge all queues and reassembly state.
 *
 * @param[in,out] ctx Pointer to router context.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if @p ctx is NULL.
 */
meshx_err_t meshx_router_core_deinit(meshx_router_ctx_t *ctx)
{
    if (!ctx) {
        return MESHX_INVALID_ARG;
    }
    meshx_err_t qerr = meshx_router_queue_deinit(&ctx->queue);
    memset(ctx, 0, sizeof(*ctx));
    return qerr;
}

/**
 * @brief Build the 13-byte Canonical Bridge Nonce from CANONICAL_SRC / CANONICAL_DST / SEQ / IVI.
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
                                               meshx_ccm_nonce_t *out_nonce)
{
    if (!out_nonce) {
        return MESHX_INVALID_ARG;
    }
    out_nonce->bytes[0]  = is_app_nonce ? MESHX_NONCE_TYPE_APPLICATION : MESHX_NONCE_TYPE_DEVICE;
    out_nonce->bytes[1]  = (szmic != 0U) ? 0x80U : 0x00U;
    out_nonce->bytes[2]  = MESHX_BE_BYTE(seq_auth, MESHX_SHIFT_BYTE2);
    out_nonce->bytes[3]  = MESHX_BE_BYTE(seq_auth, MESHX_SHIFT_BYTE1);
    out_nonce->bytes[4]  = MESHX_BE_BYTE(seq_auth, MESHX_SHIFT_LSB);
    out_nonce->bytes[5]  = MESHX_BE_BYTE(canonical_src, MESHX_SHIFT_BYTE1);
    out_nonce->bytes[6]  = MESHX_BE_BYTE(canonical_src, MESHX_SHIFT_LSB);
    out_nonce->bytes[7]  = MESHX_BE_BYTE(canonical_dst, MESHX_SHIFT_BYTE1);
    out_nonce->bytes[8]  = MESHX_BE_BYTE(canonical_dst, MESHX_SHIFT_LSB);
    out_nonce->bytes[9]  = MESHX_BE_BYTE(iv_index, MESHX_SHIFT_MSB32);
    out_nonce->bytes[10] = MESHX_BE_BYTE(iv_index, MESHX_SHIFT_BYTE2);
    out_nonce->bytes[11] = MESHX_BE_BYTE(iv_index, MESHX_SHIFT_BYTE1);
    out_nonce->bytes[12] = MESHX_BE_BYTE(iv_index, MESHX_SHIFT_LSB);
    return MESHX_SUCCESS;
}

/**
 * @brief Ingest a single SAR segment into the active single-epoch reassembly context.
 *
 * Isolates pre- vs post-key-rotation segments on the same flow and marks
 * @p out_complete true once all segments up to @p seg->seg_n are received.
 *
 * @param[in,out] ctx          Pointer to router context.
 * @param[in]     seg          Pointer to inbound segment descriptor.
 * @param[out]    out_complete Set to true if all segments of the block have been received.
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG, MESHX_SUBNET_ERR_MTU, or
 *         MESHX_INVALID_STATE on epoch mismatch.
 */
meshx_err_t meshx_router_reasm_segment(meshx_router_ctx_t *ctx,
                                       const meshx_router_seg_rx_t *seg,
                                       bool *out_complete)
{
    if (out_complete) {
        *out_complete = false;
    }
    if (!ctx || !seg || !seg->seg_data || !out_complete || seg->seg_n >= MESHX_MAX_SEGMENTS || seg->seg_o > seg->seg_n) {
        return MESHX_INVALID_ARG;
    }
    size_t offset = (size_t)seg->seg_o * (size_t)MESHX_SAR_SEGMENT_MAX_DATA_LEN;
    if (offset + (size_t)seg->seg_len > MESHX_MAX_UPPER_PDU_LEN) {
        return MESHX_SUBNET_ERR_MTU;
    }

    if (ctx->reasm.in_use) {
        bool same_flow = (ctx->reasm.net_idx == seg->net_idx &&
                          ctx->reasm.src_addr == seg->src_addr &&
                          ctx->reasm.dst_addr == seg->dst_addr &&
                          ctx->reasm.seq_zero == seg->seq_zero);
        if (same_flow && ctx->reasm.epoch_id != seg->epoch_id) {
            /* Single-Epoch Isolation: discard stale epoch segments on key transition */
            memset(&ctx->reasm, 0, sizeof(ctx->reasm));
            return MESHX_INVALID_STATE;
        }
        if (!same_flow) {
            memset(&ctx->reasm, 0, sizeof(ctx->reasm));
        }
    }

    if (!ctx->reasm.in_use) {
        memset(&ctx->reasm, 0, sizeof(ctx->reasm));
        ctx->reasm.in_use   = true;
        ctx->reasm.net_idx  = seg->net_idx;
        ctx->reasm.src_addr = seg->src_addr;
        ctx->reasm.dst_addr = seg->dst_addr;
        ctx->reasm.epoch_id = seg->epoch_id;
        ctx->reasm.seq_zero = seg->seq_zero;
        ctx->reasm.seg_n    = seg->seg_n;
    }

    if (seg->seg_len > 0U) {
        memcpy(&ctx->reasm.buf[offset], seg->seg_data, seg->seg_len);
        uint8_t end_pos = (uint8_t)(offset + seg->seg_len);
        if (end_pos > ctx->reasm.len) {
            ctx->reasm.len = end_pos;
        }
    }
    ctx->reasm.rx_mask |= (1UL << seg->seg_o);

    uint32_t expected_mask = (seg->seg_n == (MESHX_MAX_SEGMENTS - 1U)) ? UINT32_MAX : ((1UL << (seg->seg_n + 1U)) - 1UL);
    if ((ctx->reasm.rx_mask & expected_mask) == expected_mask) {
        ctx->reasm.in_use = false;
        *out_complete = true;
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Populate forwarding-queue enqueue parameters from a Stage 1 PDU descriptor.
 *
 * @param[in]  pdu        Reassembled Stage 1 packet descriptor.
 * @param[in]  now_ms     Current monotonic timestamp in milliseconds.
 * @param[out] out_params Destination enqueue parameter structure.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if pointers are NULL.
 */
static meshx_err_t build_stage1_enqueue_params(const meshx_router_stage1_pdu_t *pdu,
                                               uint32_t now_ms,
                                               meshx_router_enqueue_params_t *out_params)
{
    if (!pdu || !out_params) {
        return MESHX_INVALID_ARG;
    }
    memset(out_params, 0, sizeof(*out_params));
    out_params->ingress_boundary_id = (pdu->global_subnet_src != MESHX_GLOBAL_SUBNET_INVALID)
                                          ? pdu->global_subnet_src
                                          : (uint32_t)pdu->net_idx;
    out_params->egress_boundary_id  = pdu->global_subnet_dst;
    out_params->seq_auth            = pdu->seq_auth;
    out_params->orig_iv_index       = pdu->iv_index;
    out_params->now_ms              = now_ms;
    out_params->canonical_src       = (pdu->canonical_src != 0U) ? pdu->canonical_src : pdu->src_addr;
    out_params->canonical_dst       = (pdu->canonical_dst != 0U) ? pdu->canonical_dst : pdu->dst_addr;
    out_params->orig_src_addr       = pdu->src_addr;
    out_params->orig_dst_addr       = (pdu->orig_dst_addr != 0U) ? pdu->orig_dst_addr : pdu->dst_addr;
    out_params->ingress_src_addr    = pdu->src_addr;
    out_params->tx_net_idx          = (pdu->egress_net_idx != 0U || pdu->net_idx == 0U)
                                          ? pdu->egress_net_idx
                                          : pdu->net_idx;
    out_params->ttl                 = (uint8_t)(pdu->ttl - 1U);
    out_params->trans_mic           = pdu->trans_mic;
    if (out_params->trans_mic.len == 0U) {
        out_params->trans_mic.len = (pdu->szmic != 0U) ? MESHX_TRANS_MIC_MAX_LEN : 4U;
    }
    out_params->pdu                 = pdu->pdu;
    out_params->pdu_len             = pdu->pdu_len;
    out_params->flags               = (pdu->dst_addr < 0x8000U ? MESHX_ROUTER_FLAG_IS_UNICAST : 0U) |
                                      (pdu->is_multihop_transit ? MESHX_ROUTER_FLAG_MULTIHOP : 0U);
    return MESHX_SUCCESS;
}

/**
 * @brief Send a rate-limited emergency 0xD7 NACK when Stage 1 queue admission fails.
 *
 * @param[in,out] ctx    Pointer to router context.
 * @param[in]     pdu    Rejected Stage 1 packet descriptor.
 * @param[in]     params Populated enqueue parameter structure for NACK metadata.
 * @param[in]     now_ms Current monotonic timestamp in milliseconds.
 * @return MESHX_SUCCESS if skipped/rate-limited/sent, or NACK TX callback error code.
 */
static meshx_err_t maybe_send_emergency_nack(meshx_router_ctx_t *ctx,
                                             const meshx_router_stage1_pdu_t *pdu,
                                             const meshx_router_enqueue_params_t *params,
                                             uint32_t now_ms)
{
    if (!ctx->nack_cb) {
        return MESHX_SUCCESS;
    }
    bool rate_limited = (ctx->last_nack_src == pdu->src_addr &&
                         (now_ms - ctx->last_nack_ms) < 1000UL &&
                         ctx->last_nack_ms != 0UL);
    if (rate_limited) {
        return MESHX_SUCCESS;
    }

    const uint8_t *nack_buf = NULL;
    uint8_t nack_len = 0U;
    uint32_t nack_id = ctx->next_nack_id++;
    if (meshx_router_queue_build_emergency_nack(&ctx->queue,
                                                nack_id,
                                                params,
                                                MESHX_NACK_REASON_QUEUE_FULL,
                                                &nack_buf,
                                                &nack_len) != MESHX_SUCCESS) {
        return MESHX_SUCCESS;
    }

    ctx->last_nack_src = pdu->src_addr;
    ctx->last_nack_ms  = (now_ms == 0UL) ? 1UL : now_ms;
    return ctx->nack_cb(pdu->net_idx, pdu->src_addr, nack_buf, nack_len, ctx->cb_ctx);
}

/**
 * @brief Stage 1 Ingress validation, NAT/Group translation & Store-and-Forward Queue admission.
 *
 * @param[in,out] ctx                       Pointer to router context.
 * @param[in]     pdu                       Reassembled Stage 1 packet descriptor.
 * @param[in]     now_ms                    Current monotonic timestamp in milliseconds.
 * @param[out]    out_send_stage1_block_ack Set to true if Lower Transport Block ACK should be sent.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_router_stage1_ingress(meshx_router_ctx_t *ctx,
                                        const meshx_router_stage1_pdu_t *pdu,
                                        uint32_t now_ms,
                                        bool *out_send_stage1_block_ack)
{
    if (out_send_stage1_block_ack) {
        *out_send_stage1_block_ack = false;
    }
    if (!ctx || !pdu || !out_send_stage1_block_ack || pdu->ttl <= 1U) {
        return MESHX_INVALID_ARG;
    }

    meshx_router_enqueue_params_t params;
    meshx_err_t err = build_stage1_enqueue_params(pdu, now_ms, &params);
    if (err != MESHX_SUCCESS) {
        return err;
    }

    bool should_nack = false;
    err = meshx_router_queue_push(&ctx->queue, &params, &should_nack);
    if (err == MESHX_SUCCESS) {
        *out_send_stage1_block_ack = true;
        return MESHX_SUCCESS;
    }
    if (should_nack) {
        meshx_err_t nack_err = maybe_send_emergency_nack(ctx, pdu, &params, now_ms);
        if (nack_err != MESHX_SUCCESS) {
            return nack_err;
        }
    }
    return err;
}

/**
 * @brief Pump Stage 2 egress queue (max 3 retries, reliable NACK if all fail).
 *
 * @param[in,out] ctx    Pointer to router context.
 * @param[in]     now_ms Current monotonic timestamp in milliseconds.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_router_stage2_pump(meshx_router_ctx_t *ctx, uint32_t now_ms)
{
    if (!ctx) {
        return MESHX_INVALID_ARG;
    }

    if (ctx->pending_nack.active &&
        (now_ms - ctx->pending_nack.last_tx_ms) >= MESHX_SUBNET_NACK_TIMEOUT_MS) {
        if (ctx->pending_nack.retry_count < MESHX_STAGE2_MAX_RETRIES) {
            ctx->pending_nack.retry_count++;
            ctx->pending_nack.last_tx_ms = now_ms;
            meshx_err_t nack_err = ctx->nack_cb
                ? ctx->nack_cb(ctx->pending_nack.return_net_idx,
                               ctx->pending_nack.return_dst_addr,
                               ctx->pending_nack.payload,
                               (uint8_t)sizeof(ctx->pending_nack.payload),
                               ctx->cb_ctx)
                : MESHX_SUCCESS;
            if (nack_err != MESHX_SUCCESS) {
                return nack_err;
            }
        } else {
            ctx->pending_nack.active = false;
        }
    }

    meshx_router_queue_entry_t *head = NULL;
    if (meshx_router_queue_peek_tx(&ctx->queue, &head) == MESHX_SUCCESS && head) {
        head->rx_timestamp_ms = now_ms;
        return dispatch_entry_stage2(ctx, head);
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Handle Stage 2 completion notification from BLE Mesh Lower SAR.
 *
 * Advances the queue on ACK, retries Stage 2 dispatch up to
 * `MESHX_STAGE2_MAX_RETRIES`, or emits a reliable `0xD7` Router NACK once
 * retries are exhausted.
 *
 * @param[in,out] ctx    Pointer to router context.
 * @param[in]     ack_ok true if Lower Transport Block ACK was received, false on timeout/abort.
 * @param[in]     now_ms Current monotonic timestamp in milliseconds.
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG, MESHX_NOT_FOUND, or error code.
 */
meshx_err_t meshx_router_stage2_on_tx_complete(meshx_router_ctx_t *ctx,
                                               bool ack_ok,
                                               uint32_t now_ms)
{
    if (!ctx) {
        return MESHX_INVALID_ARG;
    }
    meshx_router_queue_entry_t *head = NULL;
    meshx_err_t perr = meshx_router_queue_peek_inflight(&ctx->queue, &head);
    if (perr != MESHX_SUCCESS || !head) {
        return (perr == MESHX_INVALID_ARG) ? perr : MESHX_NOT_FOUND;
    }

    if (ack_ok) {
        meshx_err_t err = meshx_router_queue_complete_entry(&ctx->queue, head);
        if (err != MESHX_SUCCESS) {
            return err;
        }
        meshx_router_queue_entry_t *next = NULL;
        if (meshx_router_queue_peek_tx(&ctx->queue, &next) == MESHX_SUCCESS && next) {
            next->rx_timestamp_ms = now_ms;
            return dispatch_entry_stage2(ctx, next);
        }
        return MESHX_SUCCESS;
    }

    head->retry_count++;
    if (head->retry_count < MESHX_STAGE2_MAX_RETRIES) {
        head->rx_timestamp_ms = now_ms;
        return dispatch_entry_stage2(ctx, head);
    }

    /* Retries exhausted: record reliable Stage 2 NACK, free flow, and send 0xD7 */
    uint16_t ret_net_idx = head->tx_net_idx;
    uint16_t ret_dst     = head->ingress_src_addr;
    ctx->pending_nack.nack_id         = ctx->next_nack_id++;
    ctx->pending_nack.return_net_idx  = ret_net_idx;
    ctx->pending_nack.return_dst_addr = ret_dst;
    ctx->pending_nack.last_tx_ms      = now_ms;
    ctx->pending_nack.retry_count     = 0U;
    ctx->pending_nack.active          = true;
    meshx_err_t nack_build_err = build_nack_payload(ctx->pending_nack.nack_id,
                                                    head,
                                                    MESHX_NACK_REASON_STAGE2_FAIL,
                                                    (meshx_uvp_router_nack_t *)(void *)ctx->pending_nack.payload);
    if (nack_build_err != MESHX_SUCCESS) {
        return nack_build_err;
    }

    meshx_err_t q_err = meshx_router_queue_complete_entry(&ctx->queue, head);
    if (q_err != MESHX_SUCCESS) {
        return q_err;
    }

    if (ctx->nack_cb) {
        return ctx->nack_cb(ret_net_idx,
                            ret_dst,
                            ctx->pending_nack.payload,
                            (uint8_t)sizeof(ctx->pending_nack.payload),
                            ctx->cb_ctx);
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Process 0xD9 Router NACK ACK to stop reliable NACK retransmission.
 *
 * @param[in,out] ctx     Pointer to router context.
 * @param[in]     nack_id Acknowledged NACK identifier.
 * @return MESHX_SUCCESS on success, or MESHX_NOT_FOUND.
 */
meshx_err_t meshx_router_on_nack_ack(meshx_router_ctx_t *ctx, uint32_t nack_id)
{
    if (!ctx) {
        return MESHX_INVALID_ARG;
    }
    if (ctx->pending_nack.active && ctx->pending_nack.nack_id == nack_id) {
        ctx->pending_nack.active = false;
        return MESHX_SUCCESS;
    }
    return MESHX_NOT_FOUND;
}
