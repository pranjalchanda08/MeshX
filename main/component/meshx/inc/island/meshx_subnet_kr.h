/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_subnet_kr.h
 * @brief Multi-Subnet Independent Key Rotation (KR) Slot Manager & Burst Quarantine.
 */

#ifndef MESHX_SUBNET_KR_H
#define MESHX_SUBNET_KR_H

#include "island/meshx_subnet_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Multi-Subnet Key Rotation manager state context.
 */
typedef struct {
    meshx_subnet_crypto_slot_t slots[MESHX_MAX_BRIDGING_SLOTS];      /**< Up to 4 active subnet crypto slots */
    uint8_t                    slot_valid[MESHX_MAX_BRIDGING_SLOTS]; /**< Slot validity flags (1 = occupied) */
    bool                       has_cluster_bridge_key;               /**< true if K_bridge is active */
} meshx_subnet_kr_mgr_t;

/**
 * @brief Initialize multi-subnet Key Rotation manager.
 *
 * @param[out] mgr          Pointer to KR manager structure.
 * @param[in]  has_k_bridge Initial Cluster Bridge Key availability flag.
 */
void meshx_subnet_kr_init(meshx_subnet_kr_mgr_t *mgr, bool has_k_bridge);

/**
 * @brief Bind or update a local net_idx <-> global_subnet_id crypto slot (up to 4 simultaneous).
 *
 * @param[in,out] mgr              Pointer to KR manager.
 * @param[in]     net_idx          Local BLE Mesh NetKey Index.
 * @param[in]     global_subnet_id Global Subnet ID.
 * @param[in]     active_key       Pointer to 16-byte initial active NetKey.
 * @param[in]     key_epoch        Key rotation epoch identifier.
 * @param[in]     iv_index         Associated IV Index.
 * @return MESHX_SUCCESS on success, MESHX_NO_MEM if all 4 slots are occupied.
 */
meshx_err_t meshx_subnet_kr_slot_bind(meshx_subnet_kr_mgr_t *mgr,
                                      uint16_t net_idx,
                                      uint32_t global_subnet_id,
                                      const meshx_subnet_key_t *active_key,
                                      uint32_t key_epoch,
                                      uint32_t iv_index);

/**
 * @brief Release a crypto slot when unbridged or deleted.
 *
 * @param[in,out] mgr     Pointer to KR manager.
 * @param[in]     net_idx Local NetKey Index to unbind.
 * @return MESHX_SUCCESS on success, or MESHX_NOT_FOUND.
 */
meshx_err_t meshx_subnet_kr_slot_unbind(meshx_subnet_kr_mgr_t *mgr, uint16_t net_idx);

/**
 * @brief O(1) slot lookup by local NetKey Index (up to 4 slots).
 *
 * @param[in] mgr     Pointer to KR manager.
 * @param[in] net_idx Local NetKey Index.
 * @return Pointer to matching crypto slot, or NULL if not found.
 */
meshx_subnet_crypto_slot_t *meshx_subnet_kr_find_by_net_idx(meshx_subnet_kr_mgr_t *mgr, uint16_t net_idx);

/**
 * @brief Lookup crypto slot by Global Subnet ID.
 *
 * @param[in] mgr              Pointer to KR manager.
 * @param[in] global_subnet_id Target Global Subnet ID.
 * @return Pointer to matching crypto slot, or NULL if not found.
 */
meshx_subnet_crypto_slot_t *meshx_subnet_kr_find_by_global_id(meshx_subnet_kr_mgr_t *mgr, uint32_t global_subnet_id);

/**
 * @brief Transition a specific subnet slot through Phases 0 -> 1 -> 2 -> 3 independently.
 *
 * @param[in,out] mgr       Pointer to KR manager.
 * @param[in]     net_idx   Target local NetKey Index.
 * @param[in]     new_phase Target phase (MESHX_SUBNET_KR_PHASE_*).
 * @param[in]     new_key   Pointer to staged New NetKey (required in Phase 1 & 2, NULL in Phase 3).
 * @param[in]     new_epoch Incremented epoch counter.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_kr_set_phase(meshx_subnet_kr_mgr_t *mgr,
                                      uint16_t net_idx,
                                      meshx_kr_phase_t new_phase,
                                      const meshx_subnet_key_t *new_key,
                                      uint32_t new_epoch);

/**
 * @brief Select egress TX key for Seg 0 latch according to TRD Phase rules.
 *
 * @param[in]  slot         Pointer to crypto slot.
 * @param[out] out_tx_epoch Destination for selected key epoch flag (0 = old, 1 = new).
 * @return Pointer to selected NetKey structure, or NULL if slot is invalid.
 */
const meshx_subnet_key_t *meshx_subnet_kr_select_egress_key(const meshx_subnet_crypto_slot_t *slot,
                                                            uint8_t *out_tx_epoch);

/**
 * @brief Select candidate ingress keys for dual-RX decryption during key transition.
 *
 * @param[in]  slot             Pointer to crypto slot.
 * @param[out] out_active_keys  Destination candidate key list structure.
 * @return Number of candidate keys (1 or 2), or 0 on error.
 */
uint8_t meshx_subnet_kr_get_ingress_keys(const meshx_subnet_crypto_slot_t *slot,
                                         meshx_kr_active_keys_t *out_active_keys);

/**
 * @brief Return true if opcode (0xC4, 0xC5, 0xC6) must be blocked from cross-subnet forwarding.
 *
 * @param[in] mgr             Pointer to KR manager.
 * @param[in] uvp_opcode      UVP message opcode to check.
 * @param[in] ingress_net_idx NetKey Index of packet ingress.
 * @param[in] egress_net_idx  Target NetKey Index of packet egress.
 * @return true if opcode is quarantined, false otherwise.
 */
bool meshx_subnet_kr_is_opcode_quarantined(const meshx_subnet_kr_mgr_t *mgr,
                                           uint8_t uvp_opcode,
                                           uint16_t ingress_net_idx,
                                           uint16_t egress_net_idx);

/**
 * @brief Advance sequence counter on slot and signal if pre-wrap IV Update is required (>= 0xF00000).
 *
 * @param[in,out] mgr                Pointer to KR manager.
 * @param[in]     net_idx            Target local NetKey Index.
 * @param[out]    out_seq            Allocated monotonic sequence number.
 * @param[out]    out_iv_update_needed Set to true if sequence passed pre-wrap guard limit.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_kr_alloc_seq(meshx_subnet_kr_mgr_t *mgr,
                                      uint16_t net_idx,
                                      uint32_t *out_seq,
                                      bool *out_iv_update_needed);

#ifdef __cplusplus
}
#endif

#endif /* MESHX_SUBNET_KR_H */
