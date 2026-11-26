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
 * @param[in] mgr         Pointer to bridge manager.
 * @param[in] boundary_id Unique 32-bit boundary link identifier.
 *
 * @return Pointer to boundary context if found, or NULL.
 */
static meshx_boundary_redundancy_t *find_boundary(meshx_subnet_bridge_mgr_t *mgr, uint32_t boundary_id)
{
    for (size_t i = 0; i < MESHX_MAX_BRIDGING_SLOTS; i++)
    {
        if (mgr->boundaries[i].valid && mgr->boundaries[i].boundary_id == boundary_id)
        {
            return &mgr->boundaries[i];
        }
    }
    return NULL;
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
    return (sender_addr > local_addr) ? MESHX_SUCCESS : MESHX_FAIL;
}

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
                              uint32_t now_ms)
{
    MESHX_UNUSED(now_ms);
    if (!mgr)
    {
        return;
    }
    memset(mgr, 0, sizeof(*mgr));
    mgr->is_mains_powered = mains_powered;
    mgr->rssi_score = rssi_score;
    for (size_t i = 0; i < MESHX_MAX_BRIDGING_SLOTS; i++)
    {
        mgr->boundaries[i].local_addr = local_addr;
    }
}

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
                                           uint16_t bridge_addr)
{
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
    return score;
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
    meshx_boundary_redundancy_t *existing = find_boundary(mgr, boundary_id);
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
    existing->local_score        = meshx_subnet_bridge_compute_score(mgr->is_mains_powered,
                                                                    mgr->rssi_score,
                                                                    MESHX_WITNESS_DEFAULT_HEAP_BYTES,
                                                                    local_addr);
    existing->valid              = true;
    return MESHX_SUCCESS;
}

/**
 * @brief Execute periodic watchdog and heartbeat timing checks.
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
                                     uint32_t *out_claim_boundary_id)
{
    if (!mgr || !out_hb_boundary_id || !out_claim_boundary_id)
    {
        return MESHX_INVALID_ARG;
    }
    *out_hb_boundary_id = 0UL;
    *out_claim_boundary_id = 0UL;

    for (size_t i = 0; i < MESHX_MAX_BRIDGING_SLOTS; i++)
    {
        meshx_boundary_redundancy_t *b = &mgr->boundaries[i];
        if (!b->valid)
        {
            continue;
        }
        b->local_score = meshx_subnet_bridge_compute_score(mgr->is_mains_powered,
                                                           mgr->rssi_score,
                                                           free_heap_bytes,
                                                           b->local_addr);

        if (b->role == MESHX_BRIDGE_ROLE_ACTIVE)
        {
            if ((now_ms - b->last_hb_tx_ms) >= MESHX_BRIDGE_HEARTBEAT_MS)
            {
                b->last_hb_tx_ms = now_ms;
                if (*out_hb_boundary_id == 0UL)
                {
                    *out_hb_boundary_id = b->boundary_id;
                }
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
                if (*out_claim_boundary_id == 0UL)
                {
                    *out_claim_boundary_id = b->boundary_id;
                }
            }
        }
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Process an inbound UVP Heartbeat message and execute split-brain fencing.
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
    if (!mgr)
    {
        return MESHX_INVALID_ARG;
    }
    meshx_boundary_redundancy_t *b = find_boundary(mgr, boundary_id);
    if (!b)
    {
        return MESHX_NOT_FOUND;
    }

    if (rx_term < b->active_term)
    {
        return MESHX_SUCCESS;
    }

    if (b->role == MESHX_BRIDGE_ROLE_ACTIVE && sender_addr != b->local_addr)
    {
        if (peer_wins_arbitration(b->active_term, b->local_score, b->local_addr,
                                  rx_term, sender_score, sender_addr) == MESHX_SUCCESS)
        {
            b->role = MESHX_BRIDGE_ROLE_STANDBY;
            b->active_term = rx_term;
            b->active_bridge_addr = sender_addr;
            b->active_score = sender_score;
            b->last_hb_rx_ms = now_ms;
            b->claim_pending = false;
            if (queue_to_purge)
            {
                (void)meshx_router_queue_purge_boundary(queue_to_purge, boundary_id);
            }
        }
        else
        {
            if (out_send_demote)
            {
                *out_send_demote = true;
            }
            if (out_demote_target)
            {
                *out_demote_target = sender_addr;
            }
        }
        return MESHX_SUCCESS;
    }

    b->active_term = rx_term;
    b->active_bridge_addr = sender_addr;
    b->active_score = sender_score;
    b->last_hb_rx_ms = now_ms;
    b->claim_pending = false;
    return MESHX_SUCCESS;
}

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
                                                  bool *out_send_demote)
{
    uint16_t dummy_target = 0U;
    return meshx_subnet_bridge_on_heartbeat(mgr,
                                            boundary_id,
                                            rx_term,
                                            candidate_addr,
                                            candidate_score,
                                            now_ms,
                                            queue_to_purge,
                                            out_send_demote,
                                            &dummy_target);
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
    meshx_boundary_redundancy_t *b = find_boundary(mgr, boundary_id);
    if (!b)
    {
        return MESHX_NOT_FOUND;
    }
    if (winner_addr == b->local_addr)
    {
        return MESHX_SUCCESS;
    }

    if (winning_term > b->active_term ||
        (winning_term == b->active_term && winner_score >= b->local_score))
    {
        b->role = MESHX_BRIDGE_ROLE_STANDBY;
        b->active_term = winning_term;
        b->active_bridge_addr = winner_addr;
        b->active_score = winner_score;
        b->last_hb_rx_ms = now_ms;
        b->claim_pending = false;
        if (queue_to_purge)
        {
            (void)meshx_router_queue_purge_boundary(queue_to_purge, boundary_id);
        }
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
    uint8_t n = (adv->entry_count > MESHX_ROUTE_ADV_MAX_ENTRIES_PER_MSG) ?
                 MESHX_ROUTE_ADV_MAX_ENTRIES_PER_MSG : adv->entry_count;

    for (uint8_t k = 0U; k < n; k++)
    {
        const meshx_route_adv_dest_t *src_e = &adv->entries[k];
        uint8_t new_hops = (uint8_t)(src_e->hop_count + 1U);
        if (src_e->dest_subnet_id == MESHX_GLOBAL_SUBNET_INVALID ||
            new_hops > MESHX_TRANSIT_DEFAULT_HOP_LIMIT)
        {
            continue;
        }

        int match_idx = -1;
        int free_idx = -1;
        int worst_idx = 0;
        for (int i = 0; i < (int)MESHX_ROUTE_ADV_TABLE_SIZE; i++)
        {
            meshx_transit_route_entry_t *r = &mgr->routes[i];
            if (r->valid)
            {
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
            else if (free_idx < 0)
            {
                free_idx = i;
            }
        }

        int idx = (match_idx >= 0) ? match_idx : ((free_idx >= 0) ? free_idx : worst_idx);
        meshx_transit_route_entry_t *dst = &mgr->routes[idx];
        bool replace = (match_idx < 0) ||
                       (dst->next_hop_subnet_id == source_subnet_id) ||
                       (new_hops < dst->hop_count) ||
                       (new_hops == dst->hop_count && src_e->route_metric < dst->metric);
        if (replace)
        {
            dst->dest_subnet_id = src_e->dest_subnet_id;
            dst->next_hop_subnet_id = source_subnet_id;
            dst->next_hop_bridge_addr = adv->via_bridge_addr;
            dst->hop_count = new_hops;
            dst->metric = src_e->route_metric;
            dst->last_updated_ms = now_ms;
            dst->valid = true;
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
 * @brief Validate transit packet loop prevention rules and update echo suppression filter.
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
                                                uint32_t now_ms)
{
    if (!mgr || !hdr || hdr->magic_ver != MESHX_TRANSIT_MAGIC_VER ||
        hdr->hop_limit == 0U || hdr->payload_len > MESHX_MAX_MULTIHOP_APP_PDU_LEN)
    {
        return false;
    }

    if (current_slot_bit < MESHX_TRANSIT_MAX_SLOT_BITS &&
        (hdr->visited_subnets_mask & (1UL << current_slot_bit)) != 0UL)
    {
        return false;
    }

    uint16_t now_ds = (uint16_t)((now_ms / MESHX_TRANSIT_ECHO_DECIS_DIVISOR) & 0xFFFFUL);
    uint16_t ttl_ds = (uint16_t)(MESHX_TRANSIT_ECHO_TTL_MS / MESHX_TRANSIT_ECHO_DECIS_DIVISOR);

    for (size_t i = 0; i < MESHX_TRANSIT_ECHO_FILTER_SIZE; i++)
    {
        const meshx_echo_filter_entry_t *e = &mgr->echo_filter[i];
        if (e->orig_src_addr == hdr->orig_src_addr &&
            e->orig_dst_addr == hdr->orig_dst_addr &&
            e->tx_tag_crc16 == hdr->tx_tag_crc16)
        {
            uint16_t elapsed_ds = (uint16_t)(now_ds - e->timestamp_ds);
            if (elapsed_ds <= ttl_ds)
            {
                return false;
            }
        }
    }

    meshx_echo_filter_entry_t *slot = &mgr->echo_filter[mgr->echo_head];
    slot->orig_src_addr = hdr->orig_src_addr;
    slot->orig_dst_addr = hdr->orig_dst_addr;
    slot->tx_tag_crc16 = hdr->tx_tag_crc16;
    slot->timestamp_ds = now_ds;
    mgr->echo_head = (uint8_t)((mgr->echo_head + 1U) % MESHX_TRANSIT_ECHO_FILTER_SIZE);

    hdr->hop_limit--;
    if (current_slot_bit < MESHX_TRANSIT_MAX_SLOT_BITS)
    {
        hdr->visited_subnets_mask |= (1UL << current_slot_bit);
    }
    return true;
}
