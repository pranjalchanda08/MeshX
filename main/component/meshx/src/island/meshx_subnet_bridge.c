/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_subnet_bridge.c
 * @brief Active-Standby Redundancy, Split-Brain Fencing, Transit Loop Prevention & Route Table.
 */

#include <string.h>
#include "island/meshx_subnet_bridge.h"

/**
 * @brief Search configured boundaries for matching boundary link ID.
 *
 * @param[in]  mgr          Pointer to bridge manager.
 * @param[in]  boundary_id  Unique 32-bit boundary link identifier.
 * @param[out] out_boundary Destination for boundary context pointer.
 * @return MESHX_SUCCESS if found, MESHX_INVALID_ARG on NULL args, or MESHX_NOT_FOUND.
 */
static meshx_err_t find_boundary(meshx_subnet_bridge_mgr_t *mgr,
                                 uint32_t boundary_id,
                                 meshx_boundary_redundancy_t **out_boundary)
{
    if (!mgr || !out_boundary)
    {
        return MESHX_INVALID_ARG;
    }
    for (size_t i = 0; i < MESHX_MAX_BRIDGING_SLOTS; i++)
    {
        if (mgr->boundaries[i].valid && mgr->boundaries[i].boundary_id == boundary_id)
        {
            *out_boundary = &mgr->boundaries[i];
            return MESHX_SUCCESS;
        }
    }
    *out_boundary = NULL;
    return MESHX_NOT_FOUND;
}

/**
 * @brief Arbitrate Active bridge superiority between local node and remote peer.
 *
 * Evaluates term > score > unicast address tie-break.
 *
 * @param[in] local_term   Current epoch term counter of local node.
 * @param[in] local_score  Failover score of local node.
 * @param[in] local_addr   Unicast address of local node.
 * @param[in] rx_term      Epoch term advertised by remote peer.
 * @param[in] sender_score Failover score advertised by remote peer.
 * @param[in] sender_addr  Unicast address of remote peer.
 *
 * @return MESHX_SUCCESS if peer wins arbitration, MESHX_FAIL if local node wins.
 */
static meshx_err_t peer_wins_arbitration(uint32_t local_term,
                                         uint16_t local_score,
                                         uint16_t local_addr,
                                         uint32_t rx_term,
                                         uint16_t sender_score,
                                         uint16_t sender_addr)
{
    if (rx_term > local_term)
    {
        return MESHX_SUCCESS;
    }
    if (rx_term < local_term)
    {
        return MESHX_FAIL;
    }
    if (sender_score != local_score)
    {
        return (sender_score > local_score) ? MESHX_SUCCESS : MESHX_FAIL;
    }
    return (sender_addr < local_addr) ? MESHX_SUCCESS : MESHX_FAIL;
}

/**
 * @brief Initialize the Subnet Bridge Manager.
 *
 * @param[out] mgr           Pointer to bridge manager to initialize.
 * @param[in]  local_addr    Unicast address of local bridge element.
 * @param[in]  mains_powered True if mains-powered, false if battery-powered.
 * @param[in]  rssi_score    Baseline RSSI quality metric (0..63).
 * @param[in]  now_ms        Current timestamp in milliseconds.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if @p mgr is NULL.
 */
meshx_err_t meshx_subnet_bridge_init(meshx_subnet_bridge_mgr_t *mgr,
                                     uint16_t local_addr,
                                     bool mains_powered,
                                     uint8_t rssi_score,
                                     uint32_t now_ms)
{
    MESHX_UNUSED(now_ms);
    if (!mgr)
    {
        return MESHX_INVALID_ARG;
    }
    memset(mgr, 0, sizeof(*mgr));
    mgr->is_mains_powered = mains_powered;
    mgr->rssi_score = rssi_score;
    for (size_t i = 0; i < MESHX_MAX_BRIDGING_SLOTS; i++)
    {
        mgr->boundaries[i].local_addr = local_addr;
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Compute deterministic 16-bit Bridge Failover Score.
 *
 * Format: mains(bit 15) | rssi_score(bits 9..14) | free_heap(bits 4..8) | addr(bits 0..3 tiebreak).
 *
 * @param[in]  mains_powered   True if mains powered.
 * @param[in]  rssi_score      6-bit RSSI score (0..63).
 * @param[in]  free_heap_bytes Free heap in bytes.
 * @param[in]  bridge_addr     Unicast bridge address for low-nibble tiebreak.
 * @param[out] out_score       Destination for 16-bit composite score.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if @p out_score is NULL.
 */
meshx_err_t meshx_subnet_bridge_compute_score(bool mains_powered,
                                              uint8_t rssi_score,
                                              uint32_t free_heap_bytes,
                                              uint16_t bridge_addr,
                                              uint16_t *out_score)
{
    if (!out_score)
    {
        return MESHX_INVALID_ARG;
    }
    uint16_t score = mains_powered ? MESHX_BRIDGE_SCORE_MAINS_FLAG : 0x0000U;
    uint16_t rssi_bits = (uint16_t)(rssi_score & MESHX_BRIDGE_SCORE_RSSI_MASK);
    uint32_t heap_kb = free_heap_bytes >> MESHX_BRIDGE_SCORE_HEAP_KB_SHIFT;
    if (heap_kb > MESHX_BRIDGE_SCORE_MAX_HEAP_KB)
    {
        heap_kb = MESHX_BRIDGE_SCORE_MAX_HEAP_KB;
    }
    score |= (uint16_t)(rssi_bits << MESHX_BRIDGE_SCORE_RSSI_SHIFT);
    score |= (uint16_t)((heap_kb & MESHX_BRIDGE_SCORE_HEAP_MASK) << MESHX_BRIDGE_SCORE_HEAP_SHIFT);
    score |= (uint16_t)(bridge_addr & MESHX_BRIDGE_SCORE_ADDR_MASK);
    *out_score = score;
    return MESHX_SUCCESS;
}

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
                                             uint32_t now_ms)
{
    if (!mgr || boundary_id == 0UL)
    {
        return MESHX_INVALID_ARG;
    }
    meshx_boundary_redundancy_t *existing = NULL;
    meshx_err_t ferr = find_boundary(mgr, boundary_id, &existing);
    if (ferr != MESHX_SUCCESS && ferr != MESHX_NOT_FOUND)
    {
        return ferr;
    }
    if (!existing)
    {
        for (size_t i = 0; i < MESHX_MAX_BRIDGING_SLOTS; i++)
        {
            if (!mgr->boundaries[i].valid)
            {
                existing = &mgr->boundaries[i];
                break;
            }
        }
    }
    if (!existing)
    {
        return MESHX_NO_MEM;
    }

    uint16_t local_addr = existing->local_addr;
    memset(existing, 0, sizeof(*existing));
    existing->role               = role;
    existing->active_term        = term;
    existing->boot_ms            = now_ms;
    existing->last_hb_rx_ms      = now_ms;
    existing->last_hb_tx_ms      = now_ms;
    existing->local_addr         = local_addr;
    existing->boundary_id        = boundary_id;
    existing->active_bridge_addr = active_addr;
    existing->standby_rank       = standby_rank;
    ferr = meshx_subnet_bridge_compute_score(mgr->is_mains_powered,
                                             mgr->rssi_score,
                                             MESHX_WITNESS_DEFAULT_HEAP_BYTES,
                                             local_addr,
                                             &existing->local_score);
    if (ferr != MESHX_SUCCESS)
    {
        return ferr;
    }
    existing->valid              = true;
    return MESHX_SUCCESS;
}

/**
 * @brief Remove a boundary entry from the redundancy manager.
 *
 * @param[in,out] mgr         Pointer to bridge manager.
 * @param[in]     boundary_id Boundary Global Subnet ID to remove.
 *
 * @return MESHX_SUCCESS on success, MESHX_NOT_FOUND if absent, or MESHX_INVALID_ARG.
 */
meshx_err_t meshx_subnet_bridge_remove_boundary(meshx_subnet_bridge_mgr_t *mgr,
                                                uint32_t boundary_id)
{
    if (!mgr || boundary_id == 0UL)
    {
        return MESHX_INVALID_ARG;
    }
    meshx_boundary_redundancy_t *b = NULL;
    meshx_err_t ferr = find_boundary(mgr, boundary_id, &b);
    if (ferr != MESHX_SUCCESS)
    {
        return ferr;
    }
    uint16_t local_addr = b->local_addr;
    memset(b, 0, sizeof(*b));
    b->local_addr = local_addr;
    return MESHX_SUCCESS;
}

/**
 * @brief Tick all active boundaries and return bitmasks of heartbeat/claim actions.
 *
 * @param[in,out] mgr             Pointer to bridge manager.
 * @param[in]     now_ms          Current monotonic timestamp in milliseconds.
 * @param[in]     free_heap_bytes Current free heap in bytes for score computation.
 * @param[out]    out_hb_mask     Bitmask of boundaries due to emit a heartbeat.
 * @param[out]    out_claim_mask  Bitmask of boundaries due to emit a takeover claim.
 *
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG.
 */
meshx_err_t meshx_subnet_bridge_tick_all(meshx_subnet_bridge_mgr_t *mgr,
                                         uint32_t now_ms,
                                         uint32_t free_heap_bytes,
                                         uint32_t *out_hb_mask,
                                         uint32_t *out_claim_mask)
{
    if (!mgr || !out_hb_mask || !out_claim_mask)
    {
        return MESHX_INVALID_ARG;
    }
    *out_hb_mask = 0UL;
    *out_claim_mask = 0UL;

    for (size_t i = 0; i < MESHX_MAX_BRIDGING_SLOTS; i++)
    {
        meshx_boundary_redundancy_t *b = &mgr->boundaries[i];
        if (!b->valid)
        {
            continue;
        }
        meshx_err_t serr = meshx_subnet_bridge_compute_score(mgr->is_mains_powered,
                                                           mgr->rssi_score,
                                                           free_heap_bytes,
                                                           b->local_addr,
                                                           &b->local_score);
        if (serr != MESHX_SUCCESS)
        {
            return serr;
        }

        if (b->role == MESHX_BRIDGE_ROLE_ACTIVE)
        {
            if ((now_ms - b->last_hb_tx_ms) >= MESHX_BRIDGE_HEARTBEAT_MS)
            {
                b->last_hb_tx_ms = now_ms;
                *out_hb_mask |= (1UL << i);
            }
        }
        else if (b->role == MESHX_BRIDGE_ROLE_STANDBY)
        {
            if ((now_ms - b->boot_ms) < MESHX_BRIDGE_BOOT_GRACE_MS)
            {
                continue;
            }
            uint32_t watchdog_at = b->last_hb_rx_ms + MESHX_BRIDGE_WATCHDOG_MS;
            uint32_t backoff_at = watchdog_at + ((uint32_t)b->standby_rank * MESHX_BRIDGE_RANK_BACKOFF_STEP_MS);
            if (!b->claim_pending && now_ms >= watchdog_at)
            {
                b->claim_pending = true;
                b->claim_backoff_deadline_ms = backoff_at;
            }
            if (b->claim_pending && now_ms >= b->claim_backoff_deadline_ms)
            {
                b->active_term++;
                b->claim_pending = false;
                b->last_hb_tx_ms = now_ms;
                b->role = MESHX_BRIDGE_ROLE_ACTIVE;
                b->active_bridge_addr = b->local_addr;
                *out_claim_mask |= (1UL << i);
            }
        }
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Legacy single-boundary tick wrapper.
 *
 * @param[in,out] mgr                  Pointer to bridge manager.
 * @param[in]     now_ms               Current monotonic timestamp in milliseconds.
 * @param[in]     free_heap_bytes      Current free heap in bytes.
 * @param[out]    out_emit_hb          Set true if a heartbeat should be emitted.
 * @param[out]    out_hb_boundary_id   Boundary ID for heartbeat emission.
 * @param[out]    out_emit_claim       Set true if a takeover claim should be emitted.
 * @param[out]    out_claim_boundary_id Boundary ID for takeover claim emission.
 *
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_subnet_bridge_tick(meshx_subnet_bridge_mgr_t *mgr,
                                     uint32_t now_ms,
                                     uint32_t free_heap_bytes,
                                     uint32_t *out_hb_boundary_id,
                                     uint32_t *out_claim_boundary_id)
{
    if (!mgr || !out_hb_boundary_id || !out_claim_boundary_id)
    {
        return MESHX_INVALID_ARG;
    }
    *out_hb_boundary_id = 0UL;
    *out_claim_boundary_id = 0UL;
    uint32_t hb_mask = 0UL;
    uint32_t claim_mask = 0UL;
    meshx_err_t err = meshx_subnet_bridge_tick_all(mgr, now_ms, free_heap_bytes, &hb_mask, &claim_mask);
    if (err != MESHX_SUCCESS)
    {
        return err;
    }
    for (size_t i = 0; i < MESHX_MAX_BRIDGING_SLOTS; i++)
    {
        if (*out_hb_boundary_id == 0UL && (hb_mask & (1UL << i)) != 0UL)
        {
            *out_hb_boundary_id = mgr->boundaries[i].boundary_id;
        }
        if (*out_claim_boundary_id == 0UL && (claim_mask & (1UL << i)) != 0UL)
        {
            *out_claim_boundary_id = mgr->boundaries[i].boundary_id;
        }
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Demote a boundary to STANDBY after losing split-brain arbitration and purge its queue.
 *
 * @param[in,out] b   Boundary redundancy context to demote.
 * @param[in]     evt Peer event containing winning term, address, score, and optional queue.
 * @return MESHX_SUCCESS on success, or error from meshx_router_queue_purge_boundary().
 */
static meshx_err_t demote_to_standby(meshx_boundary_redundancy_t *b,
                                     const meshx_bridge_peer_event_t *evt)
{
    b->role               = MESHX_BRIDGE_ROLE_STANDBY;
    b->active_term        = evt->rx_term;
    b->active_bridge_addr = evt->peer_addr;
    b->active_score       = evt->peer_score;
    b->last_hb_rx_ms      = evt->now_ms;
    b->claim_pending      = false;
    if (evt->queue_to_purge)
    {
        return meshx_router_queue_purge_boundary(evt->queue_to_purge, evt->boundary_id, NULL);
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Process an inbound UVP Heartbeat message and execute split-brain fencing.
 *
 * @param[in,out] mgr               Pointer to bridge manager.
 * @param[in]     evt               Pointer to peer heartbeat event descriptor.
 * @param[out]    out_send_demote   Pointer to store whether a Demote packet must be sent.
 * @param[out]    out_demote_target Pointer to store destination unicast address for Demote.
 *
 * @return MESHX_SUCCESS on success, MESHX_NOT_FOUND if boundary unknown, or MESHX_INVALID_ARG.
 */
meshx_err_t meshx_subnet_bridge_on_heartbeat(meshx_subnet_bridge_mgr_t *mgr,
                                             const meshx_bridge_peer_event_t *evt,
                                             bool *out_send_demote,
                                             uint16_t *out_demote_target)
{
    if (out_send_demote)
    {
        *out_send_demote = false;
    }
    if (out_demote_target)
    {
        *out_demote_target = 0U;
    }
    if (!mgr || !evt)
    {
        return MESHX_INVALID_ARG;
    }
    meshx_boundary_redundancy_t *b = NULL;
    meshx_err_t ferr = find_boundary(mgr, evt->boundary_id, &b);
    if (ferr != MESHX_SUCCESS)
    {
        return ferr;
    }

    if (evt->rx_term < b->active_term)
    {
        return MESHX_SUCCESS;
    }

    if (b->role == MESHX_BRIDGE_ROLE_ACTIVE && evt->peer_addr != b->local_addr)
    {
        if (peer_wins_arbitration(b->active_term, b->local_score, b->local_addr,
                                  evt->rx_term, evt->peer_score, evt->peer_addr) == MESHX_SUCCESS)
        {
            return demote_to_standby(b, evt);
        }
        if (out_send_demote)
        {
            *out_send_demote = true;
        }
        if (out_demote_target)
        {
            *out_demote_target = evt->peer_addr;
        }
        return MESHX_SUCCESS;
    }

    b->active_term        = evt->rx_term;
    b->active_bridge_addr = evt->peer_addr;
    b->active_score       = evt->peer_score;
    b->last_hb_rx_ms      = evt->now_ms;
    b->claim_pending      = false;
    return MESHX_SUCCESS;
}

/**
 * @brief Process an inbound UVP Takeover Claim message.
 *
 * Arbitrates takeover claim against current Active bridge status.
 *
 * @param[in,out] mgr               Pointer to bridge manager.
 * @param[in]     evt               Pointer to peer takeover claim event descriptor.
 * @param[out]    out_send_demote   Pointer to store whether a Demote packet must be sent.
 * @param[out]    out_demote_target Pointer to store destination unicast address for Demote.
 *
 * @return MESHX_SUCCESS on success, or appropriate error code.
 */
meshx_err_t meshx_subnet_bridge_on_takeover_claim(meshx_subnet_bridge_mgr_t *mgr,
                                                  const meshx_bridge_peer_event_t *evt,
                                                  bool *out_send_demote,
                                                  uint16_t *out_demote_target)
{
    return meshx_subnet_bridge_on_heartbeat(mgr, evt, out_send_demote, out_demote_target);
}

/**
 * @brief Process an inbound UVP Demote message.
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
                                          meshx_router_queue_t *queue_to_purge)
{
    if (!mgr)
    {
        return MESHX_INVALID_ARG;
    }
    meshx_boundary_redundancy_t *b = NULL;
    meshx_err_t ferr = find_boundary(mgr, boundary_id, &b);
    if (ferr != MESHX_SUCCESS)
    {
        return ferr;
    }
    if (winner_addr == b->local_addr)
    {
        return MESHX_SUCCESS;
    }

    if (peer_wins_arbitration(b->active_term, b->local_score, b->local_addr,
                              winning_term, winner_score, winner_addr) == MESHX_SUCCESS)
    {
        b->role = MESHX_BRIDGE_ROLE_STANDBY;
        b->active_term = winning_term;
        b->active_bridge_addr = winner_addr;
        b->active_score = winner_score;
        b->last_hb_rx_ms = now_ms;
        b->claim_pending = false;
        if (queue_to_purge)
        {
            meshx_err_t perr = meshx_router_queue_purge_boundary(queue_to_purge, boundary_id, NULL);
            if (perr != MESHX_SUCCESS)
            {
                return perr;
            }
        }
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Remove poisoned routes (hop_count == 0xFF) learned from the advertising neighbor.
 *
 * @param[in,out] mgr              Pointer to bridge manager.
 * @param[in]     dest_subnet_id   Destination Global Subnet ID being poisoned.
 * @param[in]     source_subnet_id Next-hop Global Subnet ID that advertised the poison.
 * @return MESHX_SUCCESS on success.
 */
static meshx_err_t prune_poisoned_routes(meshx_subnet_bridge_mgr_t *mgr,
                                         uint32_t dest_subnet_id,
                                         uint32_t source_subnet_id)
{
    for (int i = 0; i < (int)MESHX_ROUTE_ADV_TABLE_SIZE; i++)
    {
        if (mgr->routes[i].valid &&
            mgr->routes[i].dest_subnet_id == dest_subnet_id &&
            mgr->routes[i].next_hop_subnet_id == source_subnet_id)
        {
            memset(&mgr->routes[i], 0, sizeof(mgr->routes[i]));
        }
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Insert or refresh a single route entry in the multi-hop transit routing table.
 *
 * @param[in,out] mgr              Pointer to bridge manager.
 * @param[in]     src_e            Advertised destination entry.
 * @param[in]     source_subnet_id Next-hop Global Subnet ID.
 * @param[in]     via_bridge_addr  Advertising neighbor bridge address.
 * @param[in]     now_ms           Current timestamp in milliseconds.
 * @return MESHX_SUCCESS on success.
 */
static meshx_err_t upsert_route_entry(meshx_subnet_bridge_mgr_t *mgr,
                                      const meshx_route_adv_dest_t *src_e,
                                      uint32_t source_subnet_id,
                                      uint16_t via_bridge_addr,
                                      uint32_t now_ms)
{
    uint8_t new_hops = (uint8_t)(src_e->hop_count + 1U);
    if (new_hops > MESHX_TRANSIT_DEFAULT_HOP_LIMIT)
    {
        return MESHX_SUCCESS;
    }

    int match_idx = -1;
    int free_idx = -1;
    int worst_idx = 0;
    for (int i = 0; i < (int)MESHX_ROUTE_ADV_TABLE_SIZE; i++)
    {
        const meshx_transit_route_entry_t *r = &mgr->routes[i];
        if (!r->valid)
        {
            if (free_idx < 0)
            {
                free_idx = i;
            }
            continue;
        }
        if (r->dest_subnet_id == src_e->dest_subnet_id)
        {
            match_idx = i;
            break;
        }
        if (r->hop_count > mgr->routes[worst_idx].hop_count)
        {
            worst_idx = i;
        }
    }

    int idx = worst_idx;
    if (match_idx >= 0)
    {
        idx = match_idx;
    }
    else if (free_idx >= 0)
    {
        idx = free_idx;
    }

    meshx_transit_route_entry_t *dst = &mgr->routes[idx];
    bool replace = (match_idx < 0) ||
                   (dst->next_hop_subnet_id == source_subnet_id) ||
                   (new_hops < dst->hop_count) ||
                   (new_hops == dst->hop_count && src_e->route_metric < dst->metric);
    if (replace)
    {
        dst->dest_subnet_id = src_e->dest_subnet_id;
        dst->next_hop_subnet_id = source_subnet_id;
        dst->next_hop_bridge_addr = via_bridge_addr;
        dst->hop_count = new_hops;
        dst->metric = src_e->route_metric;
        dst->last_updated_ms = now_ms;
        dst->valid = true;
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Update multi-hop transit routing table from received UVP Route Advertisement.
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
                                             uint32_t now_ms)
{
    if (!mgr || !adv)
    {
        return MESHX_INVALID_ARG;
    }
    meshx_err_t err = MESHX_SUCCESS;
    uint8_t n = (adv->entry_count > MESHX_ROUTE_ADV_MAX_ENTRIES_PER_MSG) ?
                 MESHX_ROUTE_ADV_MAX_ENTRIES_PER_MSG : adv->entry_count;

    for (uint8_t k = 0U; k < n; k++)
    {
        const meshx_route_adv_dest_t *src_e = &adv->entries[k];
        if (src_e->dest_subnet_id == MESHX_GLOBAL_SUBNET_INVALID)
        {
            continue;
        }
        if (src_e->hop_count == 0xFFU)
        {
            err = prune_poisoned_routes(mgr, src_e->dest_subnet_id, source_subnet_id);
        }
        else
        {
            err = upsert_route_entry(mgr, src_e, source_subnet_id, adv->via_bridge_addr, now_ms);
        }
        if (err != MESHX_SUCCESS)
        {
            return err;
        }
    }
    return MESHX_SUCCESS;
}

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
                                             meshx_transit_route_entry_t *out_route)
{
    if (!mgr || !out_route)
    {
        return MESHX_INVALID_ARG;
    }
    for (size_t i = 0; i < MESHX_ROUTE_ADV_TABLE_SIZE; i++)
    {
        meshx_transit_route_entry_t *r = &mgr->routes[i];
        if (!r->valid)
        {
            continue;
        }
        if ((now_ms - r->last_updated_ms) > MESHX_ROUTE_ADV_TTL_MS)
        {
            r->valid = false;
            continue;
        }
        if (r->dest_subnet_id == dest_subnet_id)
        {
            *out_route = *r;
            return MESHX_SUCCESS;
        }
    }
    return MESHX_NOT_FOUND;
}

/**
 * @brief Export routes for periodic UVP Route Advertisement applying Split-Horizon.
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
                                             meshx_uvp_route_adv_t *out_adv)
{
    if (!mgr || !out_adv)
    {
        return MESHX_INVALID_ARG;
    }
    memset(out_adv, 0, sizeof(*out_adv));
    out_adv->via_bridge_addr = local_bridge_addr;

    for (size_t i = 0; i < MESHX_ROUTE_ADV_TABLE_SIZE &&
         out_adv->entry_count < MESHX_ROUTE_ADV_MAX_ENTRIES_PER_MSG; i++)
    {
        const meshx_transit_route_entry_t *r = &mgr->routes[i];
        if (!r->valid)
        {
            continue;
        }
        /* Split-horizon: suppress advertising routes learned directly from peer_subnet_id */
        if (r->next_hop_subnet_id == peer_subnet_id)
        {
            continue;
        }
        meshx_route_adv_dest_t *d = &out_adv->entries[out_adv->entry_count++];
        d->dest_subnet_id = r->dest_subnet_id;
        d->hop_count = r->hop_count;
        d->route_metric = r->metric;
        d->flags = 0U;
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Validate transit packet loop prevention with egress check, dual-bit marking, and keyed echo digest.
 *
 * @param[in,out] mgr         Pointer to bridge manager.
 * @param[in,out] hdr         Pointer to transit header to inspect and update.
 * @param[in]     tctx        Pointer to transit context descriptor.
 * @param[out]    out_allowed Set to true if packet is allowed to transit, false otherwise.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if @p tctx or @p out_allowed is NULL.
 */
meshx_err_t meshx_subnet_bridge_transit_check_and_mark_ex(meshx_subnet_bridge_mgr_t *mgr,
                                                          meshx_subnet_transit_hdr_t *hdr,
                                                          const meshx_bridge_transit_ctx_t *tctx,
                                                          bool *out_allowed)
{
    if (!tctx || !out_allowed)
    {
        return MESHX_INVALID_ARG;
    }
    *out_allowed = false;
    if (!mgr || !hdr || hdr->hop_limit <= 1U)
    {
        return MESHX_SUCCESS;
    }

    if (tctx->egress_subnet_id != MESHX_GLOBAL_SUBNET_INVALID &&
        hdr->orig_subnet_id == (uint16_t)(tctx->egress_subnet_id & 0xFFFFUL))
    {
        return MESHX_SUCCESS;
    }

    uint8_t in_b = (uint8_t)(tctx->in_slot_bit & 0x0FU);
    uint8_t out_b = (uint8_t)(tctx->out_slot_bit & 0x0FU);
    if ((hdr->visited_subnets_mask & (uint16_t)(1U << in_b)) != 0U)
    {
        return MESHX_SUCCESS;
    }

    uint16_t echo_tag = hdr->orig_seq_num;
    if (tctx->k_bridge)
    {
        uint8_t k_in[12] = {
            MESHX_BE_BYTE(hdr->orig_subnet_id, MESHX_SHIFT_BYTE1),
            MESHX_BE_BYTE(hdr->orig_subnet_id, MESHX_SHIFT_LSB),
            MESHX_BE_BYTE(hdr->orig_phys_src, MESHX_SHIFT_BYTE1),
            MESHX_BE_BYTE(hdr->orig_phys_src, MESHX_SHIFT_LSB),
            MESHX_BE_BYTE(hdr->orig_seq_auth, MESHX_SHIFT_MSB32),
            MESHX_BE_BYTE(hdr->orig_seq_auth, MESHX_SHIFT_BYTE2),
            MESHX_BE_BYTE(hdr->orig_seq_auth, MESHX_SHIFT_BYTE1),
            MESHX_BE_BYTE(hdr->orig_seq_auth, MESHX_SHIFT_LSB),
            tctx->k_bridge->bytes[0], tctx->k_bridge->bytes[1],
            tctx->k_bridge->bytes[2], tctx->k_bridge->bytes[3],
        };
        meshx_err_t cerr = meshx_subnet_crc16(k_in, sizeof(k_in), &echo_tag);
        if (cerr != MESHX_SUCCESS)
        {
            return cerr;
        }
    }

    uint16_t now_ds = (uint16_t)((tctx->now_ms / MESHX_TRANSIT_ECHO_DECIS_DIVISOR) & 0xFFFFUL);
    uint16_t ttl_ds = (uint16_t)(MESHX_TRANSIT_ECHO_TTL_MS / MESHX_TRANSIT_ECHO_DECIS_DIVISOR);

    for (size_t i = 0; i < MESHX_TRANSIT_ECHO_FILTER_SIZE; i++)
    {
        const meshx_echo_filter_entry_t *e = &mgr->echo_filter[i];
        if (e->orig_src_addr == hdr->orig_phys_src &&
            e->orig_dst_addr == hdr->boundary_id &&
            e->tx_tag_crc16 == echo_tag)
        {
            uint16_t elapsed_ds = (uint16_t)(now_ds - e->timestamp_ds);
            if (elapsed_ds <= ttl_ds)
            {
                return MESHX_SUCCESS;
            }
        }
    }

    meshx_echo_filter_entry_t *slot = &mgr->echo_filter[mgr->echo_head];
    slot->orig_src_addr = hdr->orig_phys_src;
    slot->orig_dst_addr = hdr->boundary_id;
    slot->tx_tag_crc16 = echo_tag;
    slot->timestamp_ds = now_ds;
    mgr->echo_head = (uint8_t)((mgr->echo_head + 1U) % MESHX_TRANSIT_ECHO_FILTER_SIZE);

    hdr->hop_limit--;
    hdr->visited_subnets_mask |= (uint16_t)((1U << in_b) | (1U << out_b));
    *out_allowed = true;
    return MESHX_SUCCESS;
}

/**
 * @brief Legacy transit loop-check wrapper without keyed echo digest.
 *
 * @param[in,out] mgr              Pointer to bridge manager.
 * @param[in,out] hdr              Pointer to transit header to inspect and update.
 * @param[in]     current_slot_bit Current subnet slot bit index.
 * @param[in]     now_ms           Current monotonic timestamp in milliseconds.
 * @param[out]    out_allowed      Set to true if packet is allowed to transit, false otherwise.
 * @return MESHX_SUCCESS on success, or MESHX_INVALID_ARG if @p out_allowed is NULL.
 */
meshx_err_t meshx_subnet_bridge_transit_check_and_mark(meshx_subnet_bridge_mgr_t *mgr,
                                                       meshx_subnet_transit_hdr_t *hdr,
                                                       uint8_t current_slot_bit,
                                                       uint32_t now_ms,
                                                       bool *out_allowed)
{
    meshx_bridge_transit_ctx_t tctx = {
        .in_slot_bit      = current_slot_bit,
        .out_slot_bit     = current_slot_bit,
        .egress_subnet_id = MESHX_GLOBAL_SUBNET_INVALID,
        .k_bridge         = NULL,
        .now_ms           = now_ms,
    };
    return meshx_subnet_bridge_transit_check_and_mark_ex(mgr, hdr, &tctx, out_allowed);
}
