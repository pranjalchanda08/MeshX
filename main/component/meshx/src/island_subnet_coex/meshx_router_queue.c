/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_router_queue.c
 * @brief Bounded 4-Flow Single-Block Forwarding Queue & Emergency NACK TX Buffer.
 */

#include <string.h>
#include "island_subnet_coex/meshx_router_queue.h"
#include "interface/rtos/meshx_rtos_utils.h"

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(meshx_uvp_router_nack_t) <= MESHX_EMERGENCY_NACK_BUF_LEN,
               "Emergency NACK must fit in static 64B buffer");
#endif

static void wipe_and_free_entry(meshx_router_queue_entry_t *entry)
{
    if (!entry) {
        return;
    }
    size_t total = sizeof(*entry) + (size_t)entry->pdu_len;
    volatile uint8_t *vp = (volatile uint8_t *)entry;
    for (size_t i = 0; i < total; i++) {
        vp[i] = 0U;
    }
    void *ptr = (void *)entry;
    (void)MESHX_FREE(&ptr);
}

static void refresh_active_sar_flag(meshx_router_queue_t *q)
{
    q->active_sar_flow_present = 0U;
    for (meshx_router_queue_entry_t *c = q->head; c != NULL; c = c->next) {
        if ((c->flags & MESHX_ROUTER_FLAG_TX_IN_FLIGHT) != 0U) {
            q->active_sar_flow_present = 1U;
            break;
        }
    }
}

void meshx_router_queue_init(meshx_router_queue_t *q)
{
    if (!q) {
        return;
    }
    memset(q, 0, sizeof(*q));
    q->max_capacity = MESHX_ROUTER_QUEUE_CAPACITY;
}

void meshx_router_queue_deinit(meshx_router_queue_t *q)
{
    if (!q) {
        return;
    }
    meshx_router_queue_entry_t *cur = q->head;
    while (cur) {
        meshx_router_queue_entry_t *next = cur->next;
        wipe_and_free_entry(cur);
        cur = next;
    }
    memset(q, 0, sizeof(*q));
}

meshx_err_t meshx_router_queue_push(meshx_router_queue_t *q,
                                    const meshx_router_enqueue_params_t *params,
                                    bool *out_should_nack_stage1)
{
    if (out_should_nack_stage1) {
        *out_should_nack_stage1 = false;
    }
    if (!q || !params) {
        return MESHX_INVALID_ARG;
    }

    uint8_t max_pdu = ((params->flags & MESHX_ROUTER_FLAG_MULTIHOP) != 0U)
                          ? MESHX_MAX_MULTIHOP_APP_PDU_LEN
                          : MESHX_MAX_UPPER_PDU_LEN;
    if (params->pdu_len > max_pdu) {
        return (meshx_err_t)MESHX_SUBNET_ERR_MTU;
    }
    if (params->pdu_len > 0U && !params->pdu) {
        return MESHX_INVALID_ARG;
    }

    uint8_t cap = (q->max_capacity > 0U) ? q->max_capacity : MESHX_ROUTER_QUEUE_CAPACITY;
    if (q->count >= cap) {
        q->dropped_newest_cnt++;
        if (out_should_nack_stage1) {
            *out_should_nack_stage1 = true;
        }
        return MESHX_NO_MEM;
    }

    meshx_router_queue_entry_t *entry = NULL;
    size_t alloc_sz = sizeof(meshx_router_queue_entry_t) + (size_t)params->pdu_len;
    if (MESHX_MALLOC(&entry, alloc_sz) != MESHX_SUCCESS || !entry) {
        q->dropped_newest_cnt++;
        if (out_should_nack_stage1) {
            *out_should_nack_stage1 = true;
        }
        return MESHX_NO_MEM;
    }

    memset(entry, 0, sizeof(*entry));
    entry->ingress_boundary_id = params->ingress_boundary_id;
    entry->egress_boundary_id  = params->egress_boundary_id;
    entry->seq_auth            = params->seq_auth;
    entry->rx_timestamp_ms     = params->now_ms;
    entry->canonical_src       = params->canonical_src;
    entry->canonical_dst       = params->canonical_dst;
    entry->orig_src_addr       = params->orig_src_addr;
    entry->orig_dst_addr       = params->orig_dst_addr;
    entry->ingress_src_addr    = params->ingress_src_addr;
    entry->tx_net_idx          = params->tx_net_idx;
    entry->epoch_id            = params->epoch_id;
    entry->pdu_len             = params->pdu_len;
    entry->ttl                 = params->ttl;
    entry->tx_epoch            = params->tx_epoch;
    entry->flags               = params->flags;

    if (params->trans_mic && params->trans_mic_len > 0U) {
        uint8_t mic_copy = (params->trans_mic_len > 8U) ? 8U : params->trans_mic_len;
        entry->trans_mic_len = mic_copy;
        memcpy(entry->trans_mic, params->trans_mic, mic_copy);
    }
    if (params->pdu_len > 0U && params->pdu) {
        memcpy(entry->pdu, params->pdu, params->pdu_len);
    }

    entry->next = NULL;
    if (!q->head) {
        q->head = entry;
        q->tail = entry;
    } else {
        q->tail->next = entry;
        q->tail = entry;
    }
    q->count++;
    return MESHX_SUCCESS;
}

meshx_router_queue_entry_t *meshx_router_queue_peek_tx(meshx_router_queue_t *q)
{
    if (!q) {
        return NULL;
    }
    return q->head;
}

meshx_err_t meshx_router_queue_mark_tx_inflight(meshx_router_queue_t *q,
                                                meshx_router_queue_entry_t *entry)
{
    if (!q || !entry) {
        return MESHX_INVALID_ARG;
    }
    entry->flags |= MESHX_ROUTER_FLAG_TX_IN_FLIGHT;
    q->active_sar_flow_present = 1U;
    return MESHX_SUCCESS;
}

meshx_err_t meshx_router_queue_complete_entry(meshx_router_queue_t *q,
                                              meshx_router_queue_entry_t *entry)
{
    if (!q || !entry) {
        return MESHX_INVALID_ARG;
    }

    meshx_router_queue_entry_t *prev = NULL;
    meshx_router_queue_entry_t *cur = q->head;
    while (cur && cur != entry) {
        prev = cur;
        cur = cur->next;
    }
    if (!cur) {
        return MESHX_NOT_FOUND;
    }

    if (!prev) {
        q->head = cur->next;
    } else {
        prev->next = cur->next;
    }
    if (q->tail == cur) {
        q->tail = prev;
    }
    if (q->count > 0U) {
        q->count--;
    }

    wipe_and_free_entry(cur);
    refresh_active_sar_flag(q);
    return MESHX_SUCCESS;
}

uint8_t meshx_router_queue_purge_boundary(meshx_router_queue_t *q, uint32_t boundary_id)
{
    if (!q) {
        return 0U;
    }

    uint8_t removed = 0U;
    meshx_router_queue_entry_t *prev = NULL;
    meshx_router_queue_entry_t *cur = q->head;

    while (cur) {
        meshx_router_queue_entry_t *next = cur->next;
        if (cur->ingress_boundary_id == boundary_id ||
            cur->egress_boundary_id == boundary_id) {
            if (!prev) {
                q->head = next;
            } else {
                prev->next = next;
            }
            if (q->tail == cur) {
                q->tail = prev;
            }
            if (q->count > 0U) {
                q->count--;
            }
            wipe_and_free_entry(cur);
            removed++;
            q->purged_cnt++;
        } else {
            prev = cur;
        }
        cur = next;
    }

    refresh_active_sar_flag(q);
    return removed;
}

meshx_err_t meshx_router_queue_build_emergency_nack(meshx_router_queue_t *q,
                                                    uint32_t nack_id,
                                                    const meshx_router_enqueue_params_t *params,
                                                    uint8_t reason_code,
                                                    const uint8_t **out_buf,
                                                    uint8_t *out_len)
{
    if (!q || !params || !out_buf || !out_len) {
        return MESHX_INVALID_ARG;
    }

    meshx_uvp_router_nack_t nack;
    memset(&nack, 0, sizeof(nack));
    nack.nack_id           = nack_id;
    nack.orig_seq_auth     = params->seq_auth;
    nack.global_subnet_src = params->ingress_boundary_id;
    nack.global_subnet_dst = params->egress_boundary_id;
    nack.orig_src_addr     = params->orig_src_addr;
    nack.orig_dst_addr     = params->orig_dst_addr;
    nack.reason_code       = reason_code;

    memcpy(q->emergency_nack_buf, &nack, sizeof(nack));
    q->emergency_nack_len     = (uint8_t)sizeof(nack);
    q->emergency_nack_pending = 1U;

    *out_buf = q->emergency_nack_buf;
    *out_len = q->emergency_nack_len;
    return MESHX_SUCCESS;
}

