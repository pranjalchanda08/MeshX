/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_router_core.c
 * @brief 2-Stage Decoupled Store-and-Forward Bridge Router & Canonical Nonce Engine.
 */

#include <string.h>
#include "island_subnet_coex/meshx_router_core.h"

static void build_nack_payload(uint32_t nack_id,
                               const meshx_router_queue_entry_t *entry,
                               uint8_t reason_code,
                               uint8_t out_buf[sizeof(meshx_uvp_router_nack_t)])
{
    meshx_uvp_router_nack_t nack;
    memset(&nack, 0, sizeof(nack));
    nack.nack_id           = nack_id;
    nack.orig_seq_auth     = entry->seq_auth;
    nack.global_subnet_src = entry->ingress_boundary_id;
    nack.global_subnet_dst = entry->egress_boundary_id;
    nack.orig_src_addr     = entry->orig_src_addr;
    nack.orig_dst_addr     = entry->orig_dst_addr;
    nack.reason_code       = reason_code;
    memcpy(out_buf, &nack, sizeof(nack));
}

static meshx_err_t dispatch_entry_stage2(meshx_router_ctx_t *ctx,
                                         meshx_router_queue_entry_t *entry)
{
    uint8_t nonce13[13];
    uint8_t szmic = (entry->trans_mic_len >= 8U) ? 1U : 0U;
    meshx_err_t err = meshx_router_build_canonical_nonce(entry->canonical_src,
                                                         entry->canonical_dst,
                                                         0U,
                                                         entry->seq_auth,
                                                         szmic,
                                                         true,
                                                         nonce13);
    if (err != MESHX_SUCCESS) {
        return err;
    }

    meshx_router_queue_mark_tx_inflight(&ctx->queue, entry);
    if (ctx->tx_cb) {
        return ctx->tx_cb(entry, nonce13, NULL, ctx->cb_ctx);
    }
    return MESHX_SUCCESS;
}

void meshx_router_core_init(meshx_router_ctx_t *ctx,
                            meshx_router_send_fn_t tx_cb,
                            meshx_router_nack_send_fn_t nack_cb,
                            void *cb_ctx)
{
    if (!ctx) {
        return;
    }
    memset(ctx, 0, sizeof(*ctx));
    meshx_router_queue_init(&ctx->queue);
    ctx->next_nack_id = 1UL;
    ctx->tx_cb        = tx_cb;
    ctx->nack_cb      = nack_cb;
    ctx->cb_ctx       = cb_ctx;
    ctx->initialized  = true;
}

void meshx_router_core_deinit(meshx_router_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }
    meshx_router_queue_deinit(&ctx->queue);
    memset(ctx, 0, sizeof(*ctx));
}

meshx_err_t meshx_router_build_canonical_nonce(uint16_t canonical_src,
                                               uint16_t canonical_dst,
                                               uint32_t iv_index,
                                               uint32_t seq_auth,
                                               uint8_t  szmic,
                                               bool     is_app_nonce,
                                               uint8_t  out_nonce[13])
{
    if (!out_nonce) {
        return MESHX_INVALID_ARG;
    }
    out_nonce[0]  = is_app_nonce ? 0x01U : 0x02U;
    out_nonce[1]  = (szmic != 0U) ? 0x80U : 0x00U;
    out_nonce[2]  = (uint8_t)((seq_auth >> 16) & 0xFFU);
    out_nonce[3]  = (uint8_t)((seq_auth >> 8) & 0xFFU);
    out_nonce[4]  = (uint8_t)(seq_auth & 0xFFU);
    out_nonce[5]  = (uint8_t)((canonical_src >> 8) & 0xFFU);
    out_nonce[6]  = (uint8_t)(canonical_src & 0xFFU);
    out_nonce[7]  = (uint8_t)((canonical_dst >> 8) & 0xFFU);
    out_nonce[8]  = (uint8_t)(canonical_dst & 0xFFU);
    out_nonce[9]  = (uint8_t)((iv_index >> 24) & 0xFFU);
    out_nonce[10] = (uint8_t)((iv_index >> 16) & 0xFFU);
    out_nonce[11] = (uint8_t)((iv_index >> 8) & 0xFFU);
    out_nonce[12] = (uint8_t)(iv_index & 0xFFU);
    return MESHX_SUCCESS;
}

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
                                       bool    *out_complete)
{
    if (out_complete) {
        *out_complete = false;
    }
    if (!ctx || !seg_data || !out_complete || seg_n >= 32U || seg_o > seg_n) {
        return MESHX_INVALID_ARG;
    }
    size_t offset = (size_t)seg_o * 12U;
    if (offset + (size_t)seg_len > MESHX_MAX_UPPER_PDU_LEN) {
        return (meshx_err_t)MESHX_SUBNET_ERR_MTU;
    }

    if (ctx->reasm.in_use) {
        bool same_flow = (ctx->reasm.net_idx == net_idx &&
                          ctx->reasm.src_addr == src_addr &&
                          ctx->reasm.dst_addr == dst_addr &&
                          ctx->reasm.seq_zero == seq_zero);
        if (same_flow && ctx->reasm.epoch_id != epoch_id) {
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
        ctx->reasm.net_idx  = net_idx;
        ctx->reasm.src_addr = src_addr;
        ctx->reasm.dst_addr = dst_addr;
        ctx->reasm.epoch_id = epoch_id;
        ctx->reasm.seq_zero = seq_zero;
        ctx->reasm.seg_n    = seg_n;
    }

    if (seg_len > 0U) {
        memcpy(&ctx->reasm.buf[offset], seg_data, seg_len);
        uint8_t end_pos = (uint8_t)(offset + seg_len);
        if (end_pos > ctx->reasm.len) {
            ctx->reasm.len = end_pos;
        }
    }
    ctx->reasm.rx_mask |= (1UL << seg_o);

    uint32_t expected_mask = (seg_n == 31U) ? 0xFFFFFFFFUL : ((1UL << (seg_n + 1U)) - 1UL);
    if ((ctx->reasm.rx_mask & expected_mask) == expected_mask) {
        ctx->reasm.in_use = false;
        *out_complete = true;
    }
    return MESHX_SUCCESS;
}

meshx_err_t meshx_router_stage1_ingress(meshx_router_ctx_t *ctx,
                                        const meshx_router_stage1_pdu_t *pdu,
                                        uint32_t now_ms,
                                        bool *out_send_stage1_block_ack)
{
    if (out_send_stage1_block_ack) {
        *out_send_stage1_block_ack = false;
    }
    if (!ctx || !pdu || !out_send_stage1_block_ack) {
        return MESHX_INVALID_ARG;
    }
    if (pdu->ttl <= 1U) {
        return MESHX_INVALID_ARG;
    }

    meshx_router_enqueue_params_t params;
    memset(&params, 0, sizeof(params));
    params.ingress_boundary_id = (uint32_t)pdu->net_idx;
    params.egress_boundary_id  = pdu->global_subnet_dst;
    params.seq_auth            = pdu->seq_auth;
    params.now_ms              = now_ms;
    params.canonical_src       = pdu->src_addr;
    params.canonical_dst       = pdu->dst_addr;
    params.orig_src_addr       = pdu->src_addr;
    params.orig_dst_addr       = pdu->dst_addr;
    params.ingress_src_addr    = pdu->src_addr;
    params.tx_net_idx          = pdu->net_idx;
    params.ttl                 = (uint8_t)(pdu->ttl - 1U);
    params.trans_mic_len       = (pdu->szmic != 0U) ? 8U : 4U;
    params.trans_mic           = pdu->trans_mic;
    params.pdu                 = pdu->pdu;
    params.pdu_len             = pdu->pdu_len;
    params.flags               = (pdu->dst_addr < 0x8000U ? MESHX_ROUTER_FLAG_IS_UNICAST : 0U) |
                                 (pdu->is_multihop_transit ? MESHX_ROUTER_FLAG_MULTIHOP : 0U);

    bool should_nack = false;
    meshx_err_t err = meshx_router_queue_push(&ctx->queue, &params, &should_nack);
    if (err != MESHX_SUCCESS) {
        if (should_nack && ctx->nack_cb) {
            const uint8_t *nack_buf = NULL;
            uint8_t nack_len = 0U;
            uint32_t nack_id = ctx->next_nack_id++;
            if (meshx_router_queue_build_emergency_nack(&ctx->queue,
                                                        nack_id,
                                                        &params,
                                                        MESHX_NACK_REASON_QUEUE_FULL,
                                                        &nack_buf,
                                                        &nack_len) == MESHX_SUCCESS) {
                (void)ctx->nack_cb(pdu->net_idx, pdu->src_addr, nack_buf, nack_len, ctx->cb_ctx);
            }
        }
        return err;
    }

    *out_send_stage1_block_ack = true;
    return MESHX_SUCCESS;
}

meshx_err_t meshx_router_stage2_pump(meshx_router_ctx_t *ctx, uint32_t now_ms)
{
    if (!ctx) {
        return MESHX_INVALID_ARG;
    }

    if (ctx->pending_nack.active) {
        if ((now_ms - ctx->pending_nack.last_tx_ms) >= MESHX_SUBNET_NACK_TIMEOUT_MS) {
            if (ctx->pending_nack.retry_count < MESHX_STAGE2_MAX_RETRIES) {
                ctx->pending_nack.retry_count++;
                ctx->pending_nack.last_tx_ms = now_ms;
                if (ctx->nack_cb) {
                    (void)ctx->nack_cb(ctx->pending_nack.return_net_idx,
                                       ctx->pending_nack.return_dst_addr,
                                       ctx->pending_nack.payload,
                                       (uint8_t)sizeof(ctx->pending_nack.payload),
                                       ctx->cb_ctx);
                }
            } else {
                ctx->pending_nack.active = false;
            }
        }
    }

    meshx_router_queue_entry_t *head = meshx_router_queue_peek_tx(&ctx->queue);
    if (head && (head->flags & MESHX_ROUTER_FLAG_TX_IN_FLIGHT) == 0U) {
        head->rx_timestamp_ms = now_ms;
        return dispatch_entry_stage2(ctx, head);
    }

    return MESHX_SUCCESS;
}

meshx_err_t meshx_router_stage2_on_tx_complete(meshx_router_ctx_t *ctx,
                                               bool ack_ok,
                                               uint32_t now_ms)
{
    if (!ctx) {
        return MESHX_INVALID_ARG;
    }
    meshx_router_queue_entry_t *head = meshx_router_queue_peek_tx(&ctx->queue);
    if (!head) {
        return MESHX_NOT_FOUND;
    }

    if (ack_ok) {
        meshx_err_t err = meshx_router_queue_complete_entry(&ctx->queue, head);
        if (err != MESHX_SUCCESS) {
            return err;
        }
        meshx_router_queue_entry_t *next = meshx_router_queue_peek_tx(&ctx->queue);
        if (next && (next->flags & MESHX_ROUTER_FLAG_TX_IN_FLIGHT) == 0U) {
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
    build_nack_payload(ctx->pending_nack.nack_id,
                       head,
                       MESHX_NACK_REASON_STAGE2_FAIL,
                       ctx->pending_nack.payload);

    (void)meshx_router_queue_complete_entry(&ctx->queue, head);

    if (ctx->nack_cb) {
        return ctx->nack_cb(ret_net_idx,
                            ret_dst,
                            ctx->pending_nack.payload,
                            (uint8_t)sizeof(ctx->pending_nack.payload),
                            ctx->cb_ctx);
    }
    return MESHX_SUCCESS;
}

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

