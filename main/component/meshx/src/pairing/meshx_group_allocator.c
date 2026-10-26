/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_group_allocator.c
 * @brief Dynamic Group Address Allocator and Anchor Formula Engine.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 */

#include "meshx_group_allocator.h"
#include "meshx_nvs.h"
#include "meshx_common.h"
#include "interface/rtos/meshx_rtos_utils.h"

#define MESHX_PROVISIONER_DEFAULT_ADDR  0x0001
#define MESHX_BITS_PER_ADDR_STATE       2
#define MESHX_STATES_PER_BYTE           (8 / MESHX_BITS_PER_ADDR_STATE) /* 4 addresses per byte */
#define MESHX_CHUNKS_PER_MASK_WORD      32
#define MESHX_STATE_BITMASK             0x03

/**
 * @brief Calculate group address from anchor node's unicast address.
 *
 * @param[in] anchor_unicast Unicast address of anchor node.
 * @return 16-bit group address in range 0xC000..0xC3FF.
 */
uint16_t meshx_group_derive_from_anchor(uint16_t anchor_unicast)
{
    return (uint16_t)(MESHX_DYNAMIC_GROUP_START + (anchor_unicast & MESHX_DYNAMIC_GROUP_MASK));
}

/**
 * @brief Determine anchor unicast address based on pairing roles and topology archetype.
 *
 * - 1:N (Switch -> Lamps): Producer is the hub => Anchor is Producer (Switch).
 * - N:1 (Switches -> Lamp): Consumer is the hub => Anchor is Consumer (Lamp).
 *
 * @param[in] local_unicast Unicast address of local element.
 * @param[in] peer_unicast  Unicast address of peer element.
 * @param[in] local_role    meshx_element_role_t of local element.
 * @param[in] peer_role     meshx_element_role_t of peer element.
 * @param[in] topology      meshx_topology_type_t archetype.
 * @return Unicast address of resolved anchor node.
 */
uint16_t meshx_group_resolve_anchor_topology(uint16_t local_unicast,
                                             uint16_t peer_unicast,
                                             uint8_t  local_role,
                                             uint8_t  peer_role,
                                             meshx_topology_type_t topology)
{
    /* N:1 Shared Sink (N Switches -> 1 Lamp): Consumer (Lamp) is the Anchor Hub */
    if (topology == MESHX_TOPOLOGY_N_TO_1)
    {
        if (local_role == MESHX_ROLE_CONSUMER)
        {
            return local_unicast;
        }
        if (peer_role == MESHX_ROLE_CONSUMER)
        {
            return peer_unicast;
        }
    }

    /* 1:N Broadcast (1 Switch -> N Lamps) or default: Producer (Switch) is the Anchor Hub */
    if (local_role == MESHX_ROLE_PRODUCER)
    {
        return local_unicast;
    }
    if (peer_role == MESHX_ROLE_PRODUCER)
    {
        return peer_unicast;
    }

    /* Consumer fallback */
    if (local_role == MESHX_ROLE_CONSUMER)
    {
        return local_unicast;
    }
    if (peer_role == MESHX_ROLE_CONSUMER)
    {
        return peer_unicast;
    }

    /* Default fallback: lowest unicast address */
    return (local_unicast < peer_unicast) ? local_unicast : peer_unicast;
}

/**
 * @brief Determine anchor unicast address based on pairing roles.
 *
 * - 1:N (Switch -> Lamps): Producer is the hub => Anchor is Switch.
 * - N:1 (Switches -> Lamp): Consumer is the hub => Anchor is Lamp.
 *
 * @param[in] local_unicast Unicast address of local element.
 * @param[in] peer_unicast  Unicast address of peer element.
 * @param[in] local_role    meshx_element_role_t of local element.
 * @param[in] peer_role     meshx_element_role_t of peer element.
 * @return Unicast address of resolved anchor node.
 */
uint16_t meshx_group_resolve_anchor(uint16_t local_unicast,
                                    uint16_t peer_unicast,
                                    uint8_t  local_role,
                                    uint8_t  peer_role)
{
    return meshx_group_resolve_anchor_topology(local_unicast, peer_unicast, local_role, peer_role, MESHX_TOPOLOGY_UNKNOWN);
}

/**
 * @brief Query total active reference count for a group address from the NVS link table.
 *
 * @param[in] group_addr Group address (0xC000 - 0xC3FF).
 * @return Number of active links bound to this group address, or 0 if none.
 */
uint16_t meshx_group_get_ref_count(uint16_t group_addr)
{
    uint16_t count = 0;
    if (!meshx_is_dynamic_group_addr(group_addr))
    {
        return 0;
    }

    meshx_nvs_link_table_t table;
    if (meshx_nvs_link_table_get(&table) != MESHX_SUCCESS)
    {
        return 0;
    }

    for (uint8_t i = 0; i < MESHX_MAX_PERSISTED_LINKS; ++i)
    {
        if (table.links[i].active && table.links[i].target_group_addr == group_addr)
        {
            count++;
        }
    }
    return count;
}

/**
 * @brief Get the configured provisioner address pool range.
 *
 * @param[in]  role          Provisioner role (Master, Standby, or Standalone).
 * @param[out] pool_start    Pointer to store the start of address pool.
 * @param[out] pool_end      Pointer to store the end of address pool.
 */
void meshx_prov_get_address_pool(meshx_prov_role_t role, uint16_t *pool_start, uint16_t *pool_end)
{
    (void)role;
    if (pool_start == NULL || pool_end == NULL) return;

    /* Single Unified 15-Bit Address Space for all provisioner roles (TRD Page 1) */
    *pool_start = MESHX_UNICAST_ADDR_START;
    *pool_end   = MESHX_UNICAST_ADDR_END;
}

/* =========================================================================
 * Paged Sparse 2-Bit Bitmap Allocator & In-Flight Lease Lifecycle Engine
 * (TRD Page 7 Section 2 & 5.2, TRD Page 8 Section 5)
 * ========================================================================= */

static meshx_sparse_addr_map_t s_sparse_map;
static meshx_lease_table_t     s_lease_table;
static meshx_quarantine_table_t s_quarantine_table;
static bool                    s_sparse_map_initialized = false;
static uint32_t                s_next_lease_id = 1;


/**
 * @brief Initialize the sparse paged unicast address map.
 *
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t meshx_sparse_addr_map_init(void)
{
    if (s_sparse_map_initialized)
    {
        return MESHX_SUCCESS;
    }

    MESHX_LOGI(MODULE_ID_COMMON, "Sparse Map: Initializing 15-bit address bitmap allocator");
    memset(&s_sparse_map, 0, sizeof(s_sparse_map));
    memset(&s_lease_table, 0, sizeof(s_lease_table));
    s_sparse_map.next_free_addr_hint = MESHX_UNICAST_ADDR_START;
    s_sparse_map_initialized = true;

    /* Reserve address 0x0000 (unassigned) and 0x0001 (provisioner local/gateway) */
    meshx_sparse_addr_set_state(MESHX_ADDR_UNASSIGNED, MESHX_ADDR_STATE_ASSIGNED);
    meshx_sparse_addr_set_state(MESHX_PROVISIONER_DEFAULT_ADDR, MESHX_ADDR_STATE_ASSIGNED);

    return MESHX_SUCCESS;
}

/**
 * @brief Map a 16-bit unicast address to its chunk index in the sparse map.
 *
 * @param[in] addr 16-bit mesh unicast address.
 * @return Chunk array index (0 .. MESHX_MAX_CHUNKS - 1).
 */
static inline uint16_t addr_to_chunk_idx(uint16_t addr)
{
    return (addr / MESHX_CHUNK_ADDR_COUNT);
}

/**
 * @brief Map a 16-bit unicast address to its slot index within its parent chunk.
 *
 * @param[in] addr 16-bit mesh unicast address.
 * @return Relative address slot within the chunk (0 .. MESHX_CHUNK_ADDR_COUNT - 1).
 */
static inline uint16_t addr_to_slot_in_chunk(uint16_t addr)
{
    return (addr % MESHX_CHUNK_ADDR_COUNT);
}

/**
 * @brief Get the 2-bit state of a unicast address from the sparse bitmap allocator.
 *
 * @param[in] addr 16-bit mesh unicast address (0x0002..0x7FFF).
 * @return Address state: MESHX_ADDR_STATE_FREE, IN_FLIGHT, ASSIGNED, or QUARANTINED.
 */
uint8_t meshx_sparse_addr_get_state(uint16_t addr)
{
    if (addr > MESHX_UNICAST_ADDR_END)
    {
        return MESHX_ADDR_STATE_ASSIGNED;
    }

    if (!s_sparse_map_initialized)
    {
        meshx_sparse_addr_map_init();
    }

    uint16_t chunk_idx = addr_to_chunk_idx(addr);
    uint32_t mask_word = chunk_idx / MESHX_CHUNKS_PER_MASK_WORD;
    uint32_t mask_bit  = 1U << (chunk_idx % MESHX_CHUNKS_PER_MASK_WORD);

    /* If chunk not allocated, address is STATE_FREE (00b) */
    if ((s_sparse_map.chunk_presence_mask[mask_word] & mask_bit) == 0)
    {
        return MESHX_ADDR_STATE_FREE;
    }

    meshx_addr_chunk_t *chunk = s_sparse_map.chunks[chunk_idx];
    if (!chunk)
    {
        return MESHX_ADDR_STATE_FREE;
    }

    uint16_t slot     = addr_to_slot_in_chunk(addr);
    uint16_t byte_idx = slot / MESHX_STATES_PER_BYTE;
    uint8_t shift     = (slot % MESHX_STATES_PER_BYTE) * MESHX_BITS_PER_ADDR_STATE;
    return (chunk->data[byte_idx] >> shift) & MESHX_STATE_BITMASK;
}

/**
 * @brief Set the 2-bit state of a unicast address in the sparse bitmap allocator.
 *
 * Dynamically allocates backing 32-byte chunks in RAM on-demand when first transitioning
 * away from MESHX_ADDR_STATE_FREE.
 *
 * @param[in] addr  16-bit mesh unicast address (0x0002..0x7FFF).
 * @param[in] state New 2-bit state to set (0x00..0x03).
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG on out of range, or MESHX_NO_MEM.
 */
meshx_err_t meshx_sparse_addr_set_state(uint16_t addr, uint8_t state)
{
    if (addr > MESHX_UNICAST_ADDR_END || state > MESHX_STATE_BITMASK)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Sparse Map: Invalid address 0x%04X or state 0x%02X", addr, state);
        return MESHX_INVALID_ARG;
    }

    if (!s_sparse_map_initialized)
    {
        meshx_sparse_addr_map_init();
    }

    uint16_t chunk_idx = addr_to_chunk_idx(addr);
    uint32_t mask_word = chunk_idx / MESHX_CHUNKS_PER_MASK_WORD;
    uint32_t mask_bit  = 1U << (chunk_idx % MESHX_CHUNKS_PER_MASK_WORD);

    /* Dynamically allocate chunk if not present */
    if ((s_sparse_map.chunk_presence_mask[mask_word] & mask_bit) == 0)
    {
        if (state == MESHX_ADDR_STATE_FREE)
        {
            return MESHX_SUCCESS; /* Already free, no chunk needed */
        }

        meshx_addr_chunk_t *new_chunk = (meshx_addr_chunk_t *)calloc(1, sizeof(meshx_addr_chunk_t));
        if (!new_chunk)
        {
            MESHX_LOGE(MODULE_ID_COMMON, "Sparse Map: Failed to allocate chunk %u for addr 0x%04X", chunk_idx, addr);
            return MESHX_NO_MEM;
        }

        s_sparse_map.chunks[chunk_idx] = new_chunk;
        s_sparse_map.chunk_presence_mask[mask_word] |= mask_bit;
        s_sparse_map.active_chunk_count++;
        MESHX_LOGD(MODULE_ID_COMMON, "Sparse Map: Allocated chunk %u (total active=%u)",
                   chunk_idx, s_sparse_map.active_chunk_count);
    }

    meshx_addr_chunk_t *chunk = s_sparse_map.chunks[chunk_idx];
    uint16_t slot = addr_to_slot_in_chunk(addr);
    uint16_t byte_idx = slot / MESHX_STATES_PER_BYTE;
    uint8_t shift = (slot % MESHX_STATES_PER_BYTE) * MESHX_BITS_PER_ADDR_STATE;

    chunk->data[byte_idx] &= ~(MESHX_STATE_BITMASK << shift);
    chunk->data[byte_idx] |= ((state & MESHX_STATE_BITMASK) << shift);

    s_sparse_map.dirty_node_count++;
    MESHX_LOGD(MODULE_ID_COMMON, "Sparse Map: Addr 0x%04X state -> 0x%02X (dirty=%u)",
               addr, state, s_sparse_map.dirty_node_count);
    return MESHX_SUCCESS;
}

/**
 * @brief Allocate a contiguous block of in-flight addresses under a lease.
 *
 * Performs a monotonic search starting from next_free_addr_hint, falling back
 * to a wrap-around scan if needed, and marks the block as MESHX_ADDR_STATE_IN_FLIGHT.
 *
 * @param[in]  element_count Number of contiguous elements needed.
 * @param[in]  proxy_addr    Frontier proxy unicast address requesting the lease.
 * @param[in]  target_uuid   Pointer to 16-byte UUID of unprovisioned device.
 * @param[out] out_base_addr Pointer to store allocated base unicast address.
 * @param[out] out_lease_id  Pointer to store assigned monotonic lease sequence ID.
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG on invalid arguments, or MESHX_NO_MEM.
 */
meshx_err_t meshx_lease_allocate_block(uint8_t element_count,
                                       uint16_t proxy_addr,
                                       const meshx_uuid_t *target_uuid,
                                       uint16_t *out_base_addr,
                                       uint32_t *out_lease_id)
{
    unsigned int now_ms = 0;
    if (element_count == 0 || !out_base_addr || !out_lease_id || !target_uuid)
    {
        return MESHX_INVALID_ARG;
    }

    if (!s_sparse_map_initialized)
    {
        meshx_sparse_addr_map_init();
    }

    /* Find available lease slot */
    int lease_slot = -1;
    for (int i = 0; i < CONFIG_MAX_CONCURRENT_LEASES; i++)
    {
        if (s_lease_table.entries[i].lease_state == 0x00)
        {
            lease_slot = i;
            break;
        }
    }

    if (lease_slot < 0)
    {
        return MESHX_NO_MEM; /* All concurrent lease slots busy */
    }

    /* Monotonic search starting from hint */
    uint16_t cand = s_sparse_map.next_free_addr_hint;
    if (cand < MESHX_UNICAST_ADDR_START)
    {
        cand = MESHX_UNICAST_ADDR_START;
    }

    uint16_t allocated_base = 0;
    while (cand + element_count - 1 <= MESHX_UNICAST_ADDR_END)
    {
        bool contiguous = true;
        for (uint8_t e = 0; e < element_count; e++)
        {
            if (meshx_sparse_addr_get_state(cand + e) != MESHX_ADDR_STATE_FREE ||
                meshx_quarantine_is_range_blocked(cand, element_count))
            {
                contiguous = false;
                cand += (e + 1); /* Skip past colliding or quarantined address */
                break;
            }
        }

        if (contiguous)
        {
            allocated_base = cand;
            break;
        }
    }

    /* Wrap-around search if monotonic search reached end of address space */
    if (allocated_base == 0)
    {
        bool contiguous = true;
        cand = MESHX_UNICAST_ADDR_START;
        while (cand + element_count - 1 < s_sparse_map.next_free_addr_hint)
        {
            for (uint8_t e = 0; e < element_count; e++)
            {
                if (meshx_sparse_addr_get_state(cand + e) != MESHX_ADDR_STATE_FREE ||
                    meshx_quarantine_is_range_blocked(cand, element_count))
                {
                    contiguous = false;
                    cand += (e + 1);
                    break;
                }
            }

            if (contiguous)
            {
                allocated_base = cand;
                break;
            }
        }
    }

    if (allocated_base == 0)
    {
        return MESHX_NO_MEM; /* Address space exhausted */
    }

    /* Mark allocated contiguous block as STATE_IN_FLIGHT (01b) */
    for (uint8_t e = 0; e < element_count; e++)
    {
        meshx_sparse_addr_set_state(allocated_base + e, MESHX_ADDR_STATE_IN_FLIGHT);
    }

    /* Initialize lease tracker entry */
    uint32_t lease_id = s_next_lease_id++;
    if (s_next_lease_id == 0) s_next_lease_id = 1;

    meshx_lease_entry_t *entry = &s_lease_table.entries[lease_slot];
    memcpy(entry->target_uuid, target_uuid, sizeof(meshx_uuid_t));
    entry->lease_id = lease_id;
    entry->base_addr = allocated_base;
    entry->element_count = element_count;
    entry->lease_state = 0x01; /* ACTIVE_WAITING_REPORT */
    entry->proxy_unicast_addr = proxy_addr;
    meshx_rtos_get_sys_time(&now_ms);
    entry->lease_timestamp = (uint32_t)now_ms;
    s_lease_table.active_count++;

    /* Update watermark hint */
    s_sparse_map.next_free_addr_hint = allocated_base + element_count;

    *out_base_addr = allocated_base;
    *out_lease_id  = lease_id;
    return MESHX_SUCCESS;
}

/**
 * @brief Commit an in-flight lease to permanent ASSIGNED state upon completion.
 *
 * @param[in] lease_id Lease ID returned by meshx_lease_allocate_block.
 * @return MESHX_SUCCESS on success, or MESHX_NOT_FOUND if lease ID not found.
 */
meshx_err_t meshx_lease_commit(uint32_t lease_id)
{
    for (int i = 0; i < CONFIG_MAX_CONCURRENT_LEASES; i++)
    {
        meshx_lease_entry_t *entry = &s_lease_table.entries[i];
        if (entry->lease_state == 0x01 && entry->lease_id == lease_id)
        {
            /* Transition all block elements to permanent STATE_ASSIGNED (10b) */
            for (uint8_t e = 0; e < entry->element_count; e++)
            {
                meshx_sparse_addr_set_state(entry->base_addr + e, MESHX_ADDR_STATE_ASSIGNED);
            }

            entry->lease_state = 0x00; /* IDLE */
            entry->lease_id = 0;
            if (s_lease_table.active_count > 0)
            {
                s_lease_table.active_count--;
            }

            return MESHX_SUCCESS;
        }
    }
    return MESHX_NOT_FOUND;
}

/**
 * @brief Roll back an in-flight lease to FREE state.
 *
 * @param[in] lease_id Lease ID returned by meshx_lease_allocate_block.
 * @return MESHX_SUCCESS on success, or MESHX_NOT_FOUND if lease ID not found.
 */
meshx_err_t meshx_lease_rollback(uint32_t lease_id)
{
    for (int i = 0; i < CONFIG_MAX_CONCURRENT_LEASES; i++)
    {
        meshx_lease_entry_t *entry = &s_lease_table.entries[i];
        if (entry->lease_state == 0x01 && entry->lease_id == lease_id)
        {
            /* Roll back all block elements to STATE_FREE (00b) */
            for (uint8_t e = 0; e < entry->element_count; e++)
            {
                meshx_sparse_addr_set_state(entry->base_addr + e, MESHX_ADDR_STATE_FREE);
            }

            /* Rewind hint if this block was near the watermark */
            if (entry->base_addr < s_sparse_map.next_free_addr_hint)
            {
                s_sparse_map.next_free_addr_hint = entry->base_addr;
            }

            entry->lease_state = 0x00;
            entry->lease_id = 0;
            if (s_lease_table.active_count > 0)
            {
                s_lease_table.active_count--;
            }
            return MESHX_SUCCESS;
        }
    }
    return MESHX_NOT_FOUND;
}

/**
 * @brief Roll back all active in-flight leases across the system.
 *
 * Transitions all in-flight lease entries back to MESHX_ADDR_STATE_FREE and resets
 * the lease tracker table.
 *
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t meshx_lease_rollback_all(void)
{
    uint8_t rolled_back_count = 0;
    for (int i = 0; i < CONFIG_MAX_CONCURRENT_LEASES; i++)
    {
        meshx_lease_entry_t *entry = &s_lease_table.entries[i];
        if (entry->lease_state == 0x01)
        {
            for (uint8_t e = 0; e < entry->element_count; e++)
            {
                meshx_sparse_addr_set_state(entry->base_addr + e, MESHX_ADDR_STATE_FREE);
            }

            if (entry->base_addr < s_sparse_map.next_free_addr_hint)
            {
                s_sparse_map.next_free_addr_hint = entry->base_addr;
            }

            entry->lease_state = 0x00;
            entry->lease_id = 0;
            rolled_back_count++;
        }
    }
    s_lease_table.active_count = 0;
    MESHX_LOGI(MODULE_ID_COMMON, "Group Allocator: Rolled back all in-flight leases (count=%u)", rolled_back_count);
    return MESHX_SUCCESS;
}

/* =========================================================================
 * RPL Quarantine Table Implementation (TASK-REPLACE-001 / TRD Page 9)
 * ========================================================================= */

/**
 * @brief Register a decommissioned or surplus address block in the 24-hour RPL Quarantine Table.
 *
 * @param[in] base_addr     Base unicast address to quarantine.
 * @param[in] element_count Number of contiguous addresses to quarantine.
 * @param[in] timeout_sec   Quarantine hold duration in seconds (typically MESHX_QUARANTINE_TIMEOUT_SEC).
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_quarantine_register(uint16_t base_addr, uint8_t element_count, uint32_t timeout_sec)
{
    if (base_addr < MESHX_UNICAST_ADDR_START || element_count == 0)
    {
        return MESHX_INVALID_ARG;
    }

    unsigned int now_ms = 0;
    meshx_rtos_get_sys_time(&now_ms);
    uint32_t now_sec = now_ms / 1000U;

    /* Mark addresses as quarantined in sparse bitmap */
    for (uint8_t e = 0; e < element_count; e++)
    {
        meshx_sparse_addr_set_state(base_addr + e, MESHX_ADDR_STATE_QUARANTINED);
    }

    /* Find existing entry or free slot */
    int free_slot = -1;
    for (int i = 0; i < CONFIG_MAX_QUARANTINED_BLOCKS; i++)
    {
        meshx_quarantine_entry_t *entry = &s_quarantine_table.entries[i];
        if (entry->active && entry->base_addr == base_addr)
        {
            entry->element_count = element_count;
            entry->release_epoch_timestamp = now_sec + timeout_sec;
            MESHX_LOGI(MODULE_ID_COMMON, "Quarantine: Refreshed block 0x%04X (count=%u, timeout=%lu s)",
                       base_addr, element_count, (unsigned long)timeout_sec);
            return MESHX_SUCCESS;
        }
        if (!entry->active && free_slot < 0)
        {
            free_slot = i;
        }
    }

    if (free_slot < 0)
    {
        MESHX_LOGW(MODULE_ID_COMMON, "Quarantine: Table full, cannot track block 0x%04X", base_addr);
        return MESHX_NO_MEM;
    }

    meshx_quarantine_entry_t *entry = &s_quarantine_table.entries[free_slot];
    entry->base_addr = base_addr;
    entry->element_count = element_count;
    entry->release_epoch_timestamp = now_sec + timeout_sec;
    entry->active = true;
    s_quarantine_table.active_count++;

    MESHX_LOGI(MODULE_ID_COMMON, "Quarantine: Blocked 0x%04X..0x%04X for %lu s (RPL decay hold)",
               base_addr, base_addr + element_count - 1, (unsigned long)timeout_sec);
    return MESHX_SUCCESS;
}

/**
 * @brief Check if a target unicast address range overlaps with any actively quarantined block.
 *
 * @param[in] base_addr     Base unicast address.
 * @param[in] element_count Number of contiguous addresses.
 * @return true if range overlaps an active quarantine, false if clear.
 */
bool meshx_quarantine_is_range_blocked(uint16_t base_addr, uint8_t element_count)
{
    if (s_quarantine_table.active_count == 0)
    {
        return false;
    }

    uint16_t range_end = base_addr + element_count - 1;
    for (int i = 0; i < CONFIG_MAX_QUARANTINED_BLOCKS; i++)
    {
        const meshx_quarantine_entry_t *entry = &s_quarantine_table.entries[i];
        if (entry->active)
        {
            uint16_t q_end = entry->base_addr + entry->element_count - 1;
            /* Check overlap: max(start1, start2) <= min(end1, end2) */
            uint16_t max_start = (base_addr > entry->base_addr) ? base_addr : entry->base_addr;
            uint16_t min_end   = (range_end < q_end) ? range_end : q_end;
            if (max_start <= min_end)
            {
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief Poll and release expired entries in the RPL Quarantine Table.
 */
void meshx_quarantine_poll_expiry(void)
{
    if (s_quarantine_table.active_count == 0)
    {
        return;
    }

    unsigned int now_ms = 0;
    meshx_rtos_get_sys_time(&now_ms);
    uint32_t now_sec = now_ms / 1000U;

    for (int i = 0; i < CONFIG_MAX_QUARANTINED_BLOCKS; i++)
    {
        meshx_quarantine_entry_t *entry = &s_quarantine_table.entries[i];
        if (entry->active && now_sec >= entry->release_epoch_timestamp)
        {
            MESHX_LOGI(MODULE_ID_COMMON, "Quarantine: Expired hold on block 0x%04X (count=%u), returning to FREE pool",
                       entry->base_addr, entry->element_count);
            for (uint8_t e = 0; e < entry->element_count; e++)
            {
                meshx_sparse_addr_set_state(entry->base_addr + e, MESHX_ADDR_STATE_FREE);
            }
            if (entry->base_addr < s_sparse_map.next_free_addr_hint)
            {
                s_sparse_map.next_free_addr_hint = entry->base_addr;
            }
            entry->active = false;
            if (s_quarantine_table.active_count > 0)
            {
                s_quarantine_table.active_count--;
            }
        }
    }
}

/**
 * @brief Process lease timeouts (called periodically every 1000 ms).
 *
 * Automatically rolls back any active lease exceeding MESHX_LEASE_WATCHDOG_TIMEOUT_MS.
 */
void meshx_lease_watchdog_poll(void)
{
    unsigned int now_ms = 0;
    meshx_rtos_get_sys_time(&now_ms);

    /* Poll quarantine table for expired holds */
    meshx_quarantine_poll_expiry();

    for (int i = 0; i < CONFIG_MAX_CONCURRENT_LEASES; i++)
    {
        meshx_lease_entry_t *entry = &s_lease_table.entries[i];
        if (entry->lease_state == 0x01)
        {
            if (now_ms - entry->lease_timestamp >= MESHX_LEASE_WATCHDOG_TIMEOUT_MS)
            {
                /* 30-second watchdog expired without post-close confirmation: roll back lease */
                for (uint8_t e = 0; e < entry->element_count; e++)
                {
                    meshx_sparse_addr_set_state(entry->base_addr + e, MESHX_ADDR_STATE_FREE);
                }

                if (entry->base_addr < s_sparse_map.next_free_addr_hint)
                {
                    s_sparse_map.next_free_addr_hint = entry->base_addr;
                }

                entry->lease_state = 0x00;
                if (s_lease_table.active_count > 0)
                {
                    s_lease_table.active_count--;
                }
            }
        }
    }
}

/**
 * @brief Compute the current local State Freshness Vector (TRD Page 5 Section 2).
 *
 * Calculates the current sync epoch, total count of assigned unicast addresses across
 * all active chunks, next free address watermark hint, and CRC32 across the chunk presence mask.
 *
 * @param[out] out_vector Pointer to meshx_freshness_vector_t structure.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if out_vector is NULL.
 */
meshx_err_t meshx_freshness_vector_compute(meshx_freshness_vector_t *out_vector)
{
    if (!out_vector)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Freshness Vector: NULL output pointer");
        return MESHX_INVALID_ARG;
    }

    if (!s_sparse_map_initialized)
    {
        meshx_sparse_addr_map_init();
    }

    memset(out_vector, 0, sizeof(*out_vector));
    out_vector->sync_epoch = meshx_nvs_get_kr_epoch();
    out_vector->next_free_hint = s_sparse_map.next_free_addr_hint;

    /* Compute assigned address count across all active chunks */
    uint16_t assigned_count = 0;
    for (uint16_t chunk_idx = 0; chunk_idx < MESHX_MAX_CHUNKS; chunk_idx++)
    {
        uint32_t mask_word = chunk_idx / MESHX_CHUNKS_PER_MASK_WORD;
        uint32_t mask_bit  = 1U << (chunk_idx % MESHX_CHUNKS_PER_MASK_WORD);

        if ((s_sparse_map.chunk_presence_mask[mask_word] & mask_bit) != 0 &&
            s_sparse_map.chunks[chunk_idx] != NULL)
        {
            const meshx_addr_chunk_t *chunk = s_sparse_map.chunks[chunk_idx];
            for (uint16_t b = 0; b < MESHX_CHUNK_BYTE_SIZE; b++)
            {
                uint8_t byte_val = chunk->data[b];
                for (uint8_t s = 0; s < MESHX_STATES_PER_BYTE; s++)
                {
                    uint8_t st = (byte_val >> (s * MESHX_BITS_PER_ADDR_STATE)) & MESHX_STATE_BITMASK;
                    if (st == MESHX_ADDR_STATE_ASSIGNED)
                    {
                        assigned_count++;
                    }
                }
            }
        }
    }
    out_vector->assigned_count = assigned_count;

    /* Simple checksum across chunk_presence_mask words */
    uint32_t crc = 0;
    for (int w = 0; w < 8; w++)
    {
        crc ^= s_sparse_map.chunk_presence_mask[w];
    }
    out_vector->chunk_mask_crc32 = crc;

    MESHX_LOGD(MODULE_ID_COMMON, "Freshness Vector computed: epoch=%lu, assigned=%u, hint=0x%04X, crc=0x%08lX",
               (unsigned long)out_vector->sync_epoch, out_vector->assigned_count,
               out_vector->next_free_hint, (unsigned long)out_vector->chunk_mask_crc32);

    return MESHX_SUCCESS;
}

/**
 * @brief Merge an incoming 32-byte chunk bitmap from peer provisioner using additive bitwise OR.
 *
 * Performs non-destructive additive state merge (TRD Page 5 Section 4.2), allocating the chunk in
 * local RAM if not yet resident.
 *
 * @param[in] chunk_idx  Chunk index (0..255).
 * @param[in] chunk_data 32-byte bitmap chunk (2 bits per address).
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG on invalid arguments, or MESHX_NO_MEM.
 */
meshx_err_t meshx_sparse_addr_merge_chunk(uint16_t chunk_idx, const uint8_t chunk_data[MESHX_CHUNK_BYTE_SIZE])
{
    if (chunk_idx >= MESHX_MAX_CHUNKS || !chunk_data)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Chunk Merge: Invalid chunk_idx (%u) or NULL data", chunk_idx);
        return MESHX_INVALID_ARG;
    }

    if (!s_sparse_map_initialized)
    {
        meshx_sparse_addr_map_init();
    }

    uint32_t mask_word = chunk_idx / MESHX_CHUNKS_PER_MASK_WORD;
    uint32_t mask_bit  = 1U << (chunk_idx % MESHX_CHUNKS_PER_MASK_WORD);

    /* Allocate local chunk in RAM if peer has data and we don't have it allocated yet */
    if (s_sparse_map.chunks[chunk_idx] == NULL)
    {
        meshx_addr_chunk_t *new_chunk = (meshx_addr_chunk_t *)malloc(sizeof(meshx_addr_chunk_t));
        if (!new_chunk)
        {
            MESHX_LOGE(MODULE_ID_COMMON, "Chunk Merge: Out of memory allocating chunk %u", chunk_idx);
            return MESHX_NO_MEM;
        }
        memset(new_chunk, 0, sizeof(*new_chunk));
        s_sparse_map.chunks[chunk_idx] = new_chunk;
        s_sparse_map.chunk_presence_mask[mask_word] |= mask_bit;
        s_sparse_map.active_chunk_count++;
    }

    meshx_addr_chunk_t *local_chunk = s_sparse_map.chunks[chunk_idx];

    /* Non-destructive additive bitwise OR merge: ASSIGNED (10b) & QUARANTINED (11b) take precedence */
    for (uint16_t b = 0; b < MESHX_CHUNK_BYTE_SIZE; b++)
    {
        local_chunk->data[b] |= chunk_data[b];
    }

    MESHX_LOGD(MODULE_ID_COMMON, "Chunk Merge: Chunk %u successfully merged via bitwise union", chunk_idx);
    return MESHX_SUCCESS;
}
