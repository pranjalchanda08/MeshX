/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_subnet_discovery.h
 * @brief Neighbor Discovery, Boundary Witness Election & Cross-Subnet Pairing Engine.
 */

#ifndef MESHX_SUBNET_DISCOVERY_H
#define MESHX_SUBNET_DISCOVERY_H

#include "island_subnet_coex/meshx_subnet_types.h"
#include "island_subnet_coex/meshx_nat_mgr.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Keyed discovery correlation entry (4-tuple: global_subnet_src, requestor_addr, trans_id, remote_phys) */
typedef struct {
    uint32_t global_subnet_src;
    uint16_t requestor_addr;
    uint16_t trans_id;
    uint16_t remote_phys_addr;
    uint16_t assigned_virtual_addr;
    uint32_t timestamp_ms;
    bool     valid;
} meshx_disc_correlation_t;

typedef struct {
    uint32_t                  mic_fail_timestamps[MESHX_MIC_FAIL_THRESHOLD];
    uint8_t                   mic_fail_head;
    uint8_t                   mic_fail_count;
    bool                      witness_eligible;
    meshx_disc_correlation_t  corr[MESHX_DISCOVERY_TX_PENDING_MAX];
    meshx_pair_record_t       pairs[MESHX_PAIR_RECORD_MAX];
    uint8_t                   pair_count;
} meshx_subnet_discovery_t;

void meshx_subnet_disc_init(meshx_subnet_discovery_t *disc);
void meshx_subnet_disc_deinit(meshx_subnet_discovery_t *disc);

/** Record RX MIC failure and evaluate 30s sliding window threshold */
bool meshx_subnet_disc_record_mic_fail(meshx_subnet_discovery_t *disc, uint32_t now_ms);
void meshx_subnet_disc_reset_mic_fails(meshx_subnet_discovery_t *disc);

/** Evaluate witness candidate criteria (RSSI >= -82 dBm, mains-powered, heap > 4 KB) */
bool meshx_subnet_disc_eval_candidate(const meshx_subnet_discovery_t *disc,
                                      int8_t rssi_dbm,
                                      bool mains_powered,
                                      uint32_t free_heap_bytes);

/** Relay/process 0xDC Discovery Request and generate 0xDD Discovery Response with NAT entry */
meshx_err_t meshx_subnet_disc_handle_rsp(meshx_subnet_discovery_t *disc,
                                         meshx_nat_mgr_t *nat_mgr,
                                         const meshx_uvp_discover_rsp_t *rsp,
                                         uint16_t now_min,
                                         uint32_t now_ms,
                                         uint16_t *out_virtual_nat_addr);

/** Commit persistent cross-subnet pair record (up to 8 records) and promote TEMP NAT to PERSISTENT */
meshx_err_t meshx_subnet_disc_commit_pair(meshx_subnet_discovery_t *disc,
                                          meshx_nat_mgr_t *nat_mgr,
                                          const meshx_pair_record_t *rec);

/** Unpair and clean up associated TEMP/PERSISTENT record */
meshx_err_t meshx_subnet_disc_remove_pair(meshx_subnet_discovery_t *disc,
                                          uint16_t local_elem_addr,
                                          uint16_t virtual_remote_addr);

#ifdef __cplusplus
}
#endif

#endif /* MESHX_SUBNET_DISCOVERY_H */

