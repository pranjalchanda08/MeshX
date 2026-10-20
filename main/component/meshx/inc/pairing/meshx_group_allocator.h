/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_group_allocator.h
 * @brief Dynamic Group Address Allocator and Anchor Formula Engine.
 *
 * Provides pure mathematical Anchor Group Address derivation (0xC000 - 0xC3FF)
 * and group reference counting directly backed by the persistent NVS Link Table.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 */

#ifndef __MESHX_GROUP_ALLOCATOR_H__
#define __MESHX_GROUP_ALLOCATOR_H__

#include <stdint.h>
#include <stdbool.h>
#include "meshx_err.h"
#include "meshx_manifest.h"

#include "meshx_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MESHX_DYNAMIC_GROUP_START       MESHX_ADDR_GROUP_START
#define MESHX_DYNAMIC_GROUP_END         MESHX_ADDR_GROUP_END
#define MESHX_DYNAMIC_GROUP_MASK        MESHX_ADDR_GROUP_MASK

/* Partitioned Provisioner Unicast Address Pools (TASK-PROV-005) */
#define MESHX_PROV_POOL_A_START         0x0002
#define MESHX_PROV_POOL_A_END           0x3FFF
#define MESHX_PROV_POOL_B_START         0x4000
#define MESHX_PROV_POOL_B_END           0x7FFF

typedef enum {
    MESHX_PROV_ROLE_STANDALONE = 0,     /**< Provisioner is standalone */
    MESHX_PROV_ROLE_MASTER     = 1,     /**< Provisioner is master */
    MESHX_PROV_ROLE_STANDBY    = 2,     /**< Provisioner is standby */
} meshx_prov_role_t;

/**
 * @brief Topology type determined by pairing roles and current link counts.
 */
typedef enum {
    MESHX_TOPOLOGY_UNKNOWN  = 0,
    MESHX_TOPOLOGY_1_TO_1   = 1,   /**< 1 Switch -> 1 Lamp (Direct Unicast, 0 group leases) */
    MESHX_TOPOLOGY_1_TO_N   = 2,   /**< 1 Switch -> N Lamps (Anchor = Switch) */
    MESHX_TOPOLOGY_N_TO_1   = 3,   /**< N Switches -> 1 Lamp (Anchor = Lamp) */
    MESHX_TOPOLOGY_M_TO_N   = 4,   /**< M Switches -> N Lamps (Multi-controller zone) */
} meshx_topology_type_t;

/**
 * @brief Check if address is in dynamic group range (0xC000 - 0xC3FF).
 *
 * @param[in] addr Mesh address to check.
 * @return true if dynamic group address, false otherwise.
 */
static inline bool meshx_is_dynamic_group_addr(uint16_t addr) {
    return (addr >= MESHX_DYNAMIC_GROUP_START && addr <= MESHX_DYNAMIC_GROUP_END);
}

/**
 * @brief Calculate group address from anchor node's unicast address.
 *
 * Formula: Group Addr = 0xC000 + (anchor_unicast & 0x03FF)
 *
 * @param[in] anchor_unicast Unicast address of anchor node.
 * @return 16-bit group address in range 0xC000..0xC3FF.
 */
uint16_t meshx_group_derive_from_anchor(uint16_t anchor_unicast);

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
                                    uint8_t local_role,
                                    uint8_t peer_role);

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
                                             meshx_topology_type_t topology);

/**
 * @brief Query total active reference count for a group address from the NVS link table.
 *
 * @param[in] group_addr Group address (0xC000 - 0xC3FF).
 * @return Number of active links bound to this group address, or 0 if none.
 */
uint16_t meshx_group_get_ref_count(uint16_t group_addr);

/**
 * @brief Get the configured provisioner address pool range.
 *
 * @param[in]  role       Provisioner role (Master or Standby).
 * @param[out] pool_start Pointer to store start of pool.
 * @param[out] pool_end   Pointer to store end of pool.
 */
void meshx_prov_get_address_pool(meshx_prov_role_t role, uint16_t *pool_start, uint16_t *pool_end);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_GROUP_ALLOCATOR_H__ */
