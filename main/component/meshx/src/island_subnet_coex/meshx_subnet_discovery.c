/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_subnet_discovery.c
 * @brief Neighbor Discovery, Boundary Witness Election & Cross-Subnet Pairing Engine.
 */

#include <string.h>
#include "island_subnet_coex/meshx_subnet_discovery.h"

void meshx_subnet_disc_init(meshx_subnet_discovery_t *disc)
{
    if (!disc) {
        return;
    }
    memset(disc, 0, sizeof(*disc));
}

void meshx_subnet_disc_deinit(meshx_subnet_discovery_t *disc)
{
    if (!disc) {
        return;
    }
    memset(disc, 0, sizeof(*disc));
}

bool meshx_subnet_disc_record_mic_fail(meshx_subnet_discovery_t *disc, uint32_t now_ms)
{
    if (!disc) {
        return false;
    }

    disc->mic_fail_timestamps[disc->mic_fail_head] = now_ms;
    disc->mic_fail_head = (uint8_t)((disc->mic_fail_head + 1U) % MESHX_MIC_FAIL_THRESHOLD);
    if (disc->mic_fail_count < MESHX_MIC_FAIL_THRESHOLD) {
        disc->mic_fail_count++;
    }

    if (disc->mic_fail_count >= MESHX_MIC_FAIL_THRESHOLD) {
        uint32_t oldest_ts = disc->mic_fail_timestamps[disc->mic_fail_head];
        if ((now_ms - oldest_ts) <= MESHX_MIC_FAIL_WINDOW_MS) {
            disc->witness_eligible = true;
            return true;
        }
    }
    return false;
}

void meshx_subnet_disc_reset_mic_fails(meshx_subnet_discovery_t *disc)
{
    if (!disc) {
        return;
    }
    memset(disc->mic_fail_timestamps, 0, sizeof(disc->mic_fail_timestamps));
    disc->mic_fail_head    = 0U;
    disc->mic_fail_count   = 0U;
    disc->witness_eligible = false;
}

bool meshx_subnet_disc_eval_candidate(const meshx_subnet_discovery_t *disc,
                                      int8_t rssi_dbm,
                                      bool mains_powered,
                                      uint32_t free_heap_bytes)
{
    (void)disc;
    if (!mains_powered) {
        return false;
    }
    if (rssi_dbm < MESHX_WITNESS_MIN_RSSI_DBM) {
        return false;
    }
    if (free_heap_bytes <= MESHX_WITNESS_MIN_HEAP_BYTES) {
        return false;
    }
    return true;
}

meshx_err_t meshx_subnet_disc_handle_rsp(meshx_subnet_discovery_t *disc,
                                         meshx_nat_mgr_t *nat_mgr,
                                         const meshx_uvp_discover_rsp_t *rsp,
                                         uint16_t now_min,
                                         uint32_t now_ms,
                                         uint16_t *out_virtual_nat_addr)
{
    if (!disc || !nat_mgr || !rsp || !out_virtual_nat_addr) {
        return MESHX_INVALID_ARG;
    }

    uint16_t virtual_addr = 0U;
    meshx_err_t err = meshx_nat_translate_inbound(nat_mgr,
                                                  rsp->global_subnet_src,
                                                  rsp->remote_phys_addr,
                                                  NULL,
                                                  MESHX_NAT_FLAG_TEMP_DISCOVERY,
                                                  now_min,
                                                  &virtual_addr);
    if (err != MESHX_SUCCESS) {
        return err;
    }

    int match_idx  = -1;
    int free_idx   = -1;
    int oldest_idx = 0;

    for (int i = 0; i < (int)MESHX_DISCOVERY_TX_PENDING_MAX; i++) {
        meshx_disc_correlation_t *c = &disc->corr[i];
        if (c->valid) {
            if (c->global_subnet_src == rsp->global_subnet_src &&
                c->requestor_addr == rsp->requestor_addr &&
                c->trans_id == rsp->trans_id &&
                c->remote_phys_addr == rsp->remote_phys_addr) {
                match_idx = i;
                break;
            }
            if (c->timestamp_ms < disc->corr[oldest_idx].timestamp_ms) {
                oldest_idx = i;
            }
        } else if (free_idx < 0) {
            free_idx = i;
        }
    }

    int idx = (match_idx >= 0) ? match_idx : ((free_idx >= 0) ? free_idx : oldest_idx);
    disc->corr[idx].global_subnet_src     = rsp->global_subnet_src;
    disc->corr[idx].requestor_addr        = rsp->requestor_addr;
    disc->corr[idx].trans_id              = rsp->trans_id;
    disc->corr[idx].remote_phys_addr      = rsp->remote_phys_addr;
    disc->corr[idx].assigned_virtual_addr = virtual_addr;
    disc->corr[idx].timestamp_ms          = now_ms;
    disc->corr[idx].valid                 = true;

    *out_virtual_nat_addr = virtual_addr;
    return MESHX_SUCCESS;
}

meshx_err_t meshx_subnet_disc_commit_pair(meshx_subnet_discovery_t *disc,
                                          meshx_nat_mgr_t *nat_mgr,
                                          const meshx_pair_record_t *rec)
{
    if (!disc || !rec) {
        return MESHX_INVALID_ARG;
    }

    int match_idx = -1;
    int free_idx  = -1;
    for (int i = 0; i < (int)MESHX_PAIR_RECORD_MAX; i++) {
        meshx_pair_record_t *p = &disc->pairs[i];
        if (p->active != 0U) {
            if (p->local_elem_addr == rec->local_elem_addr &&
                p->virtual_remote_addr == rec->virtual_remote_addr) {
                match_idx = i;
                break;
            }
        } else if (free_idx < 0) {
            free_idx = i;
        }
    }

    int idx = (match_idx >= 0) ? match_idx : free_idx;
    if (idx < 0) {
        return MESHX_NO_MEM;
    }

    bool was_inactive = (disc->pairs[idx].active == 0U);
    memcpy(&disc->pairs[idx], rec, sizeof(*rec));
    disc->pairs[idx].active = 1U;
    if (was_inactive) {
        disc->pair_count++;
    }

    if (nat_mgr && rec->remote_phys_addr >= 0x0400U) {
        uint16_t v = 0U;
        (void)meshx_nat_tier2_alloc(nat_mgr,
                                    NULL,
                                    rec->global_subnet_src,
                                    rec->remote_phys_addr,
                                    MESHX_NAT_FLAG_PERSISTENT,
                                    0U,
                                    &v);
    }
    return MESHX_SUCCESS;
}

meshx_err_t meshx_subnet_disc_remove_pair(meshx_subnet_discovery_t *disc,
                                          uint16_t local_elem_addr,
                                          uint16_t virtual_remote_addr)
{
    if (!disc) {
        return MESHX_INVALID_ARG;
    }
    for (size_t i = 0; i < MESHX_PAIR_RECORD_MAX; i++) {
        if (disc->pairs[i].active != 0U &&
            disc->pairs[i].local_elem_addr == local_elem_addr &&
            disc->pairs[i].virtual_remote_addr == virtual_remote_addr) {
            memset(&disc->pairs[i], 0, sizeof(disc->pairs[i]));
            if (disc->pair_count > 0U) {
                disc->pair_count--;
            }
            return MESHX_SUCCESS;
        }
    }
    return MESHX_NOT_FOUND;
}

