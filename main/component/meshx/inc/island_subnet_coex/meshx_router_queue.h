/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_router_queue.h
 * @brief Bounded 4-Flow Single-Block Forwarding Queue & Emergency NACK TX Buffer.
 */

#ifndef MESHX_ROUTER_QUEUE_H
#define MESHX_ROUTER_QUEUE_H

#include "island_subnet_coex/meshx_subnet_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Forward-declared queue entry: header + flexible trailing PDU[] */
typedef struct MESHX_PACKED meshx_router_queue_entry {
    struct meshx_router_queue_entry *next;
    uint32_t ingress_boundary_id;
    uint32_t egress_boundary_id;
    uint32_t seq_auth;
    uint32_t rx_timestamp_ms;
    uint8_t  trans_mic[8];
    uint16_t canonical_src;
    uint16_t canonical_dst;
    uint16_t orig_src_addr;
    uint16_t orig_dst_addr;
    uint16_t ingress_src_addr;
    uint16_t tx_net_idx;
    uint16_t epoch_id;
    uint8_t  pdu_len;
    uint8_t  ttl;
    uint8_t  seg_tx_idx;
    uint8_t  retry_count;
    uint8_t  trans_mic_len;
    uint8_t  tx_epoch;
    uint8_t  flags;
    uint8_t  reserved;
    uint8_t  pdu[];
} meshx_router_queue_entry_t;

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(void *) != 4 || sizeof(meshx_router_queue_entry_t) <= 52,
               "32-bit MCU router queue entry header must be <= 52 bytes");
#endif

/** Queue manager state */
typedef struct {
    meshx_router_queue_entry_t *head;
    meshx_router_queue_entry_t *tail;
    uint8_t                     count;
    uint8_t                     max_capacity;
    uint8_t                     active_sar_flow_present;
    uint32_t                    dropped_newest_cnt;
    uint32_t                    purged_cnt;
    uint8_t                     emergency_nack_buf[MESHX_EMERGENCY_NACK_BUF_LEN];
    uint8_t                     emergency_nack_len;
    uint8_t                     emergency_nack_pending;
} meshx_router_queue_t;

/** Parameters for enqueuing a validated Stage 1 PDU */
typedef struct {
    uint32_t       ingress_boundary_id;
    uint32_t       egress_boundary_id;
    uint32_t       seq_auth;
    uint32_t       now_ms;
    uint16_t       canonical_src;
    uint16_t       canonical_dst;
    uint16_t       orig_src_addr;
    uint16_t       orig_dst_addr;
    uint16_t       ingress_src_addr;
    uint16_t       tx_net_idx;
    uint16_t       epoch_id;
    uint8_t        ttl;
    uint8_t        trans_mic_len;
    const uint8_t *trans_mic;
    uint8_t        tx_epoch;
    uint8_t        flags;
    const uint8_t *pdu;
    uint8_t        pdu_len;
} meshx_router_enqueue_params_t;

void meshx_router_queue_init(meshx_router_queue_t *q);
void meshx_router_queue_deinit(meshx_router_queue_t *q);

/**
 * @brief Allocate single block and enqueue. Enforces max PDU bounds (108B or 85B multihop)
 *        and pre-ACK Drop-Newest backpressure into emergency NACK buffer when count == capacity.
 */
meshx_err_t meshx_router_queue_push(meshx_router_queue_t *q,
                                    const meshx_router_enqueue_params_t *params,
                                    bool *out_should_nack_stage1);

/** Peek head flow if eligible for Stage 2 SAR TX */
meshx_router_queue_entry_t *meshx_router_queue_peek_tx(meshx_router_queue_t *q);

/** Mark head as TX in-flight (serializes Stage 2 SAR per flow) */
meshx_err_t meshx_router_queue_mark_tx_inflight(meshx_router_queue_t *q, meshx_router_queue_entry_t *entry);

/** Complete, scrub payload memory to zero, and free a specific queue entry */
meshx_err_t meshx_router_queue_complete_entry(meshx_router_queue_t *q, meshx_router_queue_entry_t *entry);

/** Purge all queued entries whose ingress or egress matches boundary_id (returns count freed) */
uint8_t meshx_router_queue_purge_boundary(meshx_router_queue_t *q, uint32_t boundary_id);

/** Format a 0xD7 Emergency NACK into the static 64-byte buffer with zero heap allocation */
meshx_err_t meshx_router_queue_build_emergency_nack(meshx_router_queue_t *q,
                                                    uint32_t nack_id,
                                                    const meshx_router_enqueue_params_t *params,
                                                    uint8_t reason_code,
                                                    const uint8_t **out_buf,
                                                    uint8_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* MESHX_ROUTER_QUEUE_H */

