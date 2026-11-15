/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_subnet_kr.c
 * @brief Multi-Subnet Independent Key Rotation (KR) Slot Manager & Burst Quarantine.
 */

#include <string.h>
#include "island/meshx_subnet_kr.h"

/**
 * @brief Volatile memory wiper to securely scrub cryptographic keys from RAM.
 *
 * @param[out] buf Pointer to buffer to wipe.
 * @param[in]  len Number of bytes to zero out.
 */
static void wipe_bytes(uint8_t *buf, size_t len)
{
    volatile uint8_t *vp = (volatile uint8_t *)buf;
    for (size_t i = 0; i < len; i++) {
        vp[i] = 0U;
    }
}

/**
 * @brief Initialize multi-subnet Key Rotation manager.
 *
 * @param[out] mgr          Pointer to KR manager structure.
 * @param[in]  has_k_bridge Initial Cluster Bridge Key availability flag.
 */
void meshx_subnet_kr_init(meshx_subnet_kr_mgr_t *mgr, bool has_k_bridge)
{
    if (!mgr) {
        return;
    }
    memset(mgr, 0, sizeof(*mgr));
    mgr->has_cluster_bridge_key = has_k_bridge;
}

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
                                      uint32_t iv_index)
{
    if (!mgr || !active_key || net_idx == MESHX_NET_IDX_INVALID) {
        return MESHX_INVALID_ARG;
    }

    int target = -1;
    int first_free = -1;
    for (int i = 0; i < (int)MESHX_MAX_BRIDGING_SLOTS; i++) {
        if (mgr->slot_valid[i] && mgr->slots[i].net_idx == net_idx) {
            target = i;
            break;
        }
        if (!mgr->slot_valid[i] && first_free < 0) {
            first_free = i;
        }
    }

    if (target < 0) {
        if (first_free < 0) {
            return MESHX_NO_MEM;
        }
        target = first_free;
    }

    meshx_subnet_crypto_slot_t *slot = &mgr->slots[target];
    memset(slot, 0, sizeof(*slot));
    slot->net_idx             = net_idx;
    slot->global_subnet_id    = global_subnet_id;
    slot->key_epoch           = key_epoch;
    slot->kr_phase            = (uint8_t)MESHX_SUBNET_KR_PHASE_NORMAL;
    memcpy(slot->old_net_key.bytes, active_key->bytes, MESHX_SUBNET_KEY_LEN);
    slot->active_net_key_flag = 0U;
    slot->iv_index            = iv_index;
    slot->iv_update_flag      = 0U;
    slot->last_seq_num        = 0UL;

    mgr->slot_valid[target] = 1U;
    return MESHX_SUCCESS;
}

/**
 * @brief Release a crypto slot when unbridged or deleted.
 *
 * @param[in,out] mgr     Pointer to KR manager.
 * @param[in]     net_idx Local NetKey Index to unbind.
 * @return MESHX_SUCCESS on success, or MESHX_NOT_FOUND.
 */
meshx_err_t meshx_subnet_kr_slot_unbind(meshx_subnet_kr_mgr_t *mgr, uint16_t net_idx)
{
    if (!mgr) {
        return MESHX_INVALID_ARG;
    }
    for (size_t i = 0; i < MESHX_MAX_BRIDGING_SLOTS; i++) {
        if (mgr->slot_valid[i] && mgr->slots[i].net_idx == net_idx) {
            wipe_bytes(mgr->slots[i].old_net_key.bytes, MESHX_SUBNET_KEY_LEN);
            wipe_bytes(mgr->slots[i].new_net_key.bytes, MESHX_SUBNET_KEY_LEN);
            memset(&mgr->slots[i], 0, sizeof(mgr->slots[i]));
            mgr->slot_valid[i] = 0U;
            return MESHX_SUCCESS;
        }
    }
    return MESHX_NOT_FOUND;
}

/**
 * @brief O(1) slot lookup by local NetKey Index (up to 4 slots).
 *
 * @param[in] mgr     Pointer to KR manager.
 * @param[in] net_idx Local NetKey Index.
 * @return Pointer to matching crypto slot, or NULL if not found.
 */
meshx_subnet_crypto_slot_t *meshx_subnet_kr_find_by_net_idx(meshx_subnet_kr_mgr_t *mgr, uint16_t net_idx)
{
    if (!mgr) {
        return NULL;
    }
    for (size_t i = 0; i < MESHX_MAX_BRIDGING_SLOTS; i++) {
        if (mgr->slot_valid[i] && mgr->slots[i].net_idx == net_idx) {
            return &mgr->slots[i];
        }
    }
    return NULL;
}

/**
 * @brief Lookup crypto slot by Global Subnet ID.
 *
 * @param[in] mgr              Pointer to KR manager.
 * @param[in] global_subnet_id Target Global Subnet ID.
 * @return Pointer to matching crypto slot, or NULL if not found.
 */
meshx_subnet_crypto_slot_t *meshx_subnet_kr_find_by_global_id(meshx_subnet_kr_mgr_t *mgr, uint32_t global_subnet_id)
{
    if (!mgr) {
        return NULL;
    }
    for (size_t i = 0; i < MESHX_MAX_BRIDGING_SLOTS; i++) {
        if (mgr->slot_valid[i] && mgr->slots[i].global_subnet_id == global_subnet_id) {
            return &mgr->slots[i];
        }
    }
    return NULL;
}

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
                                      uint32_t new_epoch)
{
    meshx_subnet_crypto_slot_t *slot = meshx_subnet_kr_find_by_net_idx(mgr, net_idx);
    if (!slot) {
        return MESHX_NOT_FOUND;
    }

    slot->kr_phase  = (uint8_t)new_phase;
    slot->key_epoch = new_epoch;

    switch (new_phase) {
    case MESHX_SUBNET_KR_PHASE_NORMAL:
        slot->active_net_key_flag = 0U;
        break;
    case MESHX_SUBNET_KR_PHASE_DIST:
        if (new_key) {
            memcpy(slot->new_net_key.bytes, new_key->bytes, MESHX_SUBNET_KEY_LEN);
        }
        /* Keep egress latched on old_net_key until SWITCH (Seg 0 stability) */
        slot->active_net_key_flag = 0U;
        break;
    case MESHX_SUBNET_KR_PHASE_SWITCH:
        if (new_key) {
            memcpy(slot->new_net_key.bytes, new_key->bytes, MESHX_SUBNET_KEY_LEN);
        }
        slot->active_net_key_flag = 1U;
        break;
    case MESHX_SUBNET_KR_PHASE_REVOKE:
        wipe_bytes(slot->old_net_key.bytes, MESHX_SUBNET_KEY_LEN);
        memcpy(slot->old_net_key.bytes, slot->new_net_key.bytes, MESHX_SUBNET_KEY_LEN);
        wipe_bytes(slot->new_net_key.bytes, MESHX_SUBNET_KEY_LEN);
        slot->active_net_key_flag = 0U;
        break;
    default:
        return MESHX_INVALID_ARG;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Select egress TX key for Seg 0 latch according to TRD Phase rules.
 *
 * @param[in]  slot         Pointer to crypto slot.
 * @param[out] out_tx_epoch Destination for selected key epoch flag (0 = old, 1 = new).
 * @return Pointer to selected NetKey structure, or NULL if slot is invalid.
 */
const meshx_subnet_key_t *meshx_subnet_kr_select_egress_key(const meshx_subnet_crypto_slot_t *slot,
                                                            uint8_t *out_tx_epoch)
{
    if (!slot) {
        return NULL;
    }
    if (out_tx_epoch) {
        *out_tx_epoch = slot->active_net_key_flag;
    }
    return (slot->active_net_key_flag != 0U) ? &slot->new_net_key : &slot->old_net_key;
}

/**
 * @brief Select candidate ingress keys for dual-RX decryption during key transition.
 *
 * @param[in]  slot             Pointer to crypto slot.
 * @param[out] out_active_keys  Destination candidate key list structure.
 * @return Number of candidate keys (1 or 2), or 0 on error.
 */
uint8_t meshx_subnet_kr_get_ingress_keys(const meshx_subnet_crypto_slot_t *slot,
                                         meshx_kr_active_keys_t *out_active_keys)
{
    if (!slot || !out_active_keys) {
        return 0U;
    }

    if (slot->kr_phase == (uint8_t)MESHX_SUBNET_KR_PHASE_DIST) {
        out_active_keys->candidates[0].key   = &slot->old_net_key;
        out_active_keys->candidates[0].epoch = 0U;
        out_active_keys->candidates[1].key   = &slot->new_net_key;
        out_active_keys->candidates[1].epoch = 1U;
        out_active_keys->count = 2U;
        return 2U;
    }
    if (slot->kr_phase == (uint8_t)MESHX_SUBNET_KR_PHASE_SWITCH) {
        out_active_keys->candidates[0].key   = &slot->new_net_key;
        out_active_keys->candidates[0].epoch = 1U;
        out_active_keys->candidates[1].key   = &slot->old_net_key;
        out_active_keys->candidates[1].epoch = 0U;
        out_active_keys->count = 2U;
        return 2U;
    }

    out_active_keys->candidates[0].key   = &slot->old_net_key;
    out_active_keys->candidates[0].epoch = 0U;
    out_active_keys->count = 1U;
    return 1U;
}

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
                                           uint16_t egress_net_idx)
{
    (void)mgr;
    if (ingress_net_idx == egress_net_idx) {
        return false;
    }
    /* Strict burst quarantine: KR UVP control opcodes (0xC4..0xC6) never cross subnets */
    if (uvp_opcode >= (uint8_t)MESHX_UVP_OP_KR_DISTRIBUTE &&
        uvp_opcode <= (uint8_t)MESHX_UVP_OP_KR_REVOKE_OLD) {
        return true;
    }
    return false;
}

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
                                      bool *out_iv_update_needed)
{
    if (!mgr || !out_seq || !out_iv_update_needed) {
        return MESHX_INVALID_ARG;
    }
    meshx_subnet_crypto_slot_t *slot = meshx_subnet_kr_find_by_net_idx(mgr, net_idx);
    if (!slot) {
        return MESHX_NOT_FOUND;
    }

    slot->last_seq_num++;
    *out_seq = slot->last_seq_num;
    *out_iv_update_needed = (slot->last_seq_num >= MESHX_IV_UPDATE_PREWRAP_SEQ_THRESHOLD);
    return MESHX_SUCCESS;
}
