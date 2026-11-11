/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_subnet_kr.c
 * @brief Multi-Subnet Independent Key Rotation (KR) Slot Manager & Burst Quarantine.
 */

#include <string.h>
#include "island_subnet_coex/meshx_subnet_kr.h"

static void wipe_bytes(uint8_t *buf, size_t len)
{
    volatile uint8_t *vp = (volatile uint8_t *)buf;
    for (size_t i = 0; i < len; i++) {
        vp[i] = 0U;
    }
}

void meshx_subnet_kr_init(meshx_subnet_kr_mgr_t *mgr, bool has_k_bridge)
{
    if (!mgr) {
        return;
    }
    memset(mgr, 0, sizeof(*mgr));
    mgr->has_cluster_bridge_key = has_k_bridge;
}

meshx_err_t meshx_subnet_kr_slot_bind(meshx_subnet_kr_mgr_t *mgr,
                                      uint16_t net_idx,
                                      uint32_t global_subnet_id,
                                      const uint8_t active_key[16],
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
    memcpy(slot->old_net_key, active_key, 16U);
    slot->active_net_key_flag = 0U;
    slot->iv_index            = iv_index;
    slot->iv_update_flag      = 0U;
    slot->last_seq_num        = 0UL;

    mgr->slot_valid[target] = 1U;
    return MESHX_SUCCESS;
}

meshx_err_t meshx_subnet_kr_slot_unbind(meshx_subnet_kr_mgr_t *mgr, uint16_t net_idx)
{
    if (!mgr) {
        return MESHX_INVALID_ARG;
    }
    for (size_t i = 0; i < MESHX_MAX_BRIDGING_SLOTS; i++) {
        if (mgr->slot_valid[i] && mgr->slots[i].net_idx == net_idx) {
            wipe_bytes(mgr->slots[i].old_net_key, 16U);
            wipe_bytes(mgr->slots[i].new_net_key, 16U);
            memset(&mgr->slots[i], 0, sizeof(mgr->slots[i]));
            mgr->slot_valid[i] = 0U;
            return MESHX_SUCCESS;
        }
    }
    return MESHX_NOT_FOUND;
}

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

meshx_err_t meshx_subnet_kr_set_phase(meshx_subnet_kr_mgr_t *mgr,
                                      uint16_t net_idx,
                                      meshx_kr_phase_t new_phase,
                                      const uint8_t new_key[16],
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
            memcpy(slot->new_net_key, new_key, 16U);
        }
        /* Keep egress latched on old_net_key until SWITCH (Seg 0 stability) */
        slot->active_net_key_flag = 0U;
        break;
    case MESHX_SUBNET_KR_PHASE_SWITCH:
        if (new_key) {
            memcpy(slot->new_net_key, new_key, 16U);
        }
        slot->active_net_key_flag = 1U;
        break;
    case MESHX_SUBNET_KR_PHASE_REVOKE:
        wipe_bytes(slot->old_net_key, 16U);
        memcpy(slot->old_net_key, slot->new_net_key, 16U);
        wipe_bytes(slot->new_net_key, 16U);
        slot->active_net_key_flag = 0U;
        break;
    default:
        return MESHX_INVALID_ARG;
    }

    return MESHX_SUCCESS;
}

const uint8_t *meshx_subnet_kr_select_egress_key(const meshx_subnet_crypto_slot_t *slot,
                                                 uint8_t *out_tx_epoch)
{
    if (!slot) {
        return NULL;
    }
    if (out_tx_epoch) {
        *out_tx_epoch = slot->active_net_key_flag;
    }
    return (slot->active_net_key_flag != 0U) ? slot->new_net_key : slot->old_net_key;
}

uint8_t meshx_subnet_kr_get_ingress_keys(const meshx_subnet_crypto_slot_t *slot,
                                         const uint8_t *out_keys[2],
                                         uint8_t out_epochs[2])
{
    if (!slot || !out_keys || !out_epochs) {
        return 0U;
    }

    if (slot->kr_phase == (uint8_t)MESHX_SUBNET_KR_PHASE_DIST) {
        out_keys[0]   = slot->old_net_key;
        out_epochs[0] = 0U;
        out_keys[1]   = slot->new_net_key;
        out_epochs[1] = 1U;
        return 2U;
    }
    if (slot->kr_phase == (uint8_t)MESHX_SUBNET_KR_PHASE_SWITCH) {
        out_keys[0]   = slot->new_net_key;
        out_epochs[0] = 1U;
        out_keys[1]   = slot->old_net_key;
        out_epochs[1] = 0U;
        return 2U;
    }

    out_keys[0]   = slot->old_net_key;
    out_epochs[0] = 0U;
    return 1U;
}

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
    if (uvp_opcode >= 0xC4U && uvp_opcode <= 0xC6U) {
        return true;
    }
    return false;
}

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
    *out_iv_update_needed = (slot->last_seq_num >= MESHX_SEQ_IV_GUARD_LIMIT);
    return MESHX_SUCCESS;
}

