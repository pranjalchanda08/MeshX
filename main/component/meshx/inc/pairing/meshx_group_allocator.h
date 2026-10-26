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

/* Unified 15-Bit Unicast Address Space (TRD Page 1 & TASK-BPROV-001) */
#define MESHX_UNICAST_ADDR_START        0x0002
#define MESHX_UNICAST_ADDR_END          0x7FFF

/* Deprecated legacy pool definitions kept as aliases for backward compatibility */
#define MESHX_PROV_POOL_A_START         MESHX_UNICAST_ADDR_START
#define MESHX_PROV_POOL_A_END           MESHX_UNICAST_ADDR_END
#define MESHX_PROV_POOL_B_START         MESHX_UNICAST_ADDR_START
#define MESHX_PROV_POOL_B_END           MESHX_UNICAST_ADDR_END

/* Paged Sparse 2-Bit Bitmap Allocator Definitions (TRD Page 7 & TASK-BPROV-002) */
#define MESHX_ADDR_STATE_FREE           0x00  /* 00b = Free */
#define MESHX_ADDR_STATE_IN_FLIGHT      0x01  /* 01b = In-Flight Lease */
#define MESHX_ADDR_STATE_ASSIGNED       0x02  /* 10b = Committed / Assigned */
#define MESHX_ADDR_STATE_QUARANTINED    0x03  /* 11b = 24h RPL Quarantine */

#define MESHX_CHUNK_ADDR_COUNT          128
#define MESHX_CHUNK_BYTE_SIZE           (MESHX_CHUNK_ADDR_COUNT / 4) /* 32 Bytes (2 bits/addr) */
#define MESHX_MAX_CHUNKS                256                          /* 256 * 128 = 32,768 addrs */
#define CONFIG_MAX_CONCURRENT_LEASES    5
#define MESHX_LEASE_WATCHDOG_TIMEOUT_MS 30000                        /* 30 seconds auto-rollback */

typedef struct {
    uint8_t data[MESHX_CHUNK_BYTE_SIZE]; /* 2 bits per address */
} meshx_addr_chunk_t;

typedef struct {
    uint32_t chunk_presence_mask[8];     /* 256 bits = 32 Bytes */
    meshx_addr_chunk_t *chunks[MESHX_MAX_CHUNKS]; /* Sparse pointers to chunks in RAM */
    uint16_t active_chunk_count;         /* Number of active chunks in RAM */
    uint16_t next_free_addr_hint;        /* Monotonic search watermark */
    uint8_t  dirty_node_count;           /* Counter for NVS write debouncing */
    uint8_t  reserved[3];                /* 32-bit alignment padding */
} meshx_sparse_addr_map_t;

/**
 * @brief State Freshness Vector for Active/Standby Provisioner synchronization (TRD Page 5 Section 2).
 */
typedef struct {
    uint32_t sync_epoch;        /**< Master state sync epoch */
    uint16_t assigned_count;    /**< Total committed addresses */
    uint16_t next_free_hint;    /**< Monotonic search watermark */
    uint32_t chunk_mask_crc32;  /**< CRC32 across active chunk presence mask */
} MESHX_PACKED meshx_freshness_vector_t;

typedef struct {
    meshx_uuid_t target_uuid;   /* UUID of target unprovisioned device */
    uint32_t lease_id;          /* Monotonic lease sequence ID */
    uint16_t base_addr;         /* Base unicast address allocated */
    uint8_t  element_count;     /* Number of contiguous elements */
    uint8_t  lease_state;       /* 0x00=IDLE, 0x01=ACTIVE_WAITING_REPORT */
    uint32_t lease_timestamp;   /* Tick or epoch ms when lease granted */
    uint16_t proxy_unicast_addr;/* Address of frontier proxy holding lease */
    uint8_t  reserved[2];       /* 32-bit alignment padding */
} meshx_lease_entry_t;          /* 32 Bytes per entry */

typedef struct {
    meshx_lease_entry_t entries[CONFIG_MAX_CONCURRENT_LEASES]; /* 160 Bytes */
    uint8_t active_count;
    uint8_t reserved[3];
} meshx_lease_table_t;

/* =========================================================================
 * RPL Quarantine Table (TASK-REPLACE-001 / TRD Page 9 Section 3.3)
 * ========================================================================= */

#define CONFIG_MAX_QUARANTINED_BLOCKS 16
#define MESHX_QUARANTINE_TIMEOUT_SEC  86400U /* 24 Hours in seconds */

/**
 * @brief Entry in the Replay Protection List (RPL) 24-hour decay quarantine table.
 */
typedef struct {
    uint16_t base_addr;               /**< Base address quarantined */
    uint8_t  element_count;           /**< Contiguous address span */
    uint32_t release_epoch_timestamp; /**< System timestamp in seconds when quarantine expires */
    bool     active;                  /**< True if quarantine entry is active */
} meshx_quarantine_entry_t;

/**
 * @brief Table tracking active quarantined unicast address ranges.
 */
typedef struct {
    meshx_quarantine_entry_t entries[CONFIG_MAX_QUARANTINED_BLOCKS];
    uint8_t                  active_count;
} meshx_quarantine_table_t;

/**
 * @brief Register a decommissioned or surplus address block in the 24-hour RPL Quarantine Table.
 *
 * @param[in] base_addr     Base unicast address to quarantine.
 * @param[in] element_count Number of contiguous addresses to quarantine.
 * @param[in] timeout_sec   Quarantine hold duration in seconds (typically MESHX_QUARANTINE_TIMEOUT_SEC).
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_quarantine_register(uint16_t base_addr, uint8_t element_count, uint32_t timeout_sec);

/**
 * @brief Check if a target unicast address range overlaps with any actively quarantined block.
 *
 * @param[in] base_addr     Base unicast address.
 * @param[in] element_count Number of contiguous addresses.
 * @return true if range overlaps an active quarantine, false if clear.
 */
bool meshx_quarantine_is_range_blocked(uint16_t base_addr, uint8_t element_count);

/**
 * @brief Poll and release expired entries in the RPL Quarantine Table.
 */
void meshx_quarantine_poll_expiry(void);

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

/**
 * @brief Initialize the Paged Sparse 2-Bit Bitmap Allocator.
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t meshx_sparse_addr_map_init(void);

/**
 * @brief Get the 2-bit state of a unicast address.
 *
 * @param[in] addr Unicast address (0x0002..0x7FFF).
 * @return State: MESHX_ADDR_STATE_FREE, IN_FLIGHT, ASSIGNED, or QUARANTINED.
 */
uint8_t meshx_sparse_addr_get_state(uint16_t addr);

/**
 * @brief Set the 2-bit state of a unicast address.
 *
 * @param[in] addr  Unicast address (0x0002..0x7FFF).
 * @param[in] state State to set.
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t meshx_sparse_addr_set_state(uint16_t addr, uint8_t state);

/**
 * @brief Allocate a contiguous block of in-flight addresses under a lease.
 *
 * @param[in]  element_count Number of contiguous elements needed.
 * @param[in]  proxy_addr    Frontier proxy unicast address requesting lease.
 * @param[in]  target_uuid   16-byte UUID of unprovisioned device.
 * @param[out] out_base_addr Allocated base unicast address.
 * @param[out] out_lease_id  Allocated lease ID.
 * @return MESHX_SUCCESS on success, or MESHX_NO_MEM.
 */
meshx_err_t meshx_lease_allocate_block(uint8_t element_count,
                                       uint16_t proxy_addr,
                                       const meshx_uuid_t *target_uuid,
                                       uint16_t *out_base_addr,
                                       uint32_t *out_lease_id);

/**
 * @brief Commit an in-flight lease to permanent ASSIGNED state upon completion.
 *
 * @param[in] lease_id Lease ID returned by meshx_lease_allocate_block.
 * @return MESHX_SUCCESS on success, or MESHX_NOT_FOUND.
 */
meshx_err_t meshx_lease_commit(uint32_t lease_id);

/**
 * @brief Roll back an in-flight lease to FREE state (e.g. on proxy crash or timeout).
 *
 * @param[in] lease_id Lease ID returned by meshx_lease_allocate_block.
 * @return MESHX_SUCCESS on success, or MESHX_NOT_FOUND.
 */
meshx_err_t meshx_lease_rollback(uint32_t lease_id);

/**
 * @brief Roll back all active in-flight leases across the system.
 *
 * Used when a fast bulk provisioning session is canceled or aborted.
 *
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t meshx_lease_rollback_all(void);

/**
 * @brief Process lease timeouts (called periodically every 1000 ms).
 * Automatically rolls back any active lease exceeding MESHX_LEASE_WATCHDOG_TIMEOUT_MS.
 */
void meshx_lease_watchdog_poll(void);

/**
 * @brief Compute the current local State Freshness Vector (TRD Page 5 Section 2).
 *
 * @param[out] out_vector Pointer to meshx_freshness_vector_t structure.
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t meshx_freshness_vector_compute(meshx_freshness_vector_t *out_vector);

/**
 * @brief Merge an incoming 32-byte chunk bitmap from peer provisioner using additive bitwise OR.
 *
 * Non-destructive state merge (TRD Page 5 Section 4.2): assigned slots always take precedence.
 *
 * @param[in] chunk_idx Chunk index (0..255).
 * @param[in] chunk_data 32-byte bitmap chunk (2 bits per address).
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t meshx_sparse_addr_merge_chunk(uint16_t chunk_idx, const uint8_t chunk_data[MESHX_CHUNK_BYTE_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_GROUP_ALLOCATOR_H__ */
