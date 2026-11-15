/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_router_queue.c
 * @brief Bounded 4-Flow Single-Block Forwarding Queue & Emergency NACK TX Buffer.
 */

#include <string.h>
#include "island/meshx_router_queue.h"
#include "interface/rtos/meshx_rtos_utils.h"

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(meshx_uvp_router_nack_t) <= MESHX_EMERGENCY_NACK_BUF_LEN,
               "Emergency NACK must fit in static 64B buffer");
#endif

/**
 * @brief Securely wipe payload and metadata bytes of a queue entry and free memory.
 *
 * @param[in] entry Pointer to dynamically allocated queue entry to wipe and release.
 */
static void wipe_and_free_entry(meshx_router_queue_entry_t *entry)
{
    if (!entry)
    {
        return;
    }
    size_t total = sizeof(*entry) + (size_t)entry->pdu_len;
    volatile uint8_t *vp = (volatile uint8_t *)entry;
    for (size_t i = 0; i < total; i++)
    {
        vp[i] = 0U;
    }
    void *ptr = (void *)entry;
    (void)MESHX_FREE(&ptr);
}

/**
 * @brief Recalculate whether any active SAR flow is currently transmitting.
 *
 * @param[in,out] q Pointer to router queue.
 */
static void refresh_active_sar_flag(meshx_router_queue_t *q)
{
    q->active_sar_flow_present = 0U;
    for (meshx_router_queue_entry_t *c = q->head; c != NULL; c = c->next)
    {
        if ((c->flags & MESHX_ROUTER_FLAG_TX_IN_FLIGHT) != 0U)
        {
            q->active_sar_flow_present = 1U;
            break;
        }
    }
}

/**
 * @brief Initialize router queue state to empty.
 *
 * @param[out] q Pointer to queue descriptor.
 */
void meshx_router_queue_init(meshx_router_queue_t *q)
{
    if (!q)
    {
        return;
    }
    memset(q, 0, sizeof(*q));
    q->max_capacity = MESHX_ROUTER_QUEUE_CAPACITY;
}

/**
 * @brief De-initialize router queue, wiping and freeing all pending entries.
 *
 * @param[in,out] q Pointer to queue descriptor.
 */
void meshx_router_queue_deinit(meshx_router_queue_t *q)
{
    if (!q)
    {
        return;
    }
    meshx_router_queue_entry_t *cur = q->head;
    while (cur)
    {
        meshx_router_queue_entry_t *next = cur->next;
        wipe_and_free_entry(cur);
        cur = next;
    }
    memset(q, 0, sizeof(*q));
}

/**
 * @brief Allocate single block and enqueue a validated Stage 1 packet.
 *
 * @param[in,out] q                     Pointer to router queue.
 * @param[in]     params                Validated ingress packet parameters.
 * @param[out]    out_should_nack_stage1 Set to true if drop-newest triggered emergency NACK.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_router_queue_push(meshx_router_queue_t *q,
                                    const meshx_router_enqueue_params_t *params,
                                    bool *out_should_nack_stage1)
{
    if (out_should_nack_stage1)
    {
        *out_should_nack_stage1 = false;
    }
    if (!q || !params)
    {
        return MESHX_INVALID_ARG;
    }

    uint8_t max_pdu = ((params->flags & MESHX_ROUTER_FLAG_MULTIHOP) != 0U)
                          ? MESHX_MAX_MULTIHOP_APP_PDU_LEN
                          : MESHX_MAX_UPPER_PDU_LEN;
    if (params->pdu_len > max_pdu)
    {
        return (meshx_err_t)MESHX_SUBNET_ERR_MTU;
    }
    if (params->pdu_len > 0U && !params->pdu)
    {
        return MESHX_INVALID_ARG;
    }

    uint8_t cap = (q->max_capacity > 0U) ? q->max_capacity : MESHX_ROUTER_QUEUE_CAPACITY;
    if (q->count >= cap)
    {
        q->dropped_newest_cnt++;
        if (out_should_nack_stage1)
        {
            *out_should_nack_stage1 = true;
        }
        return MESHX_NO_MEM;
    }

    meshx_router_queue_entry_t *entry = NULL;
    size_t alloc_sz = sizeof(meshx_router_queue_entry_t) + (size_t)params->pdu_len;
    if (MESHX_MALLOC(&entry, alloc_sz) != MESHX_SUCCESS || !entry)
    {
        q->dropped_newest_cnt++;
        if (out_should_nack_stage1)
        {
            *out_should_nack_stage1 = true;
        }
        return MESHX_NO_MEM;
    }

    memset(entry, 0, sizeof(*entry));
    entry->ingress_boundary_id = params->ingress_boundary_id;
    entry->egress_boundary_id = params->egress_boundary_id;
    entry->seq_auth = params->seq_auth;
    entry->rx_timestamp_ms = params->now_ms;
    entry->canonical_src = params->canonical_src;
    entry->canonical_dst = params->canonical_dst;
    entry->orig_src_addr = params->orig_src_addr;
    entry->orig_dst_addr = params->orig_dst_addr;
    entry->ingress_src_addr = params->ingress_src_addr;
    entry->tx_net_idx = params->tx_net_idx;
    entry->epoch_id = params->epoch_id;
    entry->pdu_len = params->pdu_len;
    entry->ttl = params->ttl;
    entry->tx_epoch = params->tx_epoch;
    entry->flags = params->flags;

    if (params->trans_mic.len > 0U)
    {
        uint8_t mic_copy = (params->trans_mic.len > MESHX_TRANS_MIC_MAX_LEN) ? MESHX_TRANS_MIC_MAX_LEN : params->trans_mic.len;
        entry->trans_mic.len = mic_copy;
        memcpy(entry->trans_mic.bytes, params->trans_mic.bytes, mic_copy);
    }
    if (params->pdu_len > 0U && params->pdu)
    {
        memcpy(entry->pdu, params->pdu, params->pdu_len);
    }

    entry->next = NULL;
    if (!q->head)
    {
        q->head = entry;
        q->tail = entry;
    }
    else
    {
        q->tail->next = entry;
        q->tail = entry;
    }
    q->count++;
    return MESHX_SUCCESS;
}

/**
 * @brief Peek head flow if eligible for Stage 2 SAR transmission.
 *
 * @param[in] q Pointer to router queue.
 * @return Pointer to head entry, or NULL if empty.
 */
meshx_router_queue_entry_t *meshx_router_queue_peek_tx(meshx_router_queue_t *q)
{
    if (!q)
    {
        return NULL;
    }
    return q->head;
}

/**
 * @brief Mark an entry as in-flight to serialize Stage 2 SAR per flow.
 *
 * @param[in,out] q     Pointer to router queue.
 * @param[in,out] entry Queue entry being dispatched.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG.
 */
meshx_err_t meshx_router_queue_mark_tx_inflight(meshx_router_queue_t *q,
                                                meshx_router_queue_entry_t *entry)
{
    if (!q || !entry)
    {
        return MESHX_INVALID_ARG;
    }
    entry->flags |= MESHX_ROUTER_FLAG_TX_IN_FLIGHT;
    q->active_sar_flow_present = 1U;
    return MESHX_SUCCESS;
}

/**
 * @brief Complete transmission, scrub payload memory to zero, and free the queue entry.
 *
 * @param[in,out] q     Pointer to router queue.
 * @param[in]     entry Entry to remove and release.
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG or MESHX_NOT_FOUND.
 */
meshx_err_t meshx_router_queue_complete_entry(meshx_router_queue_t *q,
                                              meshx_router_queue_entry_t *entry)
{
    if (!q || !entry)
    {
        return MESHX_INVALID_ARG;
    }

    meshx_router_queue_entry_t *prev = NULL;
    meshx_router_queue_entry_t *cur = q->head;
    while (cur && cur != entry)
    {
        prev = cur;
        cur = cur->next;
    }
    if (!cur)
    {
        return MESHX_NOT_FOUND;
    }

    if (!prev)
    {
        q->head = cur->next;
    }
    else
    {
        prev->next = cur->next;
    }
    if (q->tail == cur)
    {
        q->tail = prev;
    }
    if (q->count > 0U)
    {
        q->count--;
    }

    wipe_and_free_entry(cur);
    refresh_active_sar_flag(q);
    return MESHX_SUCCESS;
}

/**
 * @brief Purge all queued entries whose ingress or egress matches boundary_id.
 *
 * @param[in,out] q           Pointer to router queue.
 * @param[in]     boundary_id Global Subnet ID to purge.
 * @return Number of entries freed.
 */
uint8_t meshx_router_queue_purge_boundary(meshx_router_queue_t *q, uint32_t boundary_id)
{
    if (!q)
    {
        return 0U;
    }

    uint8_t removed = 0U;
    meshx_router_queue_entry_t *prev = NULL;
    meshx_router_queue_entry_t *cur = q->head;

    while (cur)
    {
        meshx_router_queue_entry_t *next = cur->next;
        if (cur->ingress_boundary_id == boundary_id ||
            cur->egress_boundary_id == boundary_id)
        {
            if (!prev)
            {
                q->head = next;
            }
            else
            {
                prev->next = next;
            }
            if (q->tail == cur)
            {
                q->tail = prev;
            }
            if (q->count > 0U)
            {
                q->count--;
            }
            wipe_and_free_entry(cur);
            removed++;
            q->purged_cnt++;
        }
        else
        {
            prev = cur;
        }
        cur = next;
    }

    refresh_active_sar_flag(q);
    return removed;
}

/**
 * @brief Format a 0xD7 Emergency NACK into the static 64-byte buffer with zero heap allocation.
 *
 * @param[in,out] q           Pointer to router queue.
 * @param[in]     nack_id     Unique NACK identifier.
 * @param[in]     params      Encountered packet parameters that caused the drop.
 * @param[in]     reason_code Drop reason code (MESHX_NACK_REASON_*).
 * @param[out]    out_buf     Pointer set to static emergency buffer address.
 * @param[out]    out_len     Length of formatted emergency NACK frame.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_router_queue_build_emergency_nack(meshx_router_queue_t *q,
                                                    uint32_t nack_id,
                                                    const meshx_router_enqueue_params_t *params,
                                                    uint8_t reason_code,
                                                    const uint8_t **out_buf,
                                                    uint8_t *out_len)
{
    if (!q || !params || !out_buf || !out_len)
    {
        return MESHX_INVALID_ARG;
    }

    meshx_uvp_router_nack_t nack;
    memset(&nack, 0, sizeof(nack));
    nack.nack_id            = nack_id;
    nack.orig_seq_auth      = params->seq_auth;
    nack.global_subnet_src  = params->ingress_boundary_id;
    nack.global_subnet_dst  = params->egress_boundary_id;
    nack.orig_src_addr      = params->orig_src_addr;
    nack.orig_dst_addr      = params->orig_dst_addr;
    nack.reason_code        = reason_code;

    memcpy(q->emergency_nack_buf, &nack, sizeof(nack));
    q->emergency_nack_len = (uint8_t)sizeof(nack);
    q->emergency_nack_pending = 1U;

    *out_buf = q->emergency_nack_buf;
    *out_len = q->emergency_nack_len;
    return MESHX_SUCCESS;
}
