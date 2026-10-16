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
 * @brief Determine anchor unicast address based on pairing roles.
 *
 * @param[in] local_unicast Unicast address of local element.
 * @param[in] peer_unicast  Unicast address of peer element.
 * @param[in] local_role    meshx_element_role_t of local element.
 * @param[in] peer_role     meshx_element_role_t of peer element.
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
