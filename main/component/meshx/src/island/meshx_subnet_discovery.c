/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_subnet_discovery.c
 * @brief Neighbor Discovery, Boundary Witness Election & Cross-Subnet Pairing Engine.
 */

#include <string.h>
#include "island/meshx_subnet_discovery.h"

/**
 * @brief Initialize neighbor discovery and boundary witness election context.
 *
 * @param[out] disc Pointer to discovery manager context to initialize.
 */
void meshx_subnet_disc_init(meshx_subnet_discovery_t *disc)
{
    if (!disc)
    {
        return;
    }
    memset(disc, 0, sizeof(*disc));
}

/**
 * @brief Deinitialize neighbor discovery context and clear all state.
 *
 * @param[in,out] disc Pointer to discovery manager context to deinitialize.
 */
void meshx_subnet_disc_deinit(meshx_subnet_discovery_t *disc)
{
    if (!disc)
    {
        return;
    }
    memset(disc, 0, sizeof(*disc));
}

/**
 * @brief Record an RX MIC failure and evaluate the sliding window threshold.
 *
 * @param[in,out] disc   Pointer to discovery context.
 * @param[in]     now_ms Current timestamp in milliseconds.
 *
 * @return true if boundary witness election threshold is met, false otherwise.
 */
bool meshx_subnet_disc_record_mic_fail(meshx_subnet_discovery_t *disc, uint32_t now_ms)
{
    if (!disc)
    {
        return false;
    }

    disc->mic_fail_timestamps[disc->mic_fail_head] = now_ms;
    disc->mic_fail_head = (uint8_t)((disc->mic_fail_head + 1U) % MESHX_MIC_FAIL_THRESHOLD);
    if (disc->mic_fail_count < MESHX_MIC_FAIL_THRESHOLD)
    {
        disc->mic_fail_count++;
    }

    if (disc->mic_fail_count >= MESHX_MIC_FAIL_THRESHOLD)
    {
        uint32_t oldest_ts = disc->mic_fail_timestamps[disc->mic_fail_head];
        if ((now_ms - oldest_ts) <= MESHX_MIC_FAIL_WINDOW_MS)
        {
            disc->witness_eligible = true;
            return true;
        }
    }
    return false;
}

/**
 * @brief Reset recorded MIC failure history and witness eligibility.
 *
 * @param[in,out] disc Pointer to discovery context.
 */
void meshx_subnet_disc_reset_mic_fails(meshx_subnet_discovery_t *disc)
{
    if (!disc)
    {
        return;
    }
    memset(disc->mic_fail_timestamps, 0, sizeof(disc->mic_fail_timestamps));
    disc->mic_fail_head = 0U;
    disc->mic_fail_count = 0U;
    disc->witness_eligible = false;
}

/**
 * @brief Evaluate witness candidate criteria for boundary node election.
 *
 * @param[in] disc            Pointer to discovery context (optional).
 * @param[in] rssi_dbm        Signal strength in dBm.
 * @param[in] mains_powered   True if mains powered, false if battery powered.
 * @param[in] free_heap_bytes Free heap memory in bytes.
 *
 * @return true if candidate qualifies as a boundary witness, false otherwise.
 */
bool meshx_subnet_disc_eval_candidate(const meshx_subnet_discovery_t *disc,
                                      int8_t rssi_dbm,
                                      bool mains_powered,
                                      uint32_t free_heap_bytes)
{
    MESHX_UNUSED(disc);
    if (!mains_powered)
    {
        return false;
    }
    if (rssi_dbm < MESHX_WITNESS_MIN_RSSI_DBM)
    {
        return false;
    }
    if (free_heap_bytes <= MESHX_WITNESS_MIN_HEAP_BYTES)
    {
        return false;
    }
    return true;
}

/**
 * @brief Process UVP Discovery Response and allocate ephemeral or mapped virtual NAT address.
 *
 * @param[in,out] disc                 Pointer to discovery context.
 * @param[in,out] nat_mgr              Pointer to NAT manager.
 * @param[in]     rsp                  Pointer to parsed discovery response packet.
 * @param[in]     now_min              Current time in minutes for NAT aging.
 * @param[in]     now_ms               Current timestamp in milliseconds.
 * @param[out]    out_virtual_nat_addr Pointer to store assigned virtual address.
 *
 * @return MESHX_SUCCESS on success, or appropriate error code.
 */
meshx_err_t meshx_subnet_disc_handle_rsp(meshx_subnet_discovery_t *disc,
                                         meshx_nat_mgr_t *nat_mgr,
                                         const meshx_uvp_discover_rsp_t *rsp,
                                         uint16_t now_min,
                                         uint32_t now_ms,
                                         uint16_t *out_virtual_nat_addr)
{
    if (!disc || !nat_mgr || !rsp || !out_virtual_nat_addr)
    {
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
    if (err != MESHX_SUCCESS)
    {
        return err;
    }

    int match_idx = -1;
    int free_idx = -1;
    int oldest_idx = 0;

    for (int i = 0; i < (int)MESHX_DISCOVERY_TX_PENDING_MAX; i++)
    {
        meshx_disc_correlation_t *c = &disc->corr[i];
        if (c->valid)
        {
            if (c->global_subnet_src == rsp->global_subnet_src &&
                c->requestor_addr == rsp->requestor_addr &&
                c->trans_id == rsp->trans_id &&
                c->remote_phys_addr == rsp->remote_phys_addr)
            {
                match_idx = i;
                break;
            }
            if (c->timestamp_ms < disc->corr[oldest_idx].timestamp_ms)
            {
                oldest_idx = i;
            }
        }
        else if (free_idx < 0)
        {
            free_idx = i;
        }
    }

    int idx = (match_idx >= 0) ? match_idx : ((free_idx >= 0) ? free_idx : oldest_idx);
    disc->corr[idx].valid                   = true;
    disc->corr[idx].timestamp_ms            = now_ms;
    disc->corr[idx].assigned_virtual_addr   = virtual_addr;
    disc->corr[idx].trans_id                = rsp->trans_id;
    disc->corr[idx].requestor_addr          = rsp->requestor_addr;
    disc->corr[idx].remote_phys_addr        = rsp->remote_phys_addr;
    disc->corr[idx].global_subnet_src       = rsp->global_subnet_src;

    *out_virtual_nat_addr = virtual_addr;
    return MESHX_SUCCESS;
}

/**
 * @brief Commit persistent cross-subnet pair record and promote NAT mapping.
 *
 * @param[in,out] disc    Pointer to discovery context.
 * @param[in,out] nat_mgr Pointer to NAT manager (optional, may be NULL).
 * @param[in]     rec     Pointer to pair record to commit.
 *
 * @return MESHX_SUCCESS on success, MESHX_NO_MEM if table full, or MESHX_INVALID_ARG.
 */
meshx_err_t meshx_subnet_disc_commit_pair(meshx_subnet_discovery_t *disc,
                                          meshx_nat_mgr_t *nat_mgr,
                                          const meshx_pair_record_t *rec)
{
    if (!disc || !rec)
    {
        return MESHX_INVALID_ARG;
    }

    int match_idx = -1;
    int free_idx = -1;
    for (int i = 0; i < (int)MESHX_PAIR_RECORD_MAX; i++)
    {
        meshx_pair_record_t *p = &disc->pairs[i];
        if (p->active != 0U)
        {
            if (p->local_elem_addr == rec->local_elem_addr &&
                p->virtual_remote_addr == rec->virtual_remote_addr)
            {
                match_idx = i;
                break;
            }
        }
        else if (free_idx < 0)
        {
            free_idx = i;
        }
    }

    int idx = (match_idx >= 0) ? match_idx : free_idx;
    if (idx < 0)
    {
        return MESHX_NO_MEM;
    }

    bool was_inactive = (disc->pairs[idx].active == 0U);
    memcpy(&disc->pairs[idx], rec, sizeof(*rec));
    disc->pairs[idx].active = 1U;
    if (was_inactive)
    {
        disc->pair_count++;
    }

    if (nat_mgr && rec->remote_phys_addr >= MESHX_NAT_ADDR_LIMIT_TIER1)
    {
        uint16_t v = 0U;
        meshx_err_t err = meshx_nat_tier2_alloc(nat_mgr,
                                                NULL,
                                                rec->global_subnet_src,
                                                rec->remote_phys_addr,
                                                MESHX_NAT_FLAG_PERSISTENT,
                                                0U,
                                                &v);
        if (err != MESHX_SUCCESS)
        {
            return err;
        }
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Unpair and remove persistent cross-subnet record.
 *
 * @param[in,out] disc                 Pointer to discovery context.
 * @param[in]     local_elem_addr      Local element unicast address.
 * @param[in]     virtual_remote_addr  Virtual remote address being removed.
 *
 * @return MESHX_SUCCESS on success, or MESHX_NOT_FOUND if record was not found.
 */
meshx_err_t meshx_subnet_disc_remove_pair(meshx_subnet_discovery_t *disc,
                                          uint16_t local_elem_addr,
                                          uint16_t virtual_remote_addr)
{
    if (!disc)
    {
        return MESHX_INVALID_ARG;
    }
    for (size_t i = 0; i < MESHX_PAIR_RECORD_MAX; i++)
    {
        if (disc->pairs[i].active != 0U &&
            disc->pairs[i].local_elem_addr == local_elem_addr &&
            disc->pairs[i].virtual_remote_addr == virtual_remote_addr)
        {
            memset(&disc->pairs[i], 0, sizeof(disc->pairs[i]));
            if (disc->pair_count > 0U)
            {
                disc->pair_count--;
            }
            return MESHX_SUCCESS;
        }
    }
    return MESHX_NOT_FOUND;
}
