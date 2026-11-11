/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_subnet_kr.h
 * @brief Multi-Subnet Independent Key Rotation (KR) Slot Manager & Burst Quarantine.
 */

#ifndef MESHX_SUBNET_KR_H
#define MESHX_SUBNET_KR_H

#include "island_subnet_coex/meshx_subnet_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    meshx_subnet_crypto_slot_t slots[MESHX_MAX_BRIDGING_SLOTS];
    uint8_t                    slot_valid[MESHX_MAX_BRIDGING_SLOTS];
    bool                       has_cluster_bridge_key;
} meshx_subnet_kr_mgr_t;

void meshx_subnet_kr_init(meshx_subnet_kr_mgr_t *mgr, bool has_k_bridge);

/** Bind/update a local net_idx <-> global_subnet_id crypto slot (up to 4 simultaneous) */
meshx_err_t meshx_subnet_kr_slot_bind(meshx_subnet_kr_mgr_t *mgr,
                                      uint16_t net_idx,
                                      uint32_t global_subnet_id,
                                      const uint8_t active_key[16],
                                      uint32_t key_epoch,
                                      uint32_t iv_index);

/** Release a slot when unbridged/deleted */
meshx_err_t meshx_subnet_kr_slot_unbind(meshx_subnet_kr_mgr_t *mgr, uint16_t net_idx);

/** O(1) slot lookup by local NetKey index (up to 4 slots) */
meshx_subnet_crypto_slot_t *meshx_subnet_kr_find_by_net_idx(meshx_subnet_kr_mgr_t *mgr, uint16_t net_idx);

/** Lookup slot by Global Subnet ID */
meshx_subnet_crypto_slot_t *meshx_subnet_kr_find_by_global_id(meshx_subnet_kr_mgr_t *mgr, uint32_t global_subnet_id);

/** Transition a specific subnet slot through Phases 0 -> 1 -> 2 -> 3 independently */
meshx_err_t meshx_subnet_kr_set_phase(meshx_subnet_kr_mgr_t *mgr,
                                      uint16_t net_idx,
                                      meshx_kr_phase_t new_phase,
                                      const uint8_t new_key[16],
                                      uint32_t new_epoch);

/** Select egress TX key for Seg 0 latch according to TRD Phase rules */
const uint8_t *meshx_subnet_kr_select_egress_key(const meshx_subnet_crypto_slot_t *slot,
                                                 uint8_t *out_tx_epoch);

/** Select candidate ingress keys (tries active first, plus alternate in Phase 1 & 2) */
uint8_t meshx_subnet_kr_get_ingress_keys(const meshx_subnet_crypto_slot_t *slot,
                                         const uint8_t *out_keys[2],
                                         uint8_t out_epochs[2]);

/** Return true if opcode (0xC4, 0xC5, 0xC6) must be blocked from cross-subnet forwarding */
bool meshx_subnet_kr_is_opcode_quarantined(const meshx_subnet_kr_mgr_t *mgr,
                                           uint8_t uvp_opcode,
                                           uint16_t ingress_net_idx,
                                           uint16_t egress_net_idx);

/** Advance sequence counter on slot and signal if pre-wrap IV Update is required (>= 0xF00000) */
meshx_err_t meshx_subnet_kr_alloc_seq(meshx_subnet_kr_mgr_t *mgr,
                                      uint16_t net_idx,
                                      uint32_t *out_seq,
                                      bool *out_iv_update_needed);

#ifdef __cplusplus
}
#endif

#endif /* MESHX_SUBNET_KR_H */

