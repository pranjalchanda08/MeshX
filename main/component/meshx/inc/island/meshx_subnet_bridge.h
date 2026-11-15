/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_subnet_bridge.h
 * @brief Active-Standby Redundancy, Split-Brain Fencing, Transit Loop Prevention & Route Table.
 */

#ifndef MESHX_SUBNET_BRIDGE_H
#define MESHX_SUBNET_BRIDGE_H

#include "island/meshx_subnet_types.h"
#include "island/meshx_router_queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Per-boundary Active-Standby Redundancy State Machine Context.
 */
typedef struct {
    uint32_t            boundary_id;               /**< 32-bit unique boundary link identifier */
    uint32_t            active_term;               /**< Term / Epoch counter of current Active bridge */
    uint16_t            active_bridge_addr;        /**< Unicast address of active bridge node */
    uint16_t            local_addr;                /**< Unicast address of this local node */
    uint16_t            local_score;               /**< Failover score computed for local node */
    uint16_t            active_score;              /**< Failover score advertised by current Active */
    uint8_t             standby_rank;              /**< Deterministic standby election rank (0 = highest) */
    meshx_bridge_role_t role;                      /**< Current role (NONE, STANDBY, ACTIVE) */
    uint32_t            boot_ms;                   /**< Boot timestamp in milliseconds */
    uint32_t            last_hb_rx_ms;             /**< Timestamp of last received Heartbeat */
    uint32_t            last_hb_tx_ms;             /**< Timestamp of last transmitted Heartbeat */
    uint32_t            claim_backoff_deadline_ms; /**< Rank-based backoff deadline for takeover claim */
    bool                claim_pending;             /**< True if currently backing off before takeover claim */
    bool                valid;                     /**< Whether this boundary entry is configured */
} meshx_boundary_redundancy_t;

/**
 * @brief 8-Byte Keyed Circular Echo Filter Entry (32 entries = 256 B).
 */
typedef struct {
    uint16_t orig_src_addr;  /**< Originator source unicast address */
    uint16_t orig_dst_addr;  /**< Final destination unicast/group address */
    uint16_t tx_tag_crc16;   /**< CRC-16 of transaction payload tag */
    uint16_t timestamp_ds;   /**< Deciseconds mod 65536 (for 5.0s TTL = 50 ds) */
} meshx_echo_filter_entry_t;

/**
 * @brief Multi-Hop Transit Routing Table Entry (max 8 entries).
 */
typedef struct {
    uint32_t dest_subnet_id;       /**< Target destination Global Subnet ID */
    uint32_t next_hop_subnet_id;   /**< Immediate next-hop Global Subnet ID */
    uint16_t next_hop_bridge_addr; /**< Unicast address of next-hop bridge node */
    uint8_t  hop_count;            /**< Hop count to reach destination subnet */
    uint16_t metric;               /**< Combined path metric */
    uint32_t last_updated_ms;      /**< Timestamp in milliseconds when route was last refreshed */
    bool     valid;                /**< Route entry validity */
} meshx_transit_route_entry_t;

/**
 * @brief Subnet Bridge Manager Context.
 */
typedef struct {
    meshx_boundary_redundancy_t boundaries[MESHX_MAX_BRIDGING_SLOTS];   /**< Configured boundary contexts */
    meshx_echo_filter_entry_t   echo_filter[MESHX_TRANSIT_ECHO_FILTER_SIZE]; /**< Transit echo suppression ring buffer */
    uint8_t                     echo_head;                                 /**< Ring buffer insertion head */
    meshx_transit_route_entry_t routes[MESHX_ROUTE_ADV_TABLE_SIZE];         /**< Multi-hop transit routing table */
    bool                        is_mains_powered;                          /**< Mains vs battery power status */
    bool                        candidate_active;                          /**< Set once boundary witness classification passes */
    uint8_t                     rssi_score;                                /**< Baseline RSSI score (0..63) */
} meshx_subnet_bridge_mgr_t;

/**
 * @brief Initialize the Subnet Bridge Manager.
 *
 * @param[out] mgr           Pointer to bridge manager to initialize.
 * @param[in]  local_addr    Unicast address of local bridge element.
 * @param[in]  mains_powered True if mains-powered, false if battery-powered.
 * @param[in]  rssi_score    Baseline RSSI quality metric (0..63).
 * @param[in]  now_ms        Current timestamp in milliseconds.
 */
void meshx_subnet_bridge_init(meshx_subnet_bridge_mgr_t *mgr,
                              uint16_t local_addr,
                              bool mains_powered,
                              uint8_t rssi_score,
                              uint32_t now_ms);

/**
 * @brief Compute deterministic 16-bit Bridge Failover Score.
 *
 * Format: mains(bit 15) | rssi_score(bits 9..14) | free_heap(bits 4..8) | addr(bits 0..3 tiebreak).
 *
 * @param[in] mains_powered   True if mains powered.
 * @param[in] rssi_score      6-bit RSSI score (0..63).
 * @param[in] free_heap_bytes Free heap in bytes.
 * @param[in] bridge_addr     Unicast bridge address for low-nibble tiebreak.
 *
 * @return 16-bit composite score.
 */
uint16_t meshx_subnet_bridge_compute_score(bool mains_powered,
                                           uint8_t rssi_score,
                                           uint32_t free_heap_bytes,
                                           uint16_t bridge_addr);

/**
 * @brief Configure or add a boundary redundancy tracking entry.
 *
 * @param[in,out] mgr          Pointer to bridge manager.
 * @param[in]     boundary_id  Unique 32-bit boundary link identifier.
 * @param[in]     role         Initial role (ACTIVE or STANDBY).
 * @param[in]     active_addr  Initial known active bridge address.
 * @param[in]     standby_rank Election priority rank (0 = highest).
 * @param[in]     term         Initial epoch term counter.
 * @param[in]     now_ms       Current timestamp in milliseconds.
 *
 * @return MESHX_SUCCESS on success, MESHX_NO_MEM if table full, or MESHX_INVALID_ARG.
 */
meshx_err_t meshx_subnet_bridge_add_boundary(meshx_subnet_bridge_mgr_t *mgr,
                                             uint32_t boundary_id,
                                             meshx_bridge_role_t role,
                                             uint16_t active_addr,
                                             uint8_t standby_rank,
                                             uint32_t term,
                                             uint32_t now_ms);

/**
 * @brief Execute periodic watchdog and heartbeat timing checks.
 *
 * Evaluates 4s heartbeat TX for active bridges, and 12s watchdog timeout
 * + rank-based backoff for standby bridges to initiate takeover claims.
 *
 * @param[in,out] mgr                   Pointer to bridge manager.
 * @param[in]     now_ms                Current timestamp in milliseconds.
 * @param[in]     free_heap_bytes       Current free heap memory in bytes.
 * @param[out]    out_hb_boundary_id    Pointer to store boundary ID needing Heartbeat TX (0 if none).
 * @param[out]    out_claim_boundary_id Pointer to store boundary ID needing Takeover Claim TX (0 if none).
 *
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG.
 */
meshx_err_t meshx_subnet_bridge_tick(meshx_subnet_bridge_mgr_t *mgr,
                                     uint32_t now_ms,
                                     uint32_t free_heap_bytes,
                                     uint32_t *out_hb_boundary_id,
                                     uint32_t *out_claim_boundary_id);

/**
 * @brief Process an inbound UVP Heartbeat message and execute split-brain fencing.
 *
 * Resets watchdog timer for standby bridges. If both peers are Active, triggers
 * score/term arbitration and outputs demote action if local node wins.
 *
 * @param[in,out] mgr               Pointer to bridge manager.
 * @param[in]     boundary_id       Boundary identifier.
 * @param[in]     rx_term           Term advertised by sender.
 * @param[in]     sender_addr       Unicast address of sender bridge.
 * @param[in]     sender_score      Failover score advertised by sender.
 * @param[in]     now_ms            Current timestamp in milliseconds.
 * @param[in,out] queue_to_purge    Optional queue context to purge if demoted (may be NULL).
 * @param[out]    out_send_demote   Pointer to store whether a Demote packet must be sent.
 * @param[out]    out_demote_target Pointer to store destination unicast address for Demote.
 *
 * @return MESHX_SUCCESS on success, MESHX_NOT_FOUND if boundary unknown, or MESHX_INVALID_ARG.
 */
meshx_err_t meshx_subnet_bridge_on_heartbeat(meshx_subnet_bridge_mgr_t *mgr,
                                             uint32_t boundary_id,
                                             uint32_t rx_term,
                                             uint16_t sender_addr,
                                             uint16_t sender_score,
                                             uint32_t now_ms,
                                             meshx_router_queue_t *queue_to_purge,
                                             bool *out_send_demote,
                                             uint16_t *out_demote_target);

/**
 * @brief Process an inbound UVP Takeover Claim message.
 *
 * Arbitrates takeover claim against current Active bridge status.
 *
 * @param[in,out] mgr             Pointer to bridge manager.
 * @param[in]     boundary_id     Boundary identifier.
 * @param[in]     rx_term         Claim term advertised by candidate.
 * @param[in]     candidate_addr  Unicast address of candidate node.
 * @param[in]     candidate_score Failover score of candidate node.
 * @param[in]     now_ms          Current timestamp in milliseconds.
 * @param[in,out] queue_to_purge  Optional queue context to purge if demoted (may be NULL).
 * @param[out]    out_send_demote Pointer to store whether a Demote packet must be sent.
 *
 * @return MESHX_SUCCESS on success, or appropriate error code.
 */
meshx_err_t meshx_subnet_bridge_on_takeover_claim(meshx_subnet_bridge_mgr_t *mgr,
                                                  uint32_t boundary_id,
                                                  uint32_t rx_term,
                                                  uint16_t candidate_addr,
                                                  uint16_t candidate_score,
                                                  uint32_t now_ms,
                                                  meshx_router_queue_t *queue_to_purge,
                                                  bool *out_send_demote);

/**
 * @brief Process an inbound UVP Demote message.
 *
 * If peer outranks local Active bridge, cleanly yields to Standby within 100ms
 * and purges pending egress queues for the boundary.
 *
 * @param[in,out] mgr            Pointer to bridge manager.
 * @param[in]     boundary_id    Boundary identifier.
 * @param[in]     winning_term   Term of winning Active bridge.
 * @param[in]     winner_addr    Unicast address of winner.
 * @param[in]     winner_score   Score of winner.
 * @param[in]     now_ms         Current timestamp in milliseconds.
 * @param[in,out] queue_to_purge Optional queue context to purge on demote (may be NULL).
 *
 * @return MESHX_SUCCESS on success, or appropriate error code.
 */
meshx_err_t meshx_subnet_bridge_on_demote(meshx_subnet_bridge_mgr_t *mgr,
                                          uint32_t boundary_id,
                                          uint32_t winning_term,
                                          uint16_t winner_addr,
                                          uint16_t winner_score,
                                          uint32_t now_ms,
                                          meshx_router_queue_t *queue_to_purge);

/**
 * @brief Update multi-hop transit routing table from received UVP Route Advertisement.
 *
 * Implements distance-vector path cost selection with split-horizon check.
 *
 * @param[in,out] mgr              Pointer to bridge manager.
 * @param[in]     adv              Pointer to parsed Route Advertisement payload.
 * @param[in]     source_subnet_id Global Subnet ID from which advertisement was received.
 * @param[in]     now_ms           Current timestamp in milliseconds.
 *
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG.
 */
meshx_err_t meshx_subnet_bridge_route_update(meshx_subnet_bridge_mgr_t *mgr,
                                             const meshx_uvp_route_adv_t *adv,
                                             uint32_t source_subnet_id,
                                             uint32_t now_ms);

/**
 * @brief Look up best active next-hop route toward destination subnet.
 *
 * @param[in,out] mgr            Pointer to bridge manager (prunes expired routes).
 * @param[in]     dest_subnet_id Target destination Global Subnet ID.
 * @param[in]     now_ms         Current timestamp in milliseconds.
 * @param[out]    out_route      Pointer to store matching routing table entry.
 *
 * @return MESHX_SUCCESS if route found, MESHX_NOT_FOUND otherwise.
 */
meshx_err_t meshx_subnet_bridge_route_lookup(meshx_subnet_bridge_mgr_t *mgr,
                                             uint32_t dest_subnet_id,
                                             uint32_t now_ms,
                                             meshx_transit_route_entry_t *out_route);

/**
 * @brief Export routes for periodic UVP Route Advertisement applying Split-Horizon.
 *
 * Routes learned from peer_subnet_id are poisoned/suppressed from advertisement.
 *
 * @param[in]  mgr               Pointer to bridge manager.
 * @param[in]  peer_subnet_id    Global Subnet ID toward which advertisement will be sent.
 * @param[in]  local_bridge_addr Local bridge unicast address.
 * @param[out] out_adv           Pointer to populate Route Advertisement payload.
 *
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG.
 */
meshx_err_t meshx_subnet_bridge_route_export(const meshx_subnet_bridge_mgr_t *mgr,
                                             uint32_t peer_subnet_id,
                                             uint16_t local_bridge_addr,
                                             meshx_uvp_route_adv_t *out_adv);

/**
 * @brief Validate transit packet loop prevention rules and update echo suppression filter.
 *
 * Checks: magic version, hop_limit > 0, payload_len <= MESHX_MAX_MULTIHOP_APP_PDU_LEN,
 * slot bit not already visited in visited_subnets_mask, and 5.0s echo filter suppression.
 *
 * @param[in,out] mgr              Pointer to bridge manager.
 * @param[in,out] hdr              Pointer to transit header to inspect and update.
 * @param[in]     current_slot_bit Bit position representing current subnet in mask.
 * @param[in]     now_ms           Current timestamp in milliseconds.
 *
 * @return true if packet is allowed to transit, false if it must be dropped.
 */
bool meshx_subnet_bridge_transit_check_and_mark(meshx_subnet_bridge_mgr_t *mgr,
                                                meshx_subnet_transit_hdr_t *hdr,
                                                uint8_t current_slot_bit,
                                                uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* MESHX_SUBNET_BRIDGE_H */
