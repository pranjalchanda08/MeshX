/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_subnet_discovery.h
 * @brief Neighbor Discovery, Boundary Witness Election & Cross-Subnet Pairing Engine.
 */

#ifndef MESHX_SUBNET_DISCOVERY_H
#define MESHX_SUBNET_DISCOVERY_H

#include "island/meshx_subnet_types.h"
#include "island/meshx_nat_mgr.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Keyed discovery correlation entry (4-tuple: global_subnet_src, requestor_addr, trans_id, remote_phys).
 */
typedef struct {
    uint32_t global_subnet_src;     /**< Source global subnet ID */
    uint16_t requestor_addr;        /**< Requesting local element address */
    uint16_t trans_id;              /**< Discovery transaction ID */
    uint16_t remote_phys_addr;      /**< Physical unicast address of discovered node */
    uint16_t assigned_virtual_addr; /**< Assigned Tier-1 / Tier-2 virtual address */
    uint32_t timestamp_ms;          /**< Timestamp in milliseconds when received */
    bool     valid;                 /**< Whether correlation entry is active */
} meshx_disc_correlation_t;

/**
 * @brief Discovery Engine State Context.
 */
typedef struct {
    uint32_t                  mic_fail_timestamps[MESHX_MIC_FAIL_THRESHOLD]; /**< Circular buffer of MIC failure timestamps */
    uint8_t                   mic_fail_head;                                 /**< Insertion head index in circular buffer */
    uint8_t                   mic_fail_count;                                /**< Count of recorded MIC failures in window */
    bool                      witness_eligible;                              /**< Flag indicating witness candidate eligibility */
    meshx_disc_correlation_t  corr[MESHX_DISCOVERY_TX_PENDING_MAX];          /**< Pending transaction correlation table */
    meshx_pair_record_t       pairs[MESHX_PAIR_RECORD_MAX];                  /**< Committed cross-subnet pair records */
    uint8_t                   pair_count;                                    /**< Current active pair count */
} meshx_subnet_discovery_t;

/**
 * @brief Initialize neighbor discovery and boundary witness election context.
 *
 * @param[out] disc Pointer to discovery manager context to initialize.
 */
void meshx_subnet_disc_init(meshx_subnet_discovery_t *disc);

/**
 * @brief Deinitialize neighbor discovery context and clear all state.
 *
 * @param[in,out] disc Pointer to discovery manager context to deinitialize.
 */
void meshx_subnet_disc_deinit(meshx_subnet_discovery_t *disc);

/**
 * @brief Record an RX MIC failure and evaluate the sliding window threshold.
 *
 * Evaluates whether >= MESHX_MIC_FAIL_THRESHOLD MIC failures have occurred
 * within MESHX_MIC_FAIL_WINDOW_MS. If triggered, sets witness eligibility.
 *
 * @param[in,out] disc   Pointer to discovery context.
 * @param[in]     now_ms Current timestamp in milliseconds.
 *
 * @return true if boundary witness election threshold is met, false otherwise.
 */
bool meshx_subnet_disc_record_mic_fail(meshx_subnet_discovery_t *disc, uint32_t now_ms);

/**
 * @brief Reset recorded MIC failure history and witness eligibility.
 *
 * @param[in,out] disc Pointer to discovery context.
 */
void meshx_subnet_disc_reset_mic_fails(meshx_subnet_discovery_t *disc);

/**
 * @brief Evaluate witness candidate criteria for boundary node election.
 *
 * Checks if RSSI >= MESHX_WITNESS_MIN_RSSI_DBM (-82 dBm), mains powered,
 * and free heap exceeds MESHX_WITNESS_MIN_HEAP_BYTES (4 KB).
 *
 * @param[in] disc            Pointer to discovery context (optional).
 * @param[in] rssi_dbm        Signal strength in dBm.
 * @param[in] mains_powered   True if mains powered, false if battery powered.
 * @param[in] free_heap_bytes Free heap memory in bytes.
 *
 * @return true if candidate qualifies as a boundary witness, false otherwise.
 */
bool meshx_subnet_disc_eval_candidate(const meshx_subnet_discovery_t *disc,
                                      int8_t rssi_dbm,
                                      bool mains_powered,
                                      uint32_t free_heap_bytes);

/**
 * @brief Process UVP Discovery Response and allocate ephemeral or mapped virtual NAT address.
 *
 * Correlates response with pending requests, allocates inbound NAT translation,
 * and caches entry in correlation table.
 *
 * @param[in,out] disc                 Pointer to discovery context.
 * @param[in,out] nat_mgr              Pointer to NAT manager.
 * @param[in]     rsp                  Pointer to parsed discovery response packet.
 * @param[in]     now_min              Current time in minutes for NAT aging.
 * @param[in]     now_ms               Current timestamp in milliseconds.
 * @param[out]    out_virtual_nat_addr Pointer to store assigned virtual address.
 *
 * @return MESHX_SUCCESS on success, or appropriate error code.
 */
meshx_err_t meshx_subnet_disc_handle_rsp(meshx_subnet_discovery_t *disc,
                                         meshx_nat_mgr_t *nat_mgr,
                                         const meshx_uvp_discover_rsp_t *rsp,
                                         uint16_t now_min,
                                         uint32_t now_ms,
                                         uint16_t *out_virtual_nat_addr);

/**
 * @brief Commit persistent cross-subnet pair record and promote NAT mapping.
 *
 * Stores pair record in table and allocates Tier-2 persistent NAT entry if required.
 *
 * @param[in,out] disc    Pointer to discovery context.
 * @param[in,out] nat_mgr Pointer to NAT manager (optional, may be NULL).
 * @param[in]     rec     Pointer to pair record to commit.
 *
 * @return MESHX_SUCCESS on success, MESHX_NO_MEM if table full, or MESHX_INVALID_ARG.
 */
meshx_err_t meshx_subnet_disc_commit_pair(meshx_subnet_discovery_t *disc,
                                          meshx_nat_mgr_t *nat_mgr,
                                          const meshx_pair_record_t *rec);

/**
 * @brief Unpair and remove persistent cross-subnet record.
 *
 * @param[in,out] disc                 Pointer to discovery context.
 * @param[in]     local_elem_addr      Local element unicast address.
 * @param[in]     virtual_remote_addr  Virtual remote address being removed.
 *
 * @return MESHX_SUCCESS on success, or MESHX_NOT_FOUND if record was not found.
 */
meshx_err_t meshx_subnet_disc_remove_pair(meshx_subnet_discovery_t *disc,
                                          uint16_t local_elem_addr,
                                          uint16_t virtual_remote_addr);

#ifdef __cplusplus
}
#endif

#endif /* MESHX_SUBNET_DISCOVERY_H */
