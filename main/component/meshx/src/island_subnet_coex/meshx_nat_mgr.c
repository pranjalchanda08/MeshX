/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_nat_mgr.c
 * @brief Global Subnet ID, Local Slot Registry (N=0..13), Tier-1/Tier-2 NAT & Group Prefixing.
 */

#include <string.h>
#include "island_subnet_coex/meshx_nat_mgr.h"
#include "interface/rtos/meshx_rtos_utils.h"

static meshx_local_slot_entry_t *find_slot_by_global_id(const meshx_nat_mgr_t *mgr,
                                                        uint32_t global_subnet_id)
{
    for (meshx_local_slot_entry_t *c = mgr->slots.head; c != NULL; c = c->next) {
        if (c->global_subnet_id == global_subnet_id) {
            return c;
        }
    }
    return NULL;
}

static meshx_local_slot_entry_t *find_slot_by_slot_n(const meshx_nat_mgr_t *mgr, uint8_t slot_n)
{
    for (meshx_local_slot_entry_t *c = mgr->slots.head; c != NULL; c = c->next) {
        if (c->slot_n == slot_n) {
            return c;
        }
    }
    return NULL;
}

static bool is_slot_n_used(const meshx_nat_mgr_t *mgr, uint8_t slot_n)
{
    return (find_slot_by_slot_n(mgr, slot_n) != NULL);
}

void meshx_nat_mgr_init(meshx_nat_mgr_t *mgr, uint16_t local_node_addr)
{
    if (!mgr) {
        return;
    }
    memset(mgr, 0, sizeof(*mgr));
    mgr->local_node_addr        = local_node_addr;
    mgr->slots.coordinator_slot = 0xFFU;
    mgr->slots.coordinator_addr = local_node_addr;
    mgr->initialized            = true;
}

void meshx_nat_mgr_deinit(meshx_nat_mgr_t *mgr)
{
    if (!mgr) {
        return;
    }
    meshx_local_slot_entry_t *cur = mgr->slots.head;
    while (cur) {
        meshx_local_slot_entry_t *next = cur->next;
        void *p = (void *)cur;
        (void)MESHX_FREE(&p);
        cur = next;
    }
    memset(mgr, 0, sizeof(*mgr));
}

uint32_t meshx_nat_derive_global_subnet_id(const uint8_t net_key[16],
                                           uint16_t cid,
                                           const uint32_t *peer_ids,
                                           size_t peer_count,
                                           uint8_t *out_salt_used)
{
    if (!net_key) {
        return MESHX_GLOBAL_SUBNET_INVALID;
    }

    uint8_t seed[19];
    memcpy(seed, net_key, 16U);
    seed[16] = (uint8_t)(cid & 0xFFU);
    seed[17] = (uint8_t)((cid >> 8) & 0xFFU);

    for (uint8_t salt = 0U; salt < 64U; salt++) {
        seed[18] = salt;
        uint32_t id = meshx_subnet_crc32(seed, sizeof(seed));
        if (id == MESHX_GLOBAL_SUBNET_INVALID) {
            id = 0x00000001UL;
        }
        bool clash = false;
        if (peer_ids && peer_count > 0U) {
            for (size_t i = 0; i < peer_count; i++) {
                if (peer_ids[i] == id) {
                    clash = true;
                    break;
                }
            }
        }
        if (!clash) {
            if (out_salt_used) {
                *out_salt_used = salt;
            }
            return id;
        }
    }

    return MESHX_GLOBAL_SUBNET_INVALID;
}

meshx_err_t meshx_nat_slot_assign(meshx_nat_mgr_t *mgr,
                                  uint32_t global_subnet_id,
                                  uint16_t local_net_idx,
                                  bool is_local_primary,
                                  uint8_t *out_slot_n)
{
    if (!mgr || global_subnet_id == MESHX_GLOBAL_SUBNET_INVALID) {
        return MESHX_INVALID_ARG;
    }

    meshx_local_slot_entry_t *existing = find_slot_by_global_id(mgr, global_subnet_id);
    if (existing) {
        existing->ref_count++;
        if (local_net_idx != MESHX_NET_IDX_INVALID) {
            existing->local_net_idx = local_net_idx;
        }
        if (is_local_primary) {
            mgr->local_global_subnet_id = global_subnet_id;
            existing->flags |= 0x01U;
        }
        if (out_slot_n) {
            *out_slot_n = existing->slot_n;
        }
        return MESHX_SUCCESS;
    }

    int chosen = -1;
    if (is_local_primary && !is_slot_n_used(mgr, 0U)) {
        chosen = 0;
    } else {
        uint8_t start = is_local_primary ? 0U : 1U;
        for (uint8_t s = start; s < MESHX_SLOT_MAX; s++) {
            if (!is_slot_n_used(mgr, s)) {
                chosen = (int)s;
                break;
            }
        }
        if (chosen < 0 && !is_slot_n_used(mgr, 0U)) {
            chosen = 0;
        }
    }
    if (chosen < 0) {
        return MESHX_NO_MEM;
    }

    meshx_local_slot_entry_t *node = NULL;
    if (MESHX_MALLOC(&node, sizeof(*node)) != MESHX_SUCCESS || !node) {
        return MESHX_NO_MEM;
    }

    memset(node, 0, sizeof(*node));
    node->global_subnet_id = global_subnet_id;
    node->local_net_idx    = local_net_idx;
    node->slot_n           = (uint8_t)chosen;
    node->flags            = is_local_primary ? 0x01U : 0x00U;
    node->ref_count        = 1U;

    /* Insert sorted by slot_n for canonical digest computation */
    if (!mgr->slots.head || mgr->slots.head->slot_n > node->slot_n) {
        node->next      = mgr->slots.head;
        mgr->slots.head = node;
    } else {
        meshx_local_slot_entry_t *cur = mgr->slots.head;
        while (cur->next && cur->next->slot_n < node->slot_n) {
            cur = cur->next;
        }
        node->next = cur->next;
        cur->next  = node;
    }
    mgr->slots.active_count++;

    if (is_local_primary) {
        mgr->local_global_subnet_id = global_subnet_id;
    }
    (void)meshx_nat_recompute_slot_digest(mgr);

    if (out_slot_n) {
        *out_slot_n = node->slot_n;
    }
    return MESHX_SUCCESS;
}

meshx_err_t meshx_nat_slot_release(meshx_nat_mgr_t *mgr, uint32_t global_subnet_id)
{
    if (!mgr) {
        return MESHX_INVALID_ARG;
    }
    meshx_local_slot_entry_t *prev = NULL;
    meshx_local_slot_entry_t *cur  = mgr->slots.head;
    while (cur && cur->global_subnet_id != global_subnet_id) {
        prev = cur;
        cur  = cur->next;
    }
    if (!cur) {
        return MESHX_NOT_FOUND;
    }

    if (cur->ref_count > 1U) {
        cur->ref_count--;
        return MESHX_SUCCESS;
    }

    if (!prev) {
        mgr->slots.head = cur->next;
    } else {
        prev->next = cur->next;
    }
    if (mgr->slots.active_count > 0U) {
        mgr->slots.active_count--;
    }

    void *p = (void *)cur;
    (void)MESHX_FREE(&p);
    (void)meshx_nat_recompute_slot_digest(mgr);
    return MESHX_SUCCESS;
}

int8_t meshx_nat_slot_get_by_global_id(const meshx_nat_mgr_t *mgr, uint32_t global_subnet_id)
{
    if (!mgr) {
        return -1;
    }
    meshx_local_slot_entry_t *e = find_slot_by_global_id(mgr, global_subnet_id);
    return e ? (int8_t)e->slot_n : -1;
}

int8_t meshx_nat_slot_get_by_net_idx(const meshx_nat_mgr_t *mgr, uint16_t local_net_idx)
{
    if (!mgr || local_net_idx == MESHX_NET_IDX_INVALID) {
        return -1;
    }
    for (meshx_local_slot_entry_t *c = mgr->slots.head; c != NULL; c = c->next) {
        if (c->local_net_idx == local_net_idx) {
            return (int8_t)c->slot_n;
        }
    }
    return -1;
}

uint32_t meshx_nat_get_global_id_by_net_idx(const meshx_nat_mgr_t *mgr, uint16_t local_net_idx)
{
    if (!mgr || local_net_idx == MESHX_NET_IDX_INVALID) {
        return MESHX_GLOBAL_SUBNET_INVALID;
    }
    for (meshx_local_slot_entry_t *c = mgr->slots.head; c != NULL; c = c->next) {
        if (c->local_net_idx == local_net_idx) {
            return c->global_subnet_id;
        }
    }
    return MESHX_GLOBAL_SUBNET_INVALID;
}

uint32_t meshx_nat_recompute_slot_digest(meshx_nat_mgr_t *mgr)
{
    if (!mgr) {
        return 0UL;
    }
    uint8_t buf[MESHX_SLOT_MAX * 6U];
    size_t pos = 0U;
    uint8_t lowest_slot = 0xFFU;

    for (meshx_local_slot_entry_t *c = mgr->slots.head; c != NULL; c = c->next) {
        if (c->slot_n < lowest_slot) {
            lowest_slot = c->slot_n;
        }
        if (pos + 6U <= sizeof(buf)) {
            buf[pos++] = (uint8_t)(c->global_subnet_id & 0xFFU);
            buf[pos++] = (uint8_t)((c->global_subnet_id >> 8) & 0xFFU);
            buf[pos++] = (uint8_t)((c->global_subnet_id >> 16) & 0xFFU);
            buf[pos++] = (uint8_t)((c->global_subnet_id >> 24) & 0xFFU);
            buf[pos++] = c->slot_n;
            buf[pos++] = c->flags;
        }
    }

    mgr->slots.coordinator_slot   = lowest_slot;
    mgr->slots.table_digest_crc32 = (pos > 0U) ? meshx_subnet_crc32(buf, pos) : 0UL;
    return mgr->slots.table_digest_crc32;
}

meshx_err_t meshx_nat_translate_tier1(uint8_t slot_n, uint16_t orig_addr, uint16_t *out_virtual)
{
    if (!out_virtual || slot_n >= MESHX_SLOT_MAX || orig_addr == 0U || orig_addr >= 0x0400U) {
        return MESHX_INVALID_ARG;
    }
    *out_virtual = (uint16_t)(MESHX_NAT_TIER1_BASE + ((uint16_t)slot_n * MESHX_NAT_TIER1_STRIDE) + orig_addr);
    return MESHX_SUCCESS;
}

meshx_err_t meshx_nat_reverse_tier1(uint16_t virtual_addr, uint8_t *out_slot_n, uint16_t *out_orig_addr)
{
    if (!out_slot_n || !out_orig_addr ||
        virtual_addr < MESHX_NAT_TIER1_BASE || virtual_addr > MESHX_NAT_TIER1_END) {
        return MESHX_INVALID_ARG;
    }
    uint16_t offset = (uint16_t)(virtual_addr - MESHX_NAT_TIER1_BASE);
    uint8_t  slot_n = (uint8_t)(offset / MESHX_NAT_TIER1_STRIDE);
    uint16_t orig   = (uint16_t)(offset % MESHX_NAT_TIER1_STRIDE);
    if (slot_n >= MESHX_SLOT_MAX || orig == 0U) {
        return MESHX_INVALID_ARG;
    }
    *out_slot_n    = slot_n;
    *out_orig_addr = orig;
    return MESHX_SUCCESS;
}

meshx_err_t meshx_nat_tier2_alloc(meshx_nat_mgr_t *mgr,
                                    const uint8_t uuid8[8],
                                    uint32_t global_subnet_src,
                                    uint16_t orig_addr,
                                    uint8_t flags,
                                    uint16_t now_min,
                                    uint16_t *out_virtual_addr)
{
    if (!mgr || !out_virtual_addr || orig_addr == 0U) {
        return MESHX_INVALID_ARG;
    }

    int free_idx = -1;
    int lru_evict_idx = -1;
    uint16_t min_lru_time = 0xFFFFU;

    for (int i = 0; i < (int)MESHX_NAT_TIER2_CAP; i++) {
        meshx_nat_entry_t *e = &mgr->tier2[i];
        if ((e->flags & MESHX_NAT_FLAG_VALID) != 0U) {
            if (e->global_subnet_src == global_subnet_src && e->orig_addr == orig_addr) {
                e->last_active_min = now_min;
                e->flags |= (uint8_t)(flags | MESHX_NAT_FLAG_VALID);
                if (uuid8) {
                    memcpy(e->uuid, uuid8, 8U);
                }
                *out_virtual_addr = e->virtual_addr;
                return MESHX_SUCCESS;
            }
            if ((e->flags & MESHX_NAT_FLAG_PERSISTENT) == 0U) {
                if (lru_evict_idx < 0 || e->last_active_min < min_lru_time) {
                    lru_evict_idx = i;
                    min_lru_time  = e->last_active_min;
                }
            }
        } else if (free_idx < 0) {
            free_idx = i;
        }
    }

    int idx = (free_idx >= 0) ? free_idx : lru_evict_idx;
    if (idx < 0) {
        return (meshx_err_t)MESHX_SUBNET_ERR_NAT_FULL;
    }

    meshx_nat_entry_t *dst = &mgr->tier2[idx];
    memset(dst, 0, sizeof(*dst));
    if (uuid8) {
        memcpy(dst->uuid, uuid8, 8U);
    }
    dst->global_subnet_src = global_subnet_src;
    dst->orig_addr         = orig_addr;
    dst->virtual_addr      = (uint16_t)(MESHX_NAT_TIER2_START + (uint16_t)idx);
    dst->flags             = (uint8_t)(flags | MESHX_NAT_FLAG_VALID);
    dst->last_active_min   = now_min;

    *out_virtual_addr = dst->virtual_addr;
    return MESHX_SUCCESS;
}

meshx_err_t meshx_nat_tier2_reverse(const meshx_nat_mgr_t *mgr,
                                      uint16_t virtual_addr,
                                      uint32_t *out_global_subnet_src,
                                      uint16_t *out_orig_addr)
{
    if (!mgr || !out_global_subnet_src || !out_orig_addr ||
        virtual_addr < MESHX_NAT_TIER2_START || virtual_addr > MESHX_NAT_TIER2_END) {
        return MESHX_INVALID_ARG;
    }
    for (size_t i = 0; i < MESHX_NAT_TIER2_CAP; i++) {
        const meshx_nat_entry_t *e = &mgr->tier2[i];
        if ((e->flags & MESHX_NAT_FLAG_VALID) != 0U && e->virtual_addr == virtual_addr) {
            *out_global_subnet_src = e->global_subnet_src;
            *out_orig_addr         = e->orig_addr;
            return MESHX_SUCCESS;
        }
    }
    return MESHX_NOT_FOUND;
}

uint8_t meshx_nat_tier2_gc(meshx_nat_mgr_t *mgr, uint16_t now_min)
{
    if (!mgr) {
        return 0U;
    }
    uint8_t evicted = 0U;
    for (size_t i = 0; i < MESHX_NAT_TIER2_CAP; i++) {
        meshx_nat_entry_t *e = &mgr->tier2[i];
        if ((e->flags & MESHX_NAT_FLAG_VALID) != 0U &&
            (e->flags & MESHX_NAT_FLAG_TEMP_DISCOVERY) != 0U &&
            (e->flags & MESHX_NAT_FLAG_PERSISTENT) == 0U) {
            if ((uint16_t)(now_min - e->last_active_min) > MESHX_NAT_TEMP_TTL_MIN) {
                memset(e, 0, sizeof(*e));
                evicted++;
            }
        }
    }
    return evicted;
}

meshx_err_t meshx_nat_tier2_apply_sync(meshx_nat_mgr_t *mgr, const meshx_nat_entry_t *entry, uint8_t op)
{
    if (!mgr || !entry) {
        return MESHX_INVALID_ARG;
    }
    int match_idx = -1;
    int free_idx  = -1;
    for (int i = 0; i < (int)MESHX_NAT_TIER2_CAP; i++) {
        const meshx_nat_entry_t *e = &mgr->tier2[i];
        if ((e->flags & MESHX_NAT_FLAG_VALID) != 0U) {
            if (e->global_subnet_src == entry->global_subnet_src &&
                e->orig_addr == entry->orig_addr) {
                match_idx = i;
                break;
            }
        } else if (free_idx < 0) {
            free_idx = i;
        }
    }

    if (op == 2U) {
        if (match_idx >= 0) {
            memset(&mgr->tier2[match_idx], 0, sizeof(mgr->tier2[match_idx]));
            return MESHX_SUCCESS;
        }
        return MESHX_NOT_FOUND;
    }

    if (op != 1U) {
        return MESHX_INVALID_ARG;
    }

    int idx = (match_idx >= 0) ? match_idx : free_idx;
    if (idx < 0) {
        return (meshx_err_t)MESHX_SUBNET_ERR_NAT_FULL;
    }

    memcpy(&mgr->tier2[idx], entry, sizeof(*entry));
    mgr->tier2[idx].flags |= MESHX_NAT_FLAG_VALID;
    if (mgr->tier2[idx].virtual_addr < MESHX_NAT_TIER2_START ||
        mgr->tier2[idx].virtual_addr > MESHX_NAT_TIER2_END) {
        mgr->tier2[idx].virtual_addr = (uint16_t)(MESHX_NAT_TIER2_START + (uint16_t)idx);
    }
    return MESHX_SUCCESS;
}

meshx_err_t meshx_nat_translate_inbound(meshx_nat_mgr_t *mgr,
                                        uint32_t global_subnet_src,
                                        uint16_t orig_addr,
                                        const uint8_t uuid8[8],
                                        uint8_t flags,
                                        uint16_t now_min,
                                        uint16_t *out_virtual_addr)
{
    if (!mgr || !out_virtual_addr || orig_addr == 0U) {
        return MESHX_INVALID_ARG;
    }
    int8_t slot_n = meshx_nat_slot_get_by_global_id(mgr, global_subnet_src);
    if (slot_n >= 0 && orig_addr < 0x0400U) {
        return meshx_nat_translate_tier1((uint8_t)slot_n, orig_addr, out_virtual_addr);
    }
    return meshx_nat_tier2_alloc(mgr, uuid8, global_subnet_src, orig_addr, flags, now_min, out_virtual_addr);
}

meshx_err_t meshx_nat_translate_outbound(const meshx_nat_mgr_t *mgr,
                                         uint16_t virtual_addr,
                                         uint32_t *out_global_subnet_id,
                                         uint16_t *out_phys_addr)
{
    if (!mgr || !out_global_subnet_id || !out_phys_addr) {
        return MESHX_INVALID_ARG;
    }

    if (virtual_addr >= MESHX_NAT_TIER1_BASE && virtual_addr <= MESHX_NAT_TIER1_END) {
        uint8_t slot_n = 0U;
        uint16_t phys = 0U;
        meshx_err_t err = meshx_nat_reverse_tier1(virtual_addr, &slot_n, &phys);
        if (err != MESHX_SUCCESS) {
            return err;
        }
        meshx_local_slot_entry_t *slot = find_slot_by_slot_n(mgr, slot_n);
        if (!slot) {
            return MESHX_NOT_FOUND;
        }
        *out_global_subnet_id = slot->global_subnet_id;
        *out_phys_addr        = phys;
        return MESHX_SUCCESS;
    }

    if (virtual_addr >= MESHX_NAT_TIER2_START && virtual_addr <= MESHX_NAT_TIER2_END) {
        return meshx_nat_tier2_reverse(mgr, virtual_addr, out_global_subnet_id, out_phys_addr);
    }

    return MESHX_NOT_FOUND;
}

meshx_err_t meshx_nat_prefix_group_addr(uint8_t slot_n, uint16_t orig_group_addr, uint16_t *out_prefixed_group)
{
    if (!out_prefixed_group || slot_n >= MESHX_SLOT_MAX || orig_group_addr < 0xC000U) {
        return MESHX_INVALID_ARG;
    }
    uint16_t offset = (uint16_t)(orig_group_addr & MESHX_GROUP_OFFSET_MASK);
    *out_prefixed_group = (uint16_t)(0xC000U + ((uint16_t)(slot_n + 1U) << 10) + offset);
    return MESHX_SUCCESS;
}

meshx_err_t meshx_nat_unprefix_group_addr(uint16_t prefixed_group, uint8_t *out_slot_n, uint16_t *out_orig_group_addr)
{
    if (!out_slot_n || !out_orig_group_addr ||
        prefixed_group < MESHX_GROUP_BAND_START || prefixed_group > MESHX_GROUP_BAND_END) {
        return MESHX_INVALID_ARG;
    }
    uint8_t band = (uint8_t)((prefixed_group - 0xC000U) >> 10);
    if (band == 0U || band > MESHX_SLOT_MAX) {
        return MESHX_INVALID_ARG;
    }
    *out_slot_n          = (uint8_t)(band - 1U);
    *out_orig_group_addr = (uint16_t)(0xC000U + (prefixed_group & MESHX_GROUP_OFFSET_MASK));
    return MESHX_SUCCESS;
}

void meshx_nat_set_group_forward_mask(meshx_nat_mgr_t *mgr, uint16_t group_offset, bool allow)
{
    if (!mgr) {
        return;
    }
    uint32_t bit = 1UL << (group_offset & 31U);
    if (allow) {
        mgr->group_forward_bitmap |= bit;
    } else {
        mgr->group_forward_bitmap &= ~bit;
    }
}

bool meshx_nat_is_group_forward_allowed(const meshx_nat_mgr_t *mgr, uint16_t group_addr)
{
    if (!mgr || group_addr < 0xC000U) {
        return false;
    }
    uint16_t offset = (uint16_t)(group_addr & MESHX_GROUP_OFFSET_MASK);
    uint32_t bit = 1UL << (offset & 31U);
    return (mgr->group_forward_bitmap & bit) != 0UL;
}

