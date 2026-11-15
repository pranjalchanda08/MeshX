/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_nat_mgr.c
 * @brief Global Subnet ID, Local Slot Registry (N=0..13), Tier-1/Tier-2 NAT & Group Prefixing.
 */

#include <string.h>
#include "island/meshx_nat_mgr.h"
#include "interface/rtos/meshx_rtos_utils.h"

/**
 * @brief Search dynamic slot registry for an entry matching the Global Subnet ID.
 *
 * @param[in] mgr              Pointer to NAT manager.
 * @param[in] global_subnet_id Target Global Subnet ID.
 * @return Pointer to matching slot entry, or NULL if not found.
 */
static meshx_local_slot_entry_t *find_slot_by_global_id(const meshx_nat_mgr_t *mgr,
                                                        uint32_t global_subnet_id)
{
    for (meshx_local_slot_entry_t *c = mgr->slots.head; c != NULL; c = c->next)
    {
        if (c->global_subnet_id == global_subnet_id)
        {
            return c;
        }
    }
    return NULL;
}

/**
 * @brief Search dynamic slot registry for an entry matching the slot index N.
 *
 * @param[in] mgr    Pointer to NAT manager.
 * @param[in] slot_n Slot index [0..13].
 * @return Pointer to matching slot entry, or NULL if not found.
 */
static meshx_local_slot_entry_t *find_slot_by_slot_n(const meshx_nat_mgr_t *mgr, uint8_t slot_n)
{
    for (meshx_local_slot_entry_t *c = mgr->slots.head; c != NULL; c = c->next)
    {
        if (c->slot_n == slot_n)
        {
            return c;
        }
    }
    return NULL;
}

/**
 * @brief Check whether a specific slot index N is currently assigned.
 *
 * @param[in] mgr    Pointer to NAT manager.
 * @param[in] slot_n Slot index [0..13].
 * @return true if slot is active, false if free.
 */
static bool is_slot_n_used(const meshx_nat_mgr_t *mgr, uint8_t slot_n)
{
    return (find_slot_by_slot_n(mgr, slot_n) != NULL);
}

/**
 * @brief Initialize NAT manager state and configure local physical node address.
 *
 * @param[out] mgr             Pointer to NAT manager structure.
 * @param[in]  local_node_addr Local node's primary physical unicast address.
 */
void meshx_nat_mgr_init(meshx_nat_mgr_t *mgr, uint16_t local_node_addr)
{
    if (!mgr)
    {
        return;
    }
    memset(mgr, 0, sizeof(*mgr));
    mgr->local_node_addr = local_node_addr;
    mgr->slots.coordinator_slot = MESHX_NAT_SALT_INVALID;
    mgr->slots.coordinator_addr = local_node_addr;
    mgr->initialized = true;
}

/**
 * @brief De-initialize NAT manager and free all dynamically assigned slot nodes.
 *
 * @param[in,out] mgr Pointer to NAT manager structure.
 */
void meshx_nat_mgr_deinit(meshx_nat_mgr_t *mgr)
{
    if (!mgr)
    {
        return;
    }
    meshx_local_slot_entry_t *cur = mgr->slots.head;
    while (cur)
    {
        meshx_local_slot_entry_t *next = cur->next;
        void *p = (void *)cur;
        (void)MESHX_FREE(&p);
        cur = next;
    }
    memset(mgr, 0, sizeof(*mgr));
}

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
                                           uint8_t *out_salt_used)
{
    if (!net_key)
    {
        return MESHX_GLOBAL_SUBNET_INVALID;
    }

    uint8_t seed[MESHX_NAT_DERIVE_SEED_LEN];
    memcpy(seed, net_key->bytes, MESHX_SUBNET_KEY_LEN);
    seed[16] = (uint8_t)(cid & MESHX_BYTE_MASK);
    seed[17] = (uint8_t)((cid >> 8) & MESHX_BYTE_MASK);

    for (uint8_t salt = 0U; salt < MESHX_NAT_MAX_SALT_ATTEMPTS; salt++)
    {
        seed[18] = salt;
        uint32_t id = meshx_subnet_crc32(seed, sizeof(seed));
        if (id == MESHX_GLOBAL_SUBNET_INVALID)
        {
            id = 0x00000001UL;
        }
        bool clash = false;
        if (peer_ids && peer_count > 0U)
        {
            for (size_t i = 0; i < peer_count; i++)
            {
                if (peer_ids[i] == id)
                {
                    clash = true;
                    break;
                }
            }
        }
        if (!clash)
        {
            if (out_salt_used)
            {
                *out_salt_used = salt;
            }
            return id;
        }
    }

    return MESHX_GLOBAL_SUBNET_INVALID;
}

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
                                  uint8_t *out_slot_n)
{
    if (!mgr || global_subnet_id == MESHX_GLOBAL_SUBNET_INVALID)
    {
        return MESHX_INVALID_ARG;
    }

    meshx_local_slot_entry_t *existing = find_slot_by_global_id(mgr, global_subnet_id);
    if (existing)
    {
        existing->ref_count++;
        if (local_net_idx != MESHX_NET_IDX_INVALID)
        {
            existing->local_net_idx = local_net_idx;
        }
        if (is_local_primary)
        {
            mgr->local_global_subnet_id = global_subnet_id;
            existing->flags |= 0x01U;
        }
        if (out_slot_n)
        {
            *out_slot_n = existing->slot_n;
        }
        return MESHX_SUCCESS;
    }

    int chosen = -1;
    if (is_local_primary && !is_slot_n_used(mgr, 0U))
    {
        chosen = 0;
    }
    else
    {
        uint8_t start = is_local_primary ? 0U : 1U;
        for (uint8_t s = start; s < MESHX_SLOT_MAX; s++)
        {
            if (!is_slot_n_used(mgr, s))
            {
                chosen = (int)s;
                break;
            }
        }
        if (chosen < 0 && !is_slot_n_used(mgr, 0U))
        {
            chosen = 0;
        }
    }
    if (chosen < 0)
    {
        return MESHX_NO_MEM;
    }

    meshx_local_slot_entry_t *node = NULL;
    if (MESHX_MALLOC(&node, sizeof(*node)) != MESHX_SUCCESS || !node)
    {
        return MESHX_NO_MEM;
    }

    memset(node, 0, sizeof(*node));
    node->global_subnet_id = global_subnet_id;
    node->local_net_idx = local_net_idx;
    node->slot_n = (uint8_t)chosen;
    node->flags = is_local_primary ? 0x01U : 0x00U;
    node->ref_count = 1U;

    /* Insert sorted by slot_n for canonical digest computation */
    if (!mgr->slots.head || mgr->slots.head->slot_n > node->slot_n)
    {
        node->next = mgr->slots.head;
        mgr->slots.head = node;
    }
    else
    {
        meshx_local_slot_entry_t *cur = mgr->slots.head;
        while (cur->next && cur->next->slot_n < node->slot_n)
        {
            cur = cur->next;
        }
        node->next = cur->next;
        cur->next = node;
    }
    mgr->slots.active_count++;

    if (is_local_primary)
    {
        mgr->local_global_subnet_id = global_subnet_id;
    }
    (void)meshx_nat_recompute_slot_digest(mgr);

    if (out_slot_n)
    {
        *out_slot_n = node->slot_n;
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Unassign/free dynamic slot node when ref_count reaches 0.
 *
 * @param[in,out] mgr              Pointer to NAT manager.
 * @param[in]     global_subnet_id Global Subnet ID to unbind.
 * @return MESHX_SUCCESS on success, or MESHX_NOT_FOUND.
 */
meshx_err_t meshx_nat_slot_release(meshx_nat_mgr_t *mgr, uint32_t global_subnet_id)
{
    if (!mgr)
    {
        return MESHX_INVALID_ARG;
    }
    meshx_local_slot_entry_t *prev = NULL;
    meshx_local_slot_entry_t *cur = mgr->slots.head;
    while (cur && cur->global_subnet_id != global_subnet_id)
    {
        prev = cur;
        cur = cur->next;
    }
    if (!cur)
    {
        return MESHX_NOT_FOUND;
    }

    if (cur->ref_count > 1U)
    {
        cur->ref_count--;
        return MESHX_SUCCESS;
    }

    if (!prev)
    {
        mgr->slots.head = cur->next;
    }
    else
    {
        prev->next = cur->next;
    }
    if (mgr->slots.active_count > 0U)
    {
        mgr->slots.active_count--;
    }

    void *p = (void *)cur;
    (void)MESHX_FREE(&p);
    (void)meshx_nat_recompute_slot_digest(mgr);
    return MESHX_SUCCESS;
}

/**
 * @brief Lookup slot index N [0..13] by Global Subnet ID.
 *
 * @param[in] mgr              Pointer to NAT manager.
 * @param[in] global_subnet_id Subnet identifier.
 * @return Slot index [0..13], or -1 if not found.
 */
int8_t meshx_nat_slot_get_by_global_id(const meshx_nat_mgr_t *mgr, uint32_t global_subnet_id)
{
    if (!mgr)
    {
        return -1;
    }
    meshx_local_slot_entry_t *e = find_slot_by_global_id(mgr, global_subnet_id);
    return e ? (int8_t)e->slot_n : -1;
}

/**
 * @brief Lookup slot index N [0..13] by local NetKey Index.
 *
 * @param[in] mgr           Pointer to NAT manager.
 * @param[in] local_net_idx Local NetKey Index.
 * @return Slot index [0..13], or -1 if not found.
 */
int8_t meshx_nat_slot_get_by_net_idx(const meshx_nat_mgr_t *mgr, uint16_t local_net_idx)
{
    if (!mgr || local_net_idx == MESHX_NET_IDX_INVALID)
    {
        return -1;
    }
    for (meshx_local_slot_entry_t *c = mgr->slots.head; c != NULL; c = c->next)
    {
        if (c->local_net_idx == local_net_idx)
        {
            return (int8_t)c->slot_n;
        }
    }
    return -1;
}

/**
 * @brief Get Global Subnet ID associated with a local NetKey Index.
 *
 * @param[in] mgr           Pointer to NAT manager.
 * @param[in] local_net_idx Local NetKey Index.
 * @return Associated Global Subnet ID, or MESHX_GLOBAL_SUBNET_INVALID if not found.
 */
uint32_t meshx_nat_get_global_id_by_net_idx(const meshx_nat_mgr_t *mgr, uint16_t local_net_idx)
{
    if (!mgr || local_net_idx == MESHX_NET_IDX_INVALID)
    {
        return MESHX_GLOBAL_SUBNET_INVALID;
    }
    for (meshx_local_slot_entry_t *c = mgr->slots.head; c != NULL; c = c->next)
    {
        if (c->local_net_idx == local_net_idx)
        {
            return c->global_subnet_id;
        }
    }
    return MESHX_GLOBAL_SUBNET_INVALID;
}

/**
 * @brief Deterministically update slot coordinator & recompute 32-bit CRC32 Slot Digest.
 *
 * @param[in,out] mgr Pointer to NAT manager.
 * @return Computed 32-bit CRC Slot Digest.
 */
uint32_t meshx_nat_recompute_slot_digest(meshx_nat_mgr_t *mgr)
{
    if (!mgr)
    {
        return 0UL;
    }
    uint8_t buf[MESHX_SLOT_MAX * 6U];
    size_t pos = 0U;
    uint8_t lowest_slot = MESHX_NAT_SALT_INVALID;

    for (meshx_local_slot_entry_t *c = mgr->slots.head; c != NULL; c = c->next)
    {
        if (c->slot_n < lowest_slot)
        {
            lowest_slot = c->slot_n;
        }
        if (pos + 6U <= sizeof(buf))
        {
            buf[pos++] = (uint8_t)(c->global_subnet_id & MESHX_BYTE_MASK);
            buf[pos++] = (uint8_t)((c->global_subnet_id >> 8) & MESHX_BYTE_MASK);
            buf[pos++] = (uint8_t)((c->global_subnet_id >> 16) & MESHX_BYTE_MASK);
            buf[pos++] = (uint8_t)((c->global_subnet_id >> 24) & MESHX_BYTE_MASK);
            buf[pos++] = c->slot_n;
            buf[pos++] = c->flags;
        }
    }

    mgr->slots.coordinator_slot = lowest_slot;
    mgr->slots.table_digest_crc32 = (pos > 0U) ? meshx_subnet_crc32(buf, pos) : 0UL;
    return mgr->slots.table_digest_crc32;
}

/**
 * @brief Translate original node address to Tier-1 Virtual Address (0x4000..0x77FF) for local slot N.
 *
 * @param[in]  slot_n      Local slot index [0..13].
 * @param[in]  orig_addr   Original physical node address (< 0x0400).
 * @param[out] out_virtual Destination for mapped Tier-1 virtual address.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG.
 */
meshx_err_t meshx_nat_translate_tier1(uint8_t slot_n, uint16_t orig_addr, uint16_t *out_virtual)
{
    if (!out_virtual || slot_n >= MESHX_SLOT_MAX || orig_addr == 0U || orig_addr >= MESHX_NAT_ADDR_LIMIT_TIER1)
    {
        return MESHX_INVALID_ARG;
    }
    *out_virtual = (uint16_t)(MESHX_NAT_TIER1_BASE + ((uint16_t)slot_n * MESHX_NAT_TIER1_STRIDE) + orig_addr);
    return MESHX_SUCCESS;
}

/**
 * @brief Reverse Tier-1 Virtual Address back to local slot N and original address.
 *
 * @param[in]  virtual_addr  Virtual address in range 0x4000..0x77FF.
 * @param[out] out_slot_n    Destination for slot index [0..13].
 * @param[out] out_orig_addr Destination for original physical address.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if outside Tier-1 range.
 */
meshx_err_t meshx_nat_reverse_tier1(uint16_t virtual_addr, uint8_t *out_slot_n, uint16_t *out_orig_addr)
{
    if (!out_slot_n || !out_orig_addr ||
        virtual_addr < MESHX_NAT_TIER1_BASE || virtual_addr > MESHX_NAT_TIER1_END)
    {
        return MESHX_INVALID_ARG;
    }
    uint16_t offset = (uint16_t)(virtual_addr - MESHX_NAT_TIER1_BASE);
    uint8_t slot_n = (uint8_t)(offset / MESHX_NAT_TIER1_STRIDE);
    uint16_t orig = (uint16_t)(offset % MESHX_NAT_TIER1_STRIDE);
    if (slot_n >= MESHX_SLOT_MAX || orig == 0U)
    {
        return MESHX_INVALID_ARG;
    }
    *out_slot_n = slot_n;
    *out_orig_addr = orig;
    return MESHX_SUCCESS;
}

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
                                  uint16_t *out_virtual_addr)
{
    if (!mgr || !out_virtual_addr || orig_addr == 0U)
    {
        return MESHX_INVALID_ARG;
    }

    int free_idx = -1;
    int lru_evict_idx = -1;
    uint16_t min_lru_time = 0xFFFFU;

    for (int i = 0; i < (int)MESHX_NAT_TIER2_CAP; i++)
    {
        meshx_nat_entry_t *e = &mgr->tier2[i];
        if ((e->flags & MESHX_NAT_FLAG_VALID) != 0U)
        {
            if (e->global_subnet_src == global_subnet_src && e->orig_addr == orig_addr)
            {
                e->last_active_min = now_min;
                e->flags |= (uint8_t)(flags | MESHX_NAT_FLAG_VALID);
                if (uuid8)
                {
                    memcpy(e->uuid.bytes, uuid8->bytes, MESHX_SUBNET_UUID8_LEN);
                }
                *out_virtual_addr = e->virtual_addr;
                return MESHX_SUCCESS;
            }
            if ((e->flags & MESHX_NAT_FLAG_PERSISTENT) == 0U)
            {
                if (lru_evict_idx < 0 || e->last_active_min < min_lru_time)
                {
                    lru_evict_idx = i;
                    min_lru_time = e->last_active_min;
                }
            }
        }
        else if (free_idx < 0)
        {
            free_idx = i;
        }
    }

    int idx = (free_idx >= 0) ? free_idx : lru_evict_idx;
    if (idx < 0)
    {
        return (meshx_err_t)MESHX_SUBNET_ERR_NAT_FULL;
    }

    meshx_nat_entry_t *dst = &mgr->tier2[idx];
    memset(dst, 0, sizeof(*dst));
    if (uuid8)
    {
        memcpy(dst->uuid.bytes, uuid8->bytes, MESHX_SUBNET_UUID8_LEN);
    }
    dst->global_subnet_src = global_subnet_src;
    dst->orig_addr = orig_addr;
    dst->virtual_addr = (uint16_t)(MESHX_NAT_TIER2_START + (uint16_t)idx);
    dst->flags = (uint8_t)(flags | MESHX_NAT_FLAG_VALID);
    dst->last_active_min = now_min;

    *out_virtual_addr = dst->virtual_addr;
    return MESHX_SUCCESS;
}

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
                                    uint16_t *out_orig_addr)
{
    if (!mgr || !out_global_subnet_src || !out_orig_addr ||
        virtual_addr < MESHX_NAT_TIER2_START || virtual_addr > MESHX_NAT_TIER2_END)
    {
        return MESHX_INVALID_ARG;
    }
    for (size_t i = 0; i < MESHX_NAT_TIER2_CAP; i++)
    {
        const meshx_nat_entry_t *e = &mgr->tier2[i];
        if ((e->flags & MESHX_NAT_FLAG_VALID) != 0U && e->virtual_addr == virtual_addr)
        {
            *out_global_subnet_src = e->global_subnet_src;
            *out_orig_addr = e->orig_addr;
            return MESHX_SUCCESS;
        }
    }
    return MESHX_NOT_FOUND;
}

/**
 * @brief Evict expired TEMP_DISCOVERY Tier-2 entries (TTL > 5 min).
 *
 * @param[in,out] mgr     Pointer to NAT manager.
 * @param[in]     now_min Current monotonic minute timestamp.
 * @return Number of expired entries purged.
 */
uint8_t meshx_nat_tier2_gc(meshx_nat_mgr_t *mgr, uint16_t now_min)
{
    if (!mgr)
    {
        return 0U;
    }
    uint8_t evicted = 0U;
    for (size_t i = 0; i < MESHX_NAT_TIER2_CAP; i++)
    {
        meshx_nat_entry_t *e = &mgr->tier2[i];
        if ((e->flags & MESHX_NAT_FLAG_VALID) != 0U &&
            (e->flags & MESHX_NAT_FLAG_TEMP_DISCOVERY) != 0U &&
            (e->flags & MESHX_NAT_FLAG_PERSISTENT) == 0U)
        {
            if ((uint16_t)(now_min - e->last_active_min) > MESHX_NAT_TEMP_TTL_MIN)
            {
                memset(e, 0, sizeof(*e));
                evicted++;
            }
        }
    }
    return evicted;
}

/**
 * @brief Apply inbound Tier-2 NAT upsert or delete from 0xD5 sync frame.
 *
 * @param[in,out] mgr   Pointer to NAT manager.
 * @param[in]     entry Pointer to NAT entry received in sync frame.
 * @param[in]     op    Sync operation (1 = Upsert, 2 = Delete).
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_nat_tier2_apply_sync(meshx_nat_mgr_t *mgr, const meshx_nat_entry_t *entry, uint8_t op)
{
    if (!mgr || !entry)
    {
        return MESHX_INVALID_ARG;
    }
    int match_idx = -1;
    int free_idx = -1;
    for (int i = 0; i < (int)MESHX_NAT_TIER2_CAP; i++)
    {
        const meshx_nat_entry_t *e = &mgr->tier2[i];
        if ((e->flags & MESHX_NAT_FLAG_VALID) != 0U)
        {
            if (e->global_subnet_src == entry->global_subnet_src &&
                e->orig_addr == entry->orig_addr)
            {
                match_idx = i;
                break;
            }
        }
        else if (free_idx < 0)
        {
            free_idx = i;
        }
    }

    if (op == 2U)
    {
        if (match_idx >= 0)
        {
            memset(&mgr->tier2[match_idx], 0, sizeof(mgr->tier2[match_idx]));
            return MESHX_SUCCESS;
        }
        return MESHX_NOT_FOUND;
    }

    if (op != 1U)
    {
        return MESHX_INVALID_ARG;
    }

    int idx = (match_idx >= 0) ? match_idx : free_idx;
    if (idx < 0)
    {
        return (meshx_err_t)MESHX_SUBNET_ERR_NAT_FULL;
    }

    memcpy(&mgr->tier2[idx], entry, sizeof(*entry));
    mgr->tier2[idx].flags |= MESHX_NAT_FLAG_VALID;
    if (mgr->tier2[idx].virtual_addr < MESHX_NAT_TIER2_START ||
        mgr->tier2[idx].virtual_addr > MESHX_NAT_TIER2_END)
    {
        mgr->tier2[idx].virtual_addr = (uint16_t)(MESHX_NAT_TIER2_START + (uint16_t)idx);
    }
    return MESHX_SUCCESS;
}

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
                                        uint16_t *out_virtual_addr)
{
    if (!mgr || !out_virtual_addr || orig_addr == 0U)
    {
        return MESHX_INVALID_ARG;
    }
    int8_t slot_n = meshx_nat_slot_get_by_global_id(mgr, global_subnet_src);
    if (slot_n >= 0 && orig_addr < MESHX_NAT_ADDR_LIMIT_TIER1)
    {
        return meshx_nat_translate_tier1((uint8_t)slot_n, orig_addr, out_virtual_addr);
    }
    return meshx_nat_tier2_alloc(mgr, uuid8, global_subnet_src, orig_addr, flags, now_min, out_virtual_addr);
}

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
                                         uint16_t *out_phys_addr)
{
    if (!mgr || !out_global_subnet_id || !out_phys_addr)
    {
        return MESHX_INVALID_ARG;
    }

    if (virtual_addr >= MESHX_NAT_TIER1_BASE && virtual_addr <= MESHX_NAT_TIER1_END)
    {
        uint8_t slot_n = 0U;
        uint16_t phys = 0U;
        meshx_err_t err = meshx_nat_reverse_tier1(virtual_addr, &slot_n, &phys);
        if (err != MESHX_SUCCESS)
        {
            return err;
        }
        meshx_local_slot_entry_t *slot = find_slot_by_slot_n(mgr, slot_n);
        if (!slot)
        {
            return MESHX_NOT_FOUND;
        }
        *out_global_subnet_id = slot->global_subnet_id;
        *out_phys_addr = phys;
        return MESHX_SUCCESS;
    }

    if (virtual_addr >= MESHX_NAT_TIER2_START && virtual_addr <= MESHX_NAT_TIER2_END)
    {
        return meshx_nat_tier2_reverse(mgr, virtual_addr, out_global_subnet_id, out_phys_addr);
    }

    return MESHX_NOT_FOUND;
}

/**
 * @brief Prefix original group address with slot N offset into dedicated band (0xC400..0xFBFF).
 *
 * @param[in]  slot_n             Slot index [0..13].
 * @param[in]  orig_group_addr    Original group address (0xC000..0xC3FF or offset <= 0x03FF).
 * @param[out] out_prefixed_group Destination for band-prefixed group address.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG.
 */
meshx_err_t meshx_nat_prefix_group_addr(uint8_t slot_n, uint16_t orig_group_addr, uint16_t *out_prefixed_group)
{
    if (!out_prefixed_group || slot_n >= MESHX_SLOT_MAX || orig_group_addr < 0xC000U)
    {
        return MESHX_INVALID_ARG;
    }
    uint16_t offset = (uint16_t)(orig_group_addr & MESHX_GROUP_OFFSET_MASK);
    *out_prefixed_group = (uint16_t)(0xC000U + ((uint16_t)(slot_n + 1U) << MESHX_NAT_SLOT_SHIFT) + offset);
    return MESHX_SUCCESS;
}

/**
 * @brief Unprefix band-prefixed group address back to origin slot N and base group address.
 *
 * @param[in]  prefixed_group     Group address in range 0xC400..0xFBFF.
 * @param[out] out_slot_n         Destination for origin slot index [0..13].
 * @param[out] out_orig_group_addr Destination for base group address (0xC000 + offset).
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG.
 */
meshx_err_t meshx_nat_unprefix_group_addr(uint16_t prefixed_group, uint8_t *out_slot_n, uint16_t *out_orig_group_addr)
{
    if (!out_slot_n || !out_orig_group_addr ||
        prefixed_group < MESHX_GROUP_BAND_START || prefixed_group > MESHX_GROUP_BAND_END)
    {
        return MESHX_INVALID_ARG;
    }
    uint8_t band = (uint8_t)((prefixed_group - 0xC000U) >> MESHX_NAT_SLOT_SHIFT);
    if (band == 0U || band > MESHX_SLOT_MAX)
    {
        return MESHX_INVALID_ARG;
    }
    *out_slot_n = (uint8_t)(band - 1U);
    *out_orig_group_addr = (uint16_t)(0xC000U + (prefixed_group & MESHX_GROUP_OFFSET_MASK));
    return MESHX_SUCCESS;
}

/**
 * @brief Configure selective group forwarding mask for a 10-bit group offset.
 *
 * @param[in,out] mgr          Pointer to NAT manager.
 * @param[in]     group_offset 10-bit group offset (0..1023).
 * @param[in]     allow        true to permit forwarding, false to block.
 */
void meshx_nat_set_group_forward_mask(meshx_nat_mgr_t *mgr, uint16_t group_offset, bool allow)
{
    if (!mgr)
    {
        return;
    }
    uint32_t bit = 1UL << (group_offset & 31U);
    if (allow)
    {
        mgr->group_forward_bitmap |= bit;
    }
    else
    {
        mgr->group_forward_bitmap &= ~bit;
    }
}

/**
 * @brief Check whether forwarding is allowed for a group address.
 *
 * @param[in] mgr        Pointer to NAT manager.
 * @param[in] group_addr Group address to evaluate.
 * @return true if forwarding is permitted by policy mask, false otherwise.
 */
bool meshx_nat_is_group_forward_allowed(const meshx_nat_mgr_t *mgr, uint16_t group_addr)
{
    if (!mgr || group_addr < 0xC000U)
    {
        return false;
    }
    uint16_t offset = (uint16_t)(group_addr & MESHX_GROUP_OFFSET_MASK);
    uint32_t bit = 1UL << (offset & 31U);
    return (mgr->group_forward_bitmap & bit) != 0UL;
}
