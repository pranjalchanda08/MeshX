/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_nat_mgr.h
 * @brief Global Subnet ID, Local Slot Registry (N=0..13), Tier-1/Tier-2 NAT & Group Prefixing.
 */

#ifndef MESHX_NAT_MGR_H
#define MESHX_NAT_MGR_H

#include "island_subnet_coex/meshx_subnet_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    meshx_local_slot_registry_t slots;
    meshx_nat_entry_t           tier2[MESHX_NAT_TIER2_CAP];
    uint16_t                    group_bridge_mask[MESHX_MAX_BRIDGING_SLOTS]; /**< Selective per-subnet group mask */
    uint32_t                    local_global_subnet_id;
    uint16_t                    local_node_addr;
    uint32_t                    group_forward_bitmap;
    bool                        initialized;
} meshx_nat_mgr_t;

void meshx_nat_mgr_init(meshx_nat_mgr_t *mgr, uint16_t local_node_addr);
void meshx_nat_mgr_deinit(meshx_nat_mgr_t *mgr);

/** Derive local Global Subnet ID from NetKey fingerprint with bounded remap loop against known peer IDs */
uint32_t meshx_nat_derive_global_subnet_id(const uint8_t net_key[16],
                                           uint16_t cid,
                                           const uint32_t *peer_ids,
                                           size_t peer_count,
                                           uint8_t *out_salt_used);

/** Dynamically allocate or retrieve a Local Slot Index [0..13] via malloc singly-linked list */
meshx_err_t meshx_nat_slot_assign(meshx_nat_mgr_t *mgr,
                                  uint32_t global_subnet_id,
                                  uint16_t local_net_idx,
                                  bool is_local_primary,
                                  uint8_t *out_slot_n);

/** Unassign/free dynamic slot node when ref_count reaches 0 */
meshx_err_t meshx_nat_slot_release(meshx_nat_mgr_t *mgr, uint32_t global_subnet_id);

/** Lookup slot N [0..13] by global_subnet_id or local_net_idx */
int8_t meshx_nat_slot_get_by_global_id(const meshx_nat_mgr_t *mgr, uint32_t global_subnet_id);
int8_t meshx_nat_slot_get_by_net_idx(const meshx_nat_mgr_t *mgr, uint16_t local_net_idx);
uint32_t meshx_nat_get_global_id_by_net_idx(const meshx_nat_mgr_t *mgr, uint16_t local_net_idx);

/** Update deterministic slot coordinator & recompute 32-bit CRC32 Slot Digest */
uint32_t meshx_nat_recompute_slot_digest(meshx_nat_mgr_t *mgr);

/** Translate original node address to Tier-1 Virtual Address (0x4000..0x77FF) for local slot N */
meshx_err_t meshx_nat_translate_tier1(uint8_t slot_n, uint16_t orig_addr, uint16_t *out_virtual);
meshx_err_t meshx_nat_reverse_tier1(uint16_t virtual_addr, uint8_t *out_slot_n, uint16_t *out_orig_addr);

/** Allocate/lookup Tier-2 NAT mapping (0x7800..0x7FFF) for orig_addr >= 0x0400; supports LRU eviction */
meshx_err_t meshx_nat_tier2_alloc(meshx_nat_mgr_t *mgr,
                                    const uint8_t uuid8[8],
                                    uint32_t global_subnet_src,
                                    uint16_t orig_addr,
                                    uint8_t flags,
                                    uint16_t now_min,
                                    uint16_t *out_virtual_addr);

meshx_err_t meshx_nat_tier2_reverse(const meshx_nat_mgr_t *mgr,
                                      uint16_t virtual_addr,
                                      uint32_t *out_global_subnet_src,
                                      uint16_t *out_orig_addr);

/** Evict expired TEMP_DISCOVERY Tier-2 entries (TTL > 5 min) */
uint8_t meshx_nat_tier2_gc(meshx_nat_mgr_t *mgr, uint16_t now_min);

/** Upsert/Delete Tier-2 NAT entry from 0xD5 sync */
meshx_err_t meshx_nat_tier2_apply_sync(meshx_nat_mgr_t *mgr, const meshx_nat_entry_t *entry, uint8_t op);

/** Bidirectional translation helper (Tier-1 if orig < 0x0400, Tier-2 if orig >= 0x0400) */
meshx_err_t meshx_nat_translate_inbound(meshx_nat_mgr_t *mgr,
                                        uint32_t global_subnet_src,
                                        uint16_t orig_addr,
                                        const uint8_t uuid8[8],
                                        uint8_t flags,
                                        uint16_t now_min,
                                        uint16_t *out_virtual_addr);

meshx_err_t meshx_nat_translate_outbound(const meshx_nat_mgr_t *mgr,
                                         uint16_t virtual_addr,
                                         uint32_t *out_global_subnet_id,
                                         uint16_t *out_phys_addr);

/** Group address prefixing (TRD Page 3): 0xC400..0xFBFF */
meshx_err_t meshx_nat_prefix_group_addr(uint8_t slot_n, uint16_t orig_group_addr, uint16_t *out_prefixed_group);
meshx_err_t meshx_nat_unprefix_group_addr(uint16_t prefixed_group, uint8_t *out_slot_n, uint16_t *out_orig_group_addr);

/** Configure selective group forwarding mask */
void meshx_nat_set_group_forward_mask(meshx_nat_mgr_t *mgr, uint16_t group_offset, bool allow);
bool meshx_nat_is_group_forward_allowed(const meshx_nat_mgr_t *mgr, uint16_t group_addr);

#ifdef __cplusplus
}
#endif

#endif /* MESHX_NAT_MGR_H */

