/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_subnet_bridge.h
 * @brief Active-Standby Redundancy, Split-Brain Fencing, Transit Loop Prevention & Route Table.
 */

#ifndef MESHX_SUBNET_BRIDGE_H
#define MESHX_SUBNET_BRIDGE_H

#include "island_subnet_coex/meshx_subnet_types.h"
#include "island_subnet_coex/meshx_router_queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Per-boundary Active-Standby Redundancy Machine */
typedef struct {
    uint32_t            boundary_id;
    uint32_t            active_term;
    uint16_t            active_bridge_addr;
    uint16_t            local_addr;
    uint16_t            local_score;
    uint16_t            active_score;
    uint8_t             standby_rank;
    meshx_bridge_role_t role;
    uint32_t            boot_ms;
    uint32_t            last_hb_rx_ms;
    uint32_t            last_hb_tx_ms;
    uint32_t            claim_backoff_deadline_ms;
    bool                claim_pending;
    bool                valid;
} meshx_boundary_redundancy_t;

/** 8-Byte Keyed Circular Echo Filter Entry (32 entries = 256 B) */
typedef struct {
    uint16_t orig_src_addr;
    uint16_t orig_dst_addr;
    uint16_t tx_tag_crc16;
    uint16_t timestamp_ds; /**< Deciseconds mod 65536 (for 5.0s TTL = 50 ds) */
} meshx_echo_filter_entry_t;

/** Multi-Hop Transit Routing Table Entry (max 8 entries) */
typedef struct {
    uint32_t dest_subnet_id;
    uint32_t next_hop_subnet_id;
    uint16_t next_hop_bridge_addr;
    uint8_t  hop_count;
    uint16_t metric;
    uint32_t last_updated_ms;
    bool     valid;
} meshx_transit_route_entry_t;

typedef struct {
    meshx_boundary_redundancy_t boundaries[MESHX_MAX_BRIDGING_SLOTS];
    meshx_echo_filter_entry_t   echo_filter[MESHX_TRANSIT_ECHO_FILTER_SIZE];
    uint8_t                     echo_head;
    meshx_transit_route_entry_t routes[MESHX_ROUTE_ADV_TABLE_SIZE];
    bool                        is_mains_powered;
    uint8_t                     rssi_score;
} meshx_subnet_bridge_mgr_t;

void meshx_subnet_bridge_init(meshx_subnet_bridge_mgr_t *mgr,
                              uint16_t local_addr,
                              bool mains_powered,
                              uint8_t rssi_score,
                              uint32_t now_ms);

/** Compute deterministic 16-bit Bridge Failover Score: mains(8000)|rssi|free_heap|addr tiebreak */
uint16_t meshx_subnet_bridge_compute_score(bool mains_powered,
                                           uint8_t rssi_score,
                                           uint32_t free_heap_bytes,
                                           uint16_t bridge_addr);

meshx_err_t meshx_subnet_bridge_add_boundary(meshx_subnet_bridge_mgr_t *mgr,
                                             uint32_t boundary_id,
                                             meshx_bridge_role_t role,
                                             uint16_t active_addr,
                                             uint8_t standby_rank,
                                             uint32_t term,
                                             uint32_t now_ms);

/** Periodic 4s heartbeat / 12s watchdog tick. Outputs action flags (send_hb, send_claim). */
meshx_err_t meshx_subnet_bridge_tick(meshx_subnet_bridge_mgr_t *mgr,
                                     uint32_t now_ms,
                                     uint32_t free_heap_bytes,
                                     uint32_t *out_hb_boundary_id,
                                     uint32_t *out_claim_boundary_id);

/** Handle inbound 0xD1 Heartbeat (resets watchdog, or fences split-brain via score/term) */
meshx_err_t meshx_subnet_bridge_on_heartbeat(meshx_subnet_bridge_mgr_t *mgr,
                                             uint32_t boundary_id,
                                             uint32_t rx_term,
                                             uint16_t sender_addr,
                                             uint16_t sender_score,
                                             uint32_t now_ms,
                                             meshx_router_queue_t *queue_to_purge,
                                             bool *out_send_demote,
                                             uint16_t *out_demote_target);

/** Handle inbound 0xD2 Takeover Claim */
meshx_err_t meshx_subnet_bridge_on_takeover_claim(meshx_subnet_bridge_mgr_t *mgr,
                                                  uint32_t boundary_id,
                                                  uint32_t rx_term,
                                                  uint16_t candidate_addr,
                                                  uint16_t candidate_score,
                                                  uint32_t now_ms,
                                                  meshx_router_queue_t *queue_to_purge,
                                                  bool *out_send_demote);

/** Handle inbound 0xDB Demote (self yields cleanly within 100ms + purges boundary queues) */
meshx_err_t meshx_subnet_bridge_on_demote(meshx_subnet_bridge_mgr_t *mgr,
                                          uint32_t boundary_id,
                                          uint32_t winning_term,
                                          uint16_t winner_addr,
                                          uint16_t winner_score,
                                          uint32_t now_ms,
                                          meshx_router_queue_t *queue_to_purge);

/** Split-horizon 0xD6 Route Advertisement update & lookup */
meshx_err_t meshx_subnet_bridge_route_update(meshx_subnet_bridge_mgr_t *mgr,
                                             const meshx_uvp_route_adv_t *adv,
                                             uint32_t source_subnet_id,
                                             uint32_t now_ms);

meshx_err_t meshx_subnet_bridge_route_lookup(meshx_subnet_bridge_mgr_t *mgr,
                                             uint32_t dest_subnet_id,
                                             uint32_t now_ms,
                                             meshx_transit_route_entry_t *out_route);

/** Export non-split-horizon routes for 0xD6 TX toward peer_subnet_id */
meshx_err_t meshx_subnet_bridge_route_export(const meshx_subnet_bridge_mgr_t *mgr,
                                             uint32_t peer_subnet_id,
                                             uint16_t local_bridge_addr,
                                             meshx_uvp_route_adv_t *out_adv);

/** Loop Prevention Checks: 23B transit visited mask + hop_limit + 5.0s keyed echo filter */
bool meshx_subnet_bridge_transit_check_and_mark(meshx_subnet_bridge_mgr_t *mgr,
                                                meshx_subnet_transit_hdr_t *hdr,
                                                uint8_t current_slot_bit,
                                                uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* MESHX_SUBNET_BRIDGE_H */

