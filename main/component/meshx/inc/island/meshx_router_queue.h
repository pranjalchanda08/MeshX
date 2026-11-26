/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_router_queue.h
 * @brief Bounded 4-Flow Single-Block Forwarding Queue & Emergency NACK TX Buffer.
 */

#ifndef MESHX_ROUTER_QUEUE_H
#define MESHX_ROUTER_QUEUE_H

#include "island/meshx_subnet_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Forward-declared queue entry: header + flexible trailing PDU[]
 */
typedef struct MESHX_PACKED meshx_router_queue_entry {
    struct meshx_router_queue_entry *next;                 /**< Intrusive singly-linked list pointer */
    uint32_t                        ingress_boundary_id;  /**< Ingress Global Subnet ID */
    uint32_t                        egress_boundary_id;   /**< Egress Global Subnet ID */
    uint32_t                        seq_auth;             /**< Authenticated original sequence number */
    uint32_t                        rx_timestamp_ms;      /**< Monotonic timestamp at ingress */
    meshx_trans_mic_t               trans_mic;            /**< Preserved Upper Transport MIC */
    uint16_t                        canonical_src;        /**< Canonical source unicast address */
    uint16_t                        canonical_dst;        /**< Canonical destination unicast/group address */
    uint16_t                        orig_src_addr;        /**< Original source address on ingress subnet */
    uint16_t                        orig_dst_addr;        /**< Original destination address on ingress subnet */
    uint16_t                        ingress_src_addr;     /**< Immediate transmitter address */
    uint16_t                        tx_net_idx;           /**< Target egress NetKey Index */
    uint16_t                        epoch_id;             /**< Key rotation epoch identifier */
    uint8_t                         pdu_len;              /**< Length of trailing payload */
    uint8_t                         ttl;                  /**< Time-to-Live decremented for Stage 2 */
    uint8_t                         seg_tx_idx;           /**< Current segment TX index */
    uint8_t                         retry_count;          /**< Stage 2 retransmission attempt counter */
    uint8_t                         tx_epoch;             /**< Transmit key epoch flag */
    uint8_t                         flags;                /**< MESHX_ROUTER_FLAG_* */
    uint8_t                         pdu[];                /**< Flexible array member holding Upper Transport PDU */
} meshx_router_queue_entry_t;

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(void *) != 4 || sizeof(meshx_router_queue_entry_t) <= 52,
               "32-bit MCU router queue entry header must be <= 52 bytes");
#endif

/**
 * @brief Bounded router queue state descriptor.
 */
typedef struct {
    meshx_router_queue_entry_t *head;                                             /**< Head of FIFO forwarding queue */
    meshx_router_queue_entry_t *tail;                                             /**< Tail of FIFO forwarding queue */
    uint8_t                     count;                                            /**< Current number of queued entries */
    uint8_t                     max_capacity;                                     /**< Maximum queue capacity (4) */
    uint8_t                     active_sar_flow_present;                          /**< 1 if a SAR burst is currently transmitting */
    uint32_t                    dropped_newest_cnt;                               /**< Drop-newest backpressure statistics counter */
    uint32_t                    purged_cnt;                                       /**< Demote / failover purged entries counter */
    uint8_t                     emergency_nack_buf[MESHX_EMERGENCY_NACK_BUF_LEN]; /**< Static 64B emergency NACK TX buffer */
    uint8_t                     emergency_nack_len;                               /**< Length of staged emergency NACK */
    uint8_t                     emergency_nack_pending;                           /**< 1 if emergency NACK is awaiting transmission */
} meshx_router_queue_t;

/**
 * @brief Parameters for enqueuing a validated Stage 1 PDU into the router queue.
 */
typedef struct {
    uint32_t          ingress_boundary_id; /**< Ingress Global Subnet ID */
    uint32_t          egress_boundary_id;  /**< Egress Global Subnet ID */
    uint32_t          seq_auth;            /**< Authenticated original sequence number */
    uint32_t          now_ms;              /**< Current monotonic timestamp in milliseconds */
    uint16_t          canonical_src;       /**< Canonical source unicast address */
    uint16_t          canonical_dst;       /**< Canonical destination unicast/group address */
    uint16_t          orig_src_addr;       /**< Original source address on ingress subnet */
    uint16_t          orig_dst_addr;       /**< Original destination address on ingress subnet */
    uint16_t          ingress_src_addr;    /**< Immediate transmitter address */
    uint16_t          tx_net_idx;          /**< Target egress NetKey Index */
    uint16_t          epoch_id;            /**< Key rotation epoch identifier */
    uint8_t           ttl;                 /**< Time-to-Live */
    meshx_trans_mic_t trans_mic;           /**< Preserved Upper Transport MIC */
    uint8_t           tx_epoch;            /**< Transmit key epoch flag */
    uint8_t           flags;               /**< MESHX_ROUTER_FLAG_* */
    const uint8_t    *pdu;                 /**< Pointer to Upper Transport PDU payload */
    uint8_t           pdu_len;             /**< Length of PDU payload in bytes */
} meshx_router_enqueue_params_t;

/**
 * @brief Initialize router queue state to empty.
 *
 * @param[out] q Pointer to queue descriptor.
 */
void meshx_router_queue_init(meshx_router_queue_t *q);

/**
 * @brief De-initialize router queue, wiping and freeing all pending entries.
 *
 * @param[in,out] q Pointer to queue descriptor.
 */
void meshx_router_queue_deinit(meshx_router_queue_t *q);

/**
 * @brief Allocate single block and enqueue a validated Stage 1 packet.
 *
 * Enforces max PDU bounds (108B or 85B multihop) and pre-ACK Drop-Newest
 * backpressure into the emergency NACK buffer when count == capacity.
 *
 * @param[in,out] q                     Pointer to router queue.
 * @param[in]     params                Validated ingress packet parameters.
 * @param[out]    out_should_nack_stage1 Set to true if drop-newest triggered emergency NACK.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_router_queue_push(meshx_router_queue_t *q,
                                    const meshx_router_enqueue_params_t *params,
                                    bool *out_should_nack_stage1);

/**
 * @brief Peek head flow if eligible for Stage 2 SAR transmission.
 *
 * @param[in] q Pointer to router queue.
 * @return Pointer to head entry, or NULL if empty.
 */
meshx_router_queue_entry_t *meshx_router_queue_peek_tx(meshx_router_queue_t *q);

/**
 * @brief Mark an entry as in-flight to serialize Stage 2 SAR per flow.
 *
 * @param[in,out] q     Pointer to router queue.
 * @param[in,out] entry Queue entry being dispatched.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_router_queue_mark_tx_inflight(meshx_router_queue_t *q, meshx_router_queue_entry_t *entry);

/**
 * @brief Complete transmission, scrub payload memory to zero, and free the queue entry.
 *
 * @param[in,out] q     Pointer to router queue.
 * @param[in]     entry Entry to remove and release.
 * @return MESHX_SUCCESS on success, or MESHX_NOT_FOUND.
 */
meshx_err_t meshx_router_queue_complete_entry(meshx_router_queue_t *q, const meshx_router_queue_entry_t *entry);

/**
 * @brief Purge all queued entries whose ingress or egress matches boundary_id.
 *
 * @param[in,out] q           Pointer to router queue.
 * @param[in]     boundary_id Global Subnet ID to purge.
 * @return Number of entries freed.
 */
uint8_t meshx_router_queue_purge_boundary(meshx_router_queue_t *q, uint32_t boundary_id);

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
                                                    uint8_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* MESHX_ROUTER_QUEUE_H */
