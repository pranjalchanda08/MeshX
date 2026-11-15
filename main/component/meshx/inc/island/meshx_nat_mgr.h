/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_nat_mgr.h
 * @brief Global Subnet ID, Local Slot Registry (N=0..13), Tier-1/Tier-2 NAT & Group Prefixing.
 */

#ifndef MESHX_NAT_MGR_H
#define MESHX_NAT_MGR_H

#include "island/meshx_subnet_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief NAT Manager State Context.
 */
typedef struct {
    meshx_local_slot_registry_t slots;                                        /**< Dynamically allocated local slot nodes */
    meshx_nat_entry_t           tier2[MESHX_NAT_TIER2_CAP];                  /**< Tier-2 dynamic NAT mappings */
    uint16_t                    group_bridge_mask[MESHX_MAX_BRIDGING_SLOTS]; /**< Selective per-subnet group mask */
    uint32_t                    local_global_subnet_id;                      /**< Primary island Global Subnet ID */
    uint16_t                    local_node_addr;                             /**< Local node physical unicast address */
    uint32_t                    group_forward_bitmap;                        /**< Group forwarding permission bitmap */
    bool                        initialized;                                 /**< true if NAT manager is initialized */
} meshx_nat_mgr_t;

/**
 * @brief Initialize NAT manager state and configure local physical node address.
 *
 * @param[out] mgr             Pointer to NAT manager structure.
 * @param[in]  local_node_addr Local node's primary physical unicast address.
 */
void meshx_nat_mgr_init(meshx_nat_mgr_t *mgr, uint16_t local_node_addr);

/**
 * @brief De-initialize NAT manager and free all dynamically assigned slot nodes.
 *
 * @param[in,out] mgr Pointer to NAT manager structure.
 */
void meshx_nat_mgr_deinit(meshx_nat_mgr_t *mgr);

/**
 * @brief Derive deterministic Global Subnet ID from NetKey fingerprint with bounded remap against known peer IDs.
 *
 * @param[in]  net_key       Pointer to 16-byte NetKey structure.
 * @param[in]  cid           16-bit SIG Company Identifier.
 * @param[in]  peer_ids      Array of known peer Global Subnet IDs to detect collision.
 * @param[in]  peer_count    Number of entries in peer_ids.
 * @param[out] out_salt_used Optional destination for salt iteration counter (0 if no collision).
 * @return Derived non-zero 32-bit Global Subnet ID, or MESHX_GLOBAL_SUBNET_INVALID on failure.
 */
uint32_t meshx_nat_derive_global_subnet_id(const meshx_subnet_key_t *net_key,
                                           uint16_t cid,
                                           const uint32_t *peer_ids,
                                           size_t peer_count,
                                           uint8_t *out_salt_used);

/**
 * @brief Dynamically allocate or retrieve a Local Slot Index [0..13] via singly-linked list.
 *
 * @param[in,out] mgr              Pointer to NAT manager.
 * @param[in]     global_subnet_id Subnet ID to bind.
 * @param[in]     local_net_idx    Local BLE Mesh NetKey Index.
 * @param[in]     is_local_primary true if this subnet is the local primary (slot 0).
 * @param[out]    out_slot_n       Destination for assigned slot index [0..13].
 * @return MESHX_SUCCESS on success, MESHX_NO_MEM if all 14 slots are occupied.
 */
meshx_err_t meshx_nat_slot_assign(meshx_nat_mgr_t *mgr,
                                  uint32_t global_subnet_id,
                                  uint16_t local_net_idx,
                                  bool is_local_primary,
                                  uint8_t *out_slot_n);

/**
 * @brief Unassign/free dynamic slot node when ref_count reaches 0.
 *
 * @param[in,out] mgr              Pointer to NAT manager.
 * @param[in]     global_subnet_id Global Subnet ID to unbind.
 * @return MESHX_SUCCESS on success, or MESHX_NOT_FOUND.
 */
meshx_err_t meshx_nat_slot_release(meshx_nat_mgr_t *mgr, uint32_t global_subnet_id);

/**
 * @brief Lookup slot index N [0..13] by Global Subnet ID.
 *
 * @param[in] mgr              Pointer to NAT manager.
 * @param[in] global_subnet_id Subnet identifier.
 * @return Slot index [0..13], or -1 if not found.
 */
int8_t meshx_nat_slot_get_by_global_id(const meshx_nat_mgr_t *mgr, uint32_t global_subnet_id);

/**
 * @brief Lookup slot index N [0..13] by local NetKey Index.
 *
 * @param[in] mgr           Pointer to NAT manager.
 * @param[in] local_net_idx Local NetKey Index.
 * @return Slot index [0..13], or -1 if not found.
 */
int8_t meshx_nat_slot_get_by_net_idx(const meshx_nat_mgr_t *mgr, uint16_t local_net_idx);

/**
 * @brief Get Global Subnet ID associated with a local NetKey Index.
 *
 * @param[in] mgr           Pointer to NAT manager.
 * @param[in] local_net_idx Local NetKey Index.
 * @return Associated Global Subnet ID, or MESHX_GLOBAL_SUBNET_INVALID if not found.
 */
uint32_t meshx_nat_get_global_id_by_net_idx(const meshx_nat_mgr_t *mgr, uint16_t local_net_idx);

/**
 * @brief Deterministically update slot coordinator & recompute 32-bit CRC32 Slot Digest.
 *
 * @param[in,out] mgr Pointer to NAT manager.
 * @return Computed 32-bit CRC Slot Digest.
 */
uint32_t meshx_nat_recompute_slot_digest(meshx_nat_mgr_t *mgr);

/**
 * @brief Translate original node address to Tier-1 Virtual Address (0x4000..0x77FF) for local slot N.
 *
 * @param[in]  slot_n      Local slot index [0..13].
 * @param[in]  orig_addr   Original physical node address (< 0x0400).
 * @param[out] out_virtual Destination for mapped Tier-1 virtual address.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG.
 */
meshx_err_t meshx_nat_translate_tier1(uint8_t slot_n, uint16_t orig_addr, uint16_t *out_virtual);

/**
 * @brief Reverse Tier-1 Virtual Address back to local slot N and original address.
 *
 * @param[in]  virtual_addr  Virtual address in range 0x4000..0x77FF.
 * @param[out] out_slot_n    Destination for slot index [0..13].
 * @param[out] out_orig_addr Destination for original physical address.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if outside Tier-1 range.
 */
meshx_err_t meshx_nat_reverse_tier1(uint16_t virtual_addr, uint8_t *out_slot_n, uint16_t *out_orig_addr);

/**
 * @brief Allocate or lookup Tier-2 NAT mapping (0x7800..0x7FFF) with LRU eviction support.
 *
 * @param[in,out] mgr               Pointer to NAT manager.
 * @param[in]     uuid8             Pointer to 8-byte device UUID prefix.
 * @param[in]     global_subnet_src Origin island Global Subnet ID.
 * @param[in]     orig_addr         True unicast address in origin island (>= 0x0400).
 * @param[in]     flags             MESHX_NAT_FLAG_* attributes.
 * @param[in]     now_min           Current monotonic minute timestamp for LRU tracking.
 * @param[out]    out_virtual_addr  Destination for allocated virtual address (0x7800..0x7FFF).
 * @return MESHX_SUCCESS on success, MESHX_SUBNET_ERR_NAT_FULL if table full with no evictable entries.
 */
meshx_err_t meshx_nat_tier2_alloc(meshx_nat_mgr_t *mgr,
                                  const meshx_subnet_uuid8_t *uuid8,
                                  uint32_t global_subnet_src,
                                  uint16_t orig_addr,
                                  uint8_t flags,
                                  uint16_t now_min,
                                  uint16_t *out_virtual_addr);

/**
 * @brief Reverse Tier-2 Virtual Address back to origin Global Subnet ID and physical address.
 *
 * @param[in]  mgr                   Pointer to NAT manager.
 * @param[in]  virtual_addr          Virtual address in range 0x7800..0x7FFF.
 * @param[out] out_global_subnet_src Destination for origin Global Subnet ID.
 * @param[out] out_orig_addr         Destination for original physical address.
 * @return MESHX_SUCCESS on success, or MESHX_NOT_FOUND.
 */
meshx_err_t meshx_nat_tier2_reverse(const meshx_nat_mgr_t *mgr,
                                    uint16_t virtual_addr,
                                    uint32_t *out_global_subnet_src,
                                    uint16_t *out_orig_addr);

/**
 * @brief Evict expired TEMP_DISCOVERY Tier-2 entries (TTL > 5 min).
 *
 * @param[in,out] mgr     Pointer to NAT manager.
 * @param[in]     now_min Current monotonic minute timestamp.
 * @return Number of expired entries purged.
 */
uint8_t meshx_nat_tier2_gc(meshx_nat_mgr_t *mgr, uint16_t now_min);

/**
 * @brief Apply inbound Tier-2 NAT upsert or delete from 0xD5 sync frame.
 *
 * @param[in,out] mgr   Pointer to NAT manager.
 * @param[in]     entry Pointer to NAT entry received in sync frame.
 * @param[in]     op    Sync operation (1 = Upsert, 2 = Delete).
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_nat_tier2_apply_sync(meshx_nat_mgr_t *mgr, const meshx_nat_entry_t *entry, uint8_t op);

/**
 * @brief Bidirectional translation helper (Tier-1 if orig < 0x0400, Tier-2 if orig >= 0x0400).
 *
 * @param[in,out] mgr               Pointer to NAT manager.
 * @param[in]     global_subnet_src Origin island Global Subnet ID.
 * @param[in]     orig_addr         Original physical unicast address.
 * @param[in]     uuid8             Optional pointer to 8-byte UUID prefix.
 * @param[in]     flags             MESHX_NAT_FLAG_* attributes.
 * @param[in]     now_min           Current monotonic minute timestamp.
 * @param[out]    out_virtual_addr  Destination for translated virtual address.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_nat_translate_inbound(meshx_nat_mgr_t *mgr,
                                        uint32_t global_subnet_src,
                                        uint16_t orig_addr,
                                        const meshx_subnet_uuid8_t *uuid8,
                                        uint8_t flags,
                                        uint16_t now_min,
                                        uint16_t *out_virtual_addr);

/**
 * @brief Translate outbound virtual address back to destination Global Subnet ID and physical address.
 *
 * @param[in]  mgr                 Pointer to NAT manager.
 * @param[in]  virtual_addr         Virtual proxy address.
 * @param[out] out_global_subnet_id Destination for target island ID.
 * @param[out] out_phys_addr        Destination for target physical address.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_nat_translate_outbound(const meshx_nat_mgr_t *mgr,
                                         uint16_t virtual_addr,
                                         uint32_t *out_global_subnet_id,
                                         uint16_t *out_phys_addr);

/**
 * @brief Prefix original group address with slot N offset into dedicated band (0xC400..0xFBFF).
 *
 * @param[in]  slot_n             Slot index [0..13].
 * @param[in]  orig_group_addr    Original group address (0xC000..0xC3FF or offset <= 0x03FF).
 * @param[out] out_prefixed_group Destination for band-prefixed group address.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG.
 */
meshx_err_t meshx_nat_prefix_group_addr(uint8_t slot_n, uint16_t orig_group_addr, uint16_t *out_prefixed_group);

/**
 * @brief Unprefix band-prefixed group address back to origin slot N and base group address.
 *
 * @param[in]  prefixed_group     Group address in range 0xC400..0xFBFF.
 * @param[out] out_slot_n         Destination for origin slot index [0..13].
 * @param[out] out_orig_group_addr Destination for base group address (0xC000 + offset).
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG.
 */
meshx_err_t meshx_nat_unprefix_group_addr(uint16_t prefixed_group, uint8_t *out_slot_n, uint16_t *out_orig_group_addr);

/**
 * @brief Configure selective group forwarding mask for a 10-bit group offset.
 *
 * @param[in,out] mgr          Pointer to NAT manager.
 * @param[in]     group_offset 10-bit group offset (0..1023).
 * @param[in]     allow        true to permit forwarding, false to block.
 */
void meshx_nat_set_group_forward_mask(meshx_nat_mgr_t *mgr, uint16_t group_offset, bool allow);

/**
 * @brief Check whether forwarding is allowed for a group address.
 *
 * @param[in] mgr        Pointer to NAT manager.
 * @param[in] group_addr Group address to evaluate.
 * @return true if forwarding is permitted by policy mask, false otherwise.
 */
bool meshx_nat_is_group_forward_allowed(const meshx_nat_mgr_t *mgr, uint16_t group_addr);

#ifdef __cplusplus
}
#endif

#endif /* MESHX_NAT_MGR_H */
