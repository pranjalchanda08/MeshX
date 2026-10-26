/**
 * @file meshx_uvp_redundancy_handlers.cpp
 * @brief UVP Cascading Redundancy and Fast Infill Provisioning Opcode Handlers (0xC8..0xCD, 0xD0).
 *
 * Implements redundancy synchronization heartbeats, infill lease allocation & responses,
 * fast prov progress forwarding, and candidate election claim handshakes.
 *
 * @author Pranjal Chanda
 * @date 2024-2026
 */

#include "meshx_uvp_registry.hpp"
#include <meshx_composition.hpp>
#include <meshx_c_header.h>

/* =========================================================================
 * Redundancy & Infill State Variables
 * ========================================================================= */

#if CONFIG_ENABLE_UVP_PROVISIONER
static meshx_os_timer_t *s_redundancy_wd_timer     = NULL;
static uint8_t           s_redundancy_missed_beats = 0;
static meshx_prov_role_t s_local_prov_role         = MESHX_PROV_ROLE_STANDALONE;
#endif /* CONFIG_ENABLE_UVP_PROVISIONER */
static uint8_t           s_claim_attempt        = 0;
static uint32_t          s_claim_epoch          = 0;
static uint16_t          s_claim_next_free_addr = 0;
static meshx_os_timer_t *s_claim_retry_timer    = NULL;
static uint16_t          s_claim_master_addr    = MESHX_UVP_MASTER_ADDR;
static void uvp_redundancy_claim_retry_cb(const meshx_os_timer_t *p_timer);

#if CONFIG_ENABLE_UVP_PROVISIONER
/**
 * @brief Watchdog timer callback monitoring Master heartbeats on Standby provisioner.
 *
 * Promotes Standby provisioner to Master if consecutive heartbeats are missed.
 *
 * @param[in] p_timer Pointer to the OS timer structure.
 */
static void uvp_redundancy_wd_timer_cb(const meshx_os_timer_t *p_timer)
{
    MESHX_UNUSED(p_timer);
    s_redundancy_missed_beats++;
    MESHX_LOGD(MODULE_ID_COMMON, "UVP Redundancy WD: missed %u heartbeats (threshold=%u)",
               s_redundancy_missed_beats, MESHX_REDUNDANCY_MAX_MISSED_BEATS);
    if (s_redundancy_missed_beats >= MESHX_REDUNDANCY_MAX_MISSED_BEATS && s_local_prov_role == MESHX_PROV_ROLE_STANDBY)
    {
        /* 15 seconds without heartbeat -> Promote Standby to active Master */
        s_local_prov_role = MESHX_PROV_ROLE_MASTER;
        MESHX_LOGW(MODULE_ID_COMMON, "UVP Redundancy: 3 heartbeats missed (15s). Promoting STANDBY -> MASTER!");

        mxcp_evt_uvp_redundancy_status_t evt;
        memset(&evt, 0, sizeof(evt));
        evt.current_role = MESHX_PROV_ROLE_MASTER;
        evt.peer_alive = 0;
        uint16_t p_start = 0, p_end = 0;
        meshx_prov_get_address_pool(MESHX_PROV_ROLE_MASTER, &p_start, &p_end);
        evt.next_free_addr = p_start;
        evt.sync_epoch = 1;
        mxcp_send_event(MXCP_EVT_UVP_REDUNDANCY_STATUS, reinterpret_cast<const uint8_t *>(&evt), sizeof(evt));

        /* TASK-HEAL-007: Auto-start 0xCD redundancy claim with exponential backoff */
        meshx_redundancy_claim_start(evt.sync_epoch, evt.next_free_addr);
    }
}
#endif /* CONFIG_ENABLE_UVP_PROVISIONER */

/* =========================================================================
 * Redundancy & Infill Handler Implementations
 * ========================================================================= */

#if CONFIG_ENABLE_UVP_PROVISIONER
/**
 * @brief Handle redundancy synchronization heartbeat (MESHX_UVP_OP_REDUNDANCY_SYNC).
 *
 * Master <-> Standby heartbeat token reception that resets the 15-second failover watchdog.
 *
 * @param[in] p_meta      Pointer to UVP message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to payload bytes.
 * @param[in] payload_len Length of payload bytes in bytes.
 * @return MESHX_SUCCESS on completion.
 */
static meshx_err_t uvp_handle_redundancy_sync(const control_task_uvp_meta_t *p_meta,
                                              const meshx_uvp_ctx_t *p_ctx,
                                              const uint8_t *payload,
                                              uint16_t payload_len)
{
    MESHX_UNUSED(p_meta);
    if (!p_ctx)
    {
        return MESHX_INVALID_ARG;
    }

    uint16_t p_start = 0;
    uint16_t p_end   = 0;

    /* Reset Standby missed beat watchdog counter */
    s_redundancy_missed_beats = 0;

    /* Ensure watchdog timer is running on Standby */
    if (!s_redundancy_wd_timer)
    {
        meshx_os_timer_create("redun_wd", MESHX_REDUNDANCY_WD_INTERVAL_MS, true, uvp_redundancy_wd_timer_cb, &s_redundancy_wd_timer);
        if (s_redundancy_wd_timer)
        {
            meshx_os_timer_start(s_redundancy_wd_timer);
        }
    }

    if (payload && payload_len >= 1)
    {
        uint8_t subtype = payload[0];
        if (subtype == MESHX_REDUND_SYNC_HEARTBEAT_FV && payload_len >= sizeof(meshx_uvp_redund_sync_heartbeat_t))
        {
            const meshx_uvp_redund_sync_heartbeat_t *hb = (const meshx_uvp_redund_sync_heartbeat_t *)payload;
            MESHX_LOGD(MODULE_ID_COMMON, "UVP Redundancy: Heartbeat FV from 0x%04X (epoch=%lu, assigned=%u)",
                       hb->sender_addr, (unsigned long)hb->freshness_vector.sync_epoch, hb->freshness_vector.assigned_count);

            /* Split-Brain collision arbitration using Lexicographical Freshness Vector */
            if (s_local_prov_role == MESHX_PROV_ROLE_MASTER)
            {
                meshx_freshness_vector_t local_fv;
                meshx_freshness_vector_compute(&local_fv);

                bool remote_is_newer = (hb->freshness_vector.sync_epoch > local_fv.sync_epoch) ||
                                       (hb->freshness_vector.sync_epoch == local_fv.sync_epoch &&
                                        hb->freshness_vector.assigned_count > local_fv.assigned_count) ||
                                       (hb->freshness_vector.sync_epoch == local_fv.sync_epoch &&
                                        hb->freshness_vector.assigned_count == local_fv.assigned_count &&
                                        hb->sender_addr < meshx_get_node_addr());

                if (remote_is_newer)
                {
                    s_local_prov_role = MESHX_PROV_ROLE_STANDBY;
                    MESHX_LOGW(MODULE_ID_COMMON, "UVP Redundancy: Yielding to superior Master 0x%04X; demoting to STANDBY",
                               hb->sender_addr);

                    mxcp_evt_uvp_redundancy_status_t evt;
                    memset(&evt, 0, sizeof(evt));
                    meshx_prov_get_address_pool(MESHX_PROV_ROLE_STANDBY, &p_start, &p_end);
                    evt.sync_epoch = hb->freshness_vector.sync_epoch;
                    evt.peer_alive = 1;
                    evt.next_free_addr = p_start;
                    evt.current_role = MESHX_PROV_ROLE_STANDBY;
                    mxcp_send_event(MXCP_EVT_UVP_REDUNDANCY_STATUS, reinterpret_cast<const uint8_t *>(&evt), sizeof(evt));
                }
            }
        }
        else if (subtype == MESHX_REDUND_SYNC_ADDR_DELTA && payload_len >= sizeof(meshx_uvp_redund_sync_delta_t))
        {
            const meshx_uvp_redund_sync_delta_t *delta = (const meshx_uvp_redund_sync_delta_t *)payload;
            meshx_err_t merge_err = meshx_sparse_addr_merge_chunk(delta->chunk_idx, delta->chunk_data);
            MESHX_LOGI(MODULE_ID_COMMON, "UVP Redundancy: Delta merge chunk %u completed with status %d",
                       delta->chunk_idx, merge_err);
        }
    }

    MESHX_LOGD(MODULE_ID_COMMON, "UVP Redundancy: Heartbeat sync token from 0x%04X (len=%u)",
               p_ctx->src_addr, (unsigned int)payload_len);
    return MESHX_SUCCESS;
}
#endif /* CONFIG_ENABLE_UVP_PROVISIONER */

/**
 * @brief Handle infill reserve lease request (MESHX_UVP_OP_INFILL_LEASE_REQ 0xC8).
 *
 * Saturated Frontier Proxy requests a contiguous address block from the Provisioner.
 * Allocates addresses via meshx_lease_allocate_block and responds with 0xCC.
 *
 * @param[in] p_meta      Pointer to UVP message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to payload bytes containing meshx_uvp_infill_lease_req_t.
 * @param[in] payload_len Length of payload bytes in bytes.
 * @return MESHX_SUCCESS on completion.
 */
static meshx_err_t uvp_handle_infill_lease_req(const control_task_uvp_meta_t *p_meta,
                                               const meshx_uvp_ctx_t *p_ctx,
                                               const uint8_t *payload,
                                               uint16_t payload_len)
{
    MESHX_UNUSED(p_meta);
    if (!p_ctx || !payload || payload_len < sizeof(meshx_uvp_infill_lease_req_t))
    {
        MESHX_LOGE(MODULE_ID_COMMON, "UVP Infill: Invalid lease request payload (%u bytes)", (unsigned int)payload_len);
        return MESHX_INVALID_ARG;
    }

    const meshx_uvp_infill_lease_req_t *req = (const meshx_uvp_infill_lease_req_t *)payload;
    MESHX_LOGI(MODULE_ID_COMMON, "UVP Infill: Lease request for %u addrs from proxy 0x%04X",
               req->requested_count, p_ctx->src_addr);

    uint16_t base_addr = 0;
    uint32_t lease_id = 0;
    meshx_err_t err = meshx_lease_allocate_block(req->requested_count,
                                                 p_ctx->src_addr,
                                                 &req->target_uuid,
                                                 &base_addr,
                                                 &lease_id);

    uint8_t rsp_buf[1 + sizeof(meshx_uvp_infill_lease_rsp_t)];
    rsp_buf[0] = MESHX_UVP_OP_INFILL_LEASE_RSP;
    meshx_uvp_infill_lease_rsp_t *rsp = (meshx_uvp_infill_lease_rsp_t *)&rsp_buf[1];
    memset(rsp, 0, sizeof(*rsp));
    rsp->lease_id = (err == MESHX_SUCCESS) ? lease_id : 0;
    rsp->allocated_base = (err == MESHX_SUCCESS) ? base_addr : 0;
    rsp->allocated_count = (err == MESHX_SUCCESS) ? req->requested_count : 0;
    memcpy(&rsp->target_uuid, &req->target_uuid, sizeof(meshx_uuid_t));

    MESHX_LOGD(MODULE_ID_COMMON, "UVP Infill: Dispatching lease response (base=0x%04X, count=%u, lease_id=%lu)",
               rsp->allocated_base, rsp->allocated_count, (unsigned long)rsp->lease_id);

    return meshx_comp_send_uvp(0,
                               p_ctx->src_addr,
                               MESHX_FUNC_ID_SYS_PAIRING,
                               rsp_buf,
                               (uint16_t)sizeof(rsp_buf),
                               false);
}

/**
 * @brief Handle infill reserve lease response (MESHX_UVP_OP_INFILL_LEASE_RSP 0xCC).
 *
 * Receives the leased address granted by the Master from the 10% Reserve Band.
 *
 * @param[in] p_meta      Pointer to UVP message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to payload bytes containing meshx_uvp_infill_lease_rsp_t.
 * @param[in] payload_len Length of payload bytes in bytes.
 * @return MESHX_SUCCESS on completion.
 */
static meshx_err_t uvp_handle_infill_lease_rsp(const control_task_uvp_meta_t *p_meta,
                                               const meshx_uvp_ctx_t *p_ctx,
                                               const uint8_t *payload,
                                               uint16_t payload_len)
{
    MESHX_UNUSED(p_meta);
    if (!p_ctx || !payload || payload_len < sizeof(meshx_uvp_infill_lease_rsp_t))
    {
        MESHX_LOGE(MODULE_ID_COMMON, "UVP Infill: Invalid lease response payload (%u bytes)", (unsigned int)payload_len);
        return MESHX_INVALID_ARG;
    }

    const meshx_uvp_infill_lease_rsp_t *rsp = (const meshx_uvp_infill_lease_rsp_t *)payload;
    MESHX_LOGI(MODULE_ID_COMMON, "UVP Infill: Lease response from Master 0x%04X (base=0x%04X, count=%u, lease_id=%lu)",
               p_ctx->src_addr, rsp->allocated_base, rsp->allocated_count, (unsigned long)rsp->lease_id);
    return MESHX_SUCCESS;
}

#if CONFIG_ENABLE_UVP_PROVISIONER
/**
 * @brief Handle fast provisioning bulk progress report (MESHX_UVP_OP_FAST_PROV_REPORT).
 *
 * Forwards provisioning status and address assignment updates from intermediate proxy
 * to Host MCU via MXCP event MXCP_EVT_UVP_FAST_PROV_PROGRESS, and commits the lease.
 *
 * @param[in] p_meta      Pointer to UVP message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to payload bytes containing meshx_uvp_fast_prov_report_t.
 * @param[in] payload_len Length of payload bytes in bytes.
 * @return MESHX_SUCCESS on completion.
 */
static meshx_err_t uvp_handle_fast_prov_report(const control_task_uvp_meta_t *p_meta,
                                               const meshx_uvp_ctx_t *p_ctx,
                                               const uint8_t *payload,
                                               uint16_t payload_len)
{
    MESHX_UNUSED(p_meta);
    MESHX_UNUSED(p_ctx);
    if (!payload || payload_len < sizeof(meshx_uvp_fast_prov_report_t))
    {
        MESHX_LOGE(MODULE_ID_COMMON, "UVP Infill: Invalid fast prov report payload (%u bytes)", (unsigned int)payload_len);
        return MESHX_INVALID_ARG;
    }

    const meshx_uvp_fast_prov_report_t *rep = (const meshx_uvp_fast_prov_report_t *)payload;
    MESHX_LOGI(MODULE_ID_COMMON, "UVP Infill: Fast prov report received (lease_id=%lu, addr=0x%04X, count=%u)",
               (unsigned long)rep->lease_id, rep->assigned_addr, rep->element_count);

    if (rep->lease_id != 0)
    {
        meshx_err_t commit_err = meshx_lease_commit(rep->lease_id);
        MESHX_LOGD(MODULE_ID_COMMON, "UVP Infill: Lease %lu committed with status %d",
                   (unsigned long)rep->lease_id, commit_err);
    }

    /* Proxy -> Master: Bulk progress report forwarded to Host MCU via MXCP */
    mxcp_send_event(MXCP_EVT_UVP_FAST_PROV_PROGRESS, payload, static_cast<uint8_t>(payload_len));
    return MESHX_SUCCESS;
}
#endif /* CONFIG_ENABLE_UVP_PROVISIONER */

/**
 * @brief Handle 0xCD Redundancy Claim broadcast (Candidate side).
 *
 * Received by candidate nodes when a promoted Master initiates cascading redundancy.
 * If the local node is a candidate provisioner, saves the Master address and pool B
 * boundaries, sends a 0xD0 Redundancy Claim ACK, and raises MXCP_EVT_UVP_CLAIM_STATUS.
 *
 * @param[in] p_meta      Pointer to UVP message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to received meshx_uvp_redundancy_claim_t payload.
 * @param[in] payload_len Length of payload in bytes.
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG on payload size mismatch.
 */
static meshx_err_t uvp_handle_redundancy_claim(const control_task_uvp_meta_t *p_meta,
                                               const meshx_uvp_ctx_t *p_ctx,
                                               const uint8_t *payload,
                                               uint16_t payload_len)
{
    MESHX_UNUSED(p_meta);
    if (!p_ctx || !payload || payload_len < sizeof(meshx_uvp_redundancy_claim_t))
        return MESHX_INVALID_ARG;

    const meshx_uvp_redundancy_claim_t *claim = (const meshx_uvp_redundancy_claim_t *)payload;

    MESHX_LOGI(MODULE_ID_COMMON, "UVP Redundancy: 0xCD claim received from 0x%04X (epoch=%lu, next_free_addr=0x%04X)",
               p_ctx->src_addr, (unsigned long)claim->claim_epoch, claim->next_free_addr);

    /* If this node is a ROLE_CANDIDATE, respond with 0xD0 ACK and assume ROLE_STANDBY */
    s_claim_master_addr = p_ctx->src_addr;
    s_claim_epoch = claim->claim_epoch;
    s_claim_next_free_addr = claim->next_free_addr;

    /* Build 0xD0 ACK: [msg_type(1) | UUID(16) | addr(2)] = 19 bytes */
    uint8_t ack_buf[1 + sizeof(meshx_uvp_redundancy_claim_ack_t)];
    ack_buf[0] = MESHX_UVP_OP_REDUNDANCY_CLAIM_ACK;
    meshx_uvp_redundancy_claim_ack_t *ack = (meshx_uvp_redundancy_claim_ack_t *)&ack_buf[1];
    meshx_get_device_uuid(&ack->candidate_uuid);
    ack->candidate_addr = meshx_get_node_addr();

    meshx_err_t err = meshx_comp_send_uvp(0,
                                          s_claim_master_addr,
                                          MESHX_FUNC_ID_SYS_PAIRING,
                                          ack_buf,
                                          (uint16_t)sizeof(ack_buf),
                                          false);
    if (err == MESHX_SUCCESS)
    {
        MESHX_LOGI(MODULE_ID_COMMON, "UVP Redundancy: 0xD0 ACK sent to Master 0x%04X",
                   s_claim_master_addr);
        /* Notify host of claim acceptance */
        mxcp_evt_uvp_claim_status_t evt;
        memset(&evt, 0, sizeof(evt));
        memcpy(evt.candidate_uuid, ack->candidate_uuid, sizeof(evt.candidate_uuid));
        evt.candidate_addr = ack->candidate_addr;
        evt.claim_epoch = s_claim_epoch;
        mxcp_send_event(MXCP_EVT_UVP_CLAIM_STATUS, (const uint8_t *)&evt, sizeof(evt));
    }
    return err;
}

/**
 * @brief Handle 0xD0 Redundancy Claim ACK (Master side).
 *
 * Received by Master when a Candidate node accepts the Standby role.
 * Disarms the exponential backoff claim retry timer and emits MXCP_EVT_UVP_CLAIM_STATUS.
 *
 * @param[in] p_meta      Pointer to UVP message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to received meshx_uvp_redundancy_claim_ack_t payload.
 * @param[in] payload_len Length of payload in bytes.
 * @return MESHX_SUCCESS on success, MESHX_INVALID_ARG on payload size mismatch.
 */
static meshx_err_t uvp_handle_redundancy_claim_ack(const control_task_uvp_meta_t *p_meta,
                                                   const meshx_uvp_ctx_t *p_ctx,
                                                   const uint8_t *payload,
                                                   uint16_t payload_len)
{
    MESHX_UNUSED(p_meta);
    if (!p_ctx || !payload || payload_len < sizeof(meshx_uvp_redundancy_claim_ack_t))
        return MESHX_INVALID_ARG;

    const meshx_uvp_redundancy_claim_ack_t *ack = (const meshx_uvp_redundancy_claim_ack_t *)payload;

    MESHX_LOGI(MODULE_ID_COMMON, "UVP Redundancy: 0xD0 ACK from 0x%04X (UUID %02X%02X...). Claim accepted.",
               p_ctx->src_addr, ack->candidate_uuid[0], ack->candidate_uuid[1]);

    /* Disarm claim retry timer - standby found */
    if (s_claim_retry_timer)
    {
        meshx_os_timer_stop(s_claim_retry_timer);
    }
    s_claim_attempt = 0;

    /* Notify host via MXCP */
    mxcp_evt_uvp_claim_status_t evt;
    memset(&evt, 0, sizeof(evt));
    memcpy(evt.candidate_uuid, ack->candidate_uuid, sizeof(evt.candidate_uuid));
    evt.candidate_addr = ack->candidate_addr;
    evt.claim_epoch = s_claim_epoch;
    mxcp_send_event(MXCP_EVT_UVP_CLAIM_STATUS, (const uint8_t *)&evt, sizeof(evt));

    return MESHX_SUCCESS;
}

/**
 * @brief Timer callback for retrying 0xCD Redundancy Claim with exponential backoff.
 *
 * @param[in] p_timer Pointer to the OS timer structure.
 */
static void uvp_redundancy_claim_retry_cb(const meshx_os_timer_t *p_timer)
{
    MESHX_UNUSED(p_timer);
    MESHX_LOGD(MODULE_ID_COMMON, "UVP Redundancy: Claim retry callback fired (attempt %u)", s_claim_attempt);

    if (s_claim_attempt >= MESHX_REHEAL_CLAIM_MAX_ATTEMPTS)
    {
        MESHX_LOGW(MODULE_ID_COMMON, "UVP Redundancy: Max claim retries reached. Operating as Solo Master.");
        return;
    }

    s_claim_attempt++;
    uint8_t claim_buf[1 + sizeof(meshx_uvp_redundancy_claim_t)];
    claim_buf[0] = MESHX_UVP_OP_REDUNDANCY_CLAIM;
    meshx_uvp_redundancy_claim_t *claim = (meshx_uvp_redundancy_claim_t *)&claim_buf[1];
    claim->claim_epoch = s_claim_epoch;
    claim->master_addr = meshx_get_node_addr();
    claim->next_free_addr = s_claim_next_free_addr;
    claim->reserved = 0;

    meshx_err_t err = meshx_comp_send_uvp(0,
                                          MESHX_MGMT_VIRTUAL_ADDR,
                                          MESHX_FUNC_ID_SYS_PAIRING,
                                          claim_buf,
                                          (uint16_t)sizeof(claim_buf),
                                          false);
    if (err != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "UVP Redundancy: Claim retry %u failed (err=%d)",
                   s_claim_attempt, err);
        return;
    }

    MESHX_LOGW(MODULE_ID_COMMON, "UVP Redundancy: 0xCD claim retry %u (epoch=%lu)",
               s_claim_attempt, (unsigned long)s_claim_epoch);

    /* Exponential backoff: 2s, 4s, 8s */
    uint32_t retry_delay_ms = MESHX_REHEAL_CLAIM_RETRY_BASE_MS << (s_claim_attempt - 1);
    meshx_os_timer_set_period(s_claim_retry_timer, retry_delay_ms);
    meshx_os_timer_start(s_claim_retry_timer);
}

/**
 * @brief Initiates the 0xCD Redundancy Claim procedure (Master-initiated).
 *
 * This function broadcasts a Redundancy Claim message to the virtual address
 * (29999) with exponential backoff retry scheduling for use during the
 * Standby role acquisition phase after a network failure.
 *
 * @param[in] claim_epoch   Epoch value for this claim sequence.
 * @param[in] next_free_addr Next unicast address to be assigned by the Master.
 * @return MESHX_SUCCESS if broadcast succeeds, otherwise an error code.
 */
meshx_err_t meshx_redundancy_claim_start(uint32_t claim_epoch, uint16_t next_free_addr)
{
    s_claim_epoch = claim_epoch;
    s_claim_next_free_addr = next_free_addr;
    s_claim_attempt = 1;

    uint8_t claim_buf[1 + sizeof(meshx_uvp_redundancy_claim_t)];
    claim_buf[0] = MESHX_UVP_OP_REDUNDANCY_CLAIM;
    meshx_uvp_redundancy_claim_t *claim = (meshx_uvp_redundancy_claim_t *)&claim_buf[1];
    claim->claim_epoch = claim_epoch;
    claim->master_addr = meshx_get_node_addr();
    claim->next_free_addr = next_free_addr;
    claim->reserved = 0;

    meshx_err_t err = meshx_comp_send_uvp(0,
                                          MESHX_MGMT_VIRTUAL_ADDR,
                                          MESHX_FUNC_ID_SYS_PAIRING,
                                          claim_buf,
                                          (uint16_t)sizeof(claim_buf),
                                          false);
    if (err != MESHX_SUCCESS)
        return err;

    MESHX_LOGI(MODULE_ID_COMMON, "UVP Redundancy: 0xCD claim broadcast (attempt=%u, epoch=%lu)",
               s_claim_attempt, (unsigned long)claim_epoch);

    /* Arm retry timer with exponential backoff: 2s, 4s, 8s */
    uint32_t retry_delay_ms = MESHX_REHEAL_CLAIM_RETRY_BASE_MS << (s_claim_attempt - 1);
    if (!s_claim_retry_timer)
    {
        meshx_os_timer_create("claim_retry", retry_delay_ms, false,
                              uvp_redundancy_claim_retry_cb, &s_claim_retry_timer);
    }
    if (s_claim_retry_timer)
    {
        meshx_os_timer_set_period(s_claim_retry_timer, retry_delay_ms);
        meshx_os_timer_start(s_claim_retry_timer);
    }
    return MESHX_SUCCESS;
}

/* =========================================================================
 * Registration Entry Point
 * ========================================================================= */
/**
 * @brief Register UVP redundancy handlers with the UVP module.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_uvp_register_redundancy_handlers(void)
{
    meshx_err_t err;

#if CONFIG_ENABLE_UVP_PROVISIONER
    err = meshx_uvp_register_handler(MESHX_FUNC_ID_SYS_PAIRING, MESHX_UVP_OP_REDUNDANCY_SYNC, uvp_handle_redundancy_sync);
    if (err != MESHX_SUCCESS) return err;

    err = meshx_uvp_register_handler(MESHX_FUNC_ID_SYS_PAIRING, MESHX_UVP_OP_INFILL_LEASE_REQ, uvp_handle_infill_lease_req);
    if (err != MESHX_SUCCESS) return err;

    err = meshx_uvp_register_handler(MESHX_FUNC_ID_SYS_PAIRING, MESHX_UVP_OP_FAST_PROV_REPORT, uvp_handle_fast_prov_report);
    if (err != MESHX_SUCCESS) return err;
#endif

    err = meshx_uvp_register_handler(MESHX_FUNC_ID_SYS_PAIRING, MESHX_UVP_OP_INFILL_LEASE_RSP, uvp_handle_infill_lease_rsp);
    if (err != MESHX_SUCCESS) return err;

    err = meshx_uvp_register_handler(MESHX_FUNC_ID_SYS_PAIRING, MESHX_UVP_OP_REDUNDANCY_CLAIM, uvp_handle_redundancy_claim);
    if (err != MESHX_SUCCESS) return err;

    err = meshx_uvp_register_handler(MESHX_FUNC_ID_SYS_PAIRING, MESHX_UVP_OP_REDUNDANCY_CLAIM_ACK, uvp_handle_redundancy_claim_ack);
    return err;
}
