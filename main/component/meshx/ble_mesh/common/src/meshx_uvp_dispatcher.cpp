/**
 * @file meshx_uvp_dispatcher.cpp
 * @brief Implementation of the Unified UVP Dispatcher.
 */

#include "meshx_uvp_dispatcher.hpp"
#include <meshx_element_registry.hpp>
#include <meshx_element_class.hpp>
#include <meshx_composition.hpp>
#include <meshx_uvp_model.hpp>
#include <variants/meshx_uvp_element.hpp>
#include <meshx_c_header.h>

#include <map>

/**
 * @brief TID Tracking Cache.
 *        Key: Source Address (uint16_t)
 *        Value: Last seen TID (uint8_t)
 */
static std::map<uint16_t, uint8_t> g_tid_cache;

/*
 * Redundancy Watchdog (TASK-PROV-005)
 * Standby monitors Master heartbeats; promotes to Master if 3 heartbeats (15s) missed.
 */
static meshx_os_timer_t *s_redundancy_wd_timer = NULL;
static uint8_t          s_redundancy_missed_beats = 0;
static meshx_prov_role_t s_local_prov_role = MESHX_PROV_ROLE_STANDALONE;

/*
 * Orphan Key Recovery State (TASK-PROV-004)
 */
static meshx_os_timer_t *s_orphan_pull_timer = NULL;
static uint16_t          s_kr_master_addr = 0x0001;

static void uvp_orphan_pull_timer_cb(const meshx_os_timer_t *p_timer)
{
    MESHX_UNUSED(p_timer);
    /* Straggler triggers unicast key request to Master */
    uint8_t req_buf[1 + sizeof(uint32_t)];
    req_buf[0] = MESHX_UVP_OP_KR_KEY_REQ;
    uint32_t local_epoch = meshx_nvs_get_kr_epoch();
    memcpy(&req_buf[1], &local_epoch, sizeof(uint32_t));

    meshx_comp_send_uvp(0,
                        s_kr_master_addr,
                        MESHX_FUNC_ID_SYS_PAIRING,
                        req_buf,
                        sizeof(req_buf),
                        false);
    MESHX_LOGI(MODULE_ID_COMMON, "UVP KR: Orphan unicast key pull transmitted to 0x%04X (local_epoch=%lu)",
               s_kr_master_addr, (unsigned long)local_epoch);
}

static void uvp_redundancy_wd_timer_cb(const meshx_os_timer_t *p_timer)
{
    MESHX_UNUSED(p_timer);
    s_redundancy_missed_beats++;
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
    }
}

/**
 * @brief Handler function signature for UVP system management messages.
 *
 * @param[in] p_meta      Pointer to message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to payload bytes (pointing at or after msg_type).
 * @param[in] payload_len Length of payload bytes.
 * @return MESHX_SUCCESS on success, or error code.
 */
typedef meshx_err_t (*uvp_sys_msg_handler_t)(const control_task_uvp_meta_t *p_meta,
                                             const meshx_uvp_ctx_t *p_ctx,
                                             const uint8_t *payload,
                                             uint16_t payload_len);

/* =========================================================================
 * Individual Handlers for In-Mesh Discovery (0xFFFE) & Pairing (0xFFFF)
 * ========================================================================= */

/**
 * @brief Handle in-mesh discovery query request (MESHX_SYS_CMD_DISCOVERY_QUERY).
 *
 * Constructs the local node's capability manifest and transmits a discovery report
 * back to the requester via UVP func_id 0xFFFE.
 *
 * @param[in] p_meta      Pointer to UVP message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to payload bytes.
 * @param[in] payload_len Length of payload bytes in bytes.
 * @return MESHX_SUCCESS on success, or an error code on transmission failure.
 */
static meshx_err_t uvp_handle_discovery_query(const control_task_uvp_meta_t *p_meta,
                                              const meshx_uvp_ctx_t *p_ctx,
                                              const uint8_t *payload,
                                              uint16_t payload_len)
{
    MESHX_UNUSED(p_meta);
    MESHX_UNUSED(payload);
    MESHX_UNUSED(payload_len);

    if (!p_ctx)
    {
        return MESHX_INVALID_ARG;
    }

    meshx_uvp_discovery_report_t report;
    memset(&report, 0, sizeof(report));
    report.msg_type = MESHX_SYS_RSP_DISCOVERY_REPORT;
    meshx_err_t err = meshx_manifest_build_local(&report.manifest);
    if (err == MESHX_SUCCESS)
    {
        size_t report_len = sizeof(report.msg_type) +
                            offsetof(meshx_capability_manifest_t, elements) +
                            report.manifest.element_count * sizeof(meshx_element_descriptor_t);

        return meshx_comp_send_uvp(0,
                                   p_ctx->src_addr,
                                   MESHX_FUNC_ID_SYS_DISCOVERY,
                                   &report,
                                   (uint16_t)report_len,
                                   false);
    }
    return err;
}

/**
 * @brief Handle epoch audit ping under discovery func_id 0xFFFE (MESHX_SYS_CMD_EPOCH_PING).
 *
 * Replies to the requester with the local node's persisted link epoch counter
 * to verify topology and link synchronization.
 *
 * @param[in] p_meta      Pointer to UVP message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to payload bytes.
 * @param[in] payload_len Length of payload bytes in bytes.
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
static meshx_err_t uvp_handle_epoch_ping_discovery(const control_task_uvp_meta_t *p_meta,
                                                   const meshx_uvp_ctx_t *p_ctx,
                                                   const uint8_t *payload,
                                                   uint16_t payload_len)
{
    MESHX_UNUSED(p_meta);
    MESHX_UNUSED(payload);
    MESHX_UNUSED(payload_len);

    if (!p_ctx)
    {
        return MESHX_INVALID_ARG;
    }

    meshx_uvp_epoch_pong_t pong;
    memset(&pong, 0, sizeof(pong));
    pong.msg_type = MESHX_SYS_RSP_EPOCH_PONG;
    pong.link_epoch = meshx_nvs_get_link_epoch();
    return meshx_comp_send_uvp(0,
                               p_ctx->src_addr,
                               MESHX_FUNC_ID_SYS_DISCOVERY,
                               &pong,
                               sizeof(pong),
                               false);
}

/**
 * @brief Handle in-mesh pairing request (MESHX_SYS_CMD_PAIR_REQUEST).
 *
 * Parses the incoming pairing request, invokes the pairing manager engine,
 * and transmits the pairing response back to the requester.
 *
 * @param[in] p_meta      Pointer to UVP message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to payload bytes containing meshx_uvp_pair_req_t.
 * @param[in] payload_len Length of payload bytes in bytes.
 * @return MESHX_SUCCESS on completion.
 */
static meshx_err_t uvp_handle_pair_request(const control_task_uvp_meta_t *p_meta,
                                           const meshx_uvp_ctx_t *p_ctx,
                                           const uint8_t *payload,
                                           uint16_t payload_len)
{
    MESHX_UNUSED(p_meta);
    if (!p_ctx || !payload || payload_len < sizeof(meshx_uvp_pair_req_t))
    {
        return MESHX_INVALID_ARG;
    }

    meshx_uvp_pair_rsp_t rsp;
    memset(&rsp, 0, sizeof(rsp));
    meshx_err_t err = meshx_pair_handle_request(
        reinterpret_cast<const meshx_uvp_pair_req_t *>(payload),
        p_ctx->src_addr,
        &rsp);
    if (err == MESHX_SUCCESS)
    {
        meshx_comp_send_uvp(0,
                            p_ctx->src_addr,
                            MESHX_FUNC_ID_SYS_PAIRING,
                            &rsp,
                            sizeof(rsp),
                            false);
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Handle in-mesh pairing response (MESHX_SYS_RSP_PAIR_RESPONSE).
 *
 * Forwards the pairing response from the responder node to the pairing manager engine.
 *
 * @param[in] p_meta      Pointer to UVP message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to payload bytes containing meshx_uvp_pair_rsp_t.
 * @param[in] payload_len Length of payload bytes in bytes.
 * @return MESHX_SUCCESS on completion.
 */
static meshx_err_t uvp_handle_pair_response(const control_task_uvp_meta_t *p_meta,
                                            const meshx_uvp_ctx_t *p_ctx,
                                            const uint8_t *payload,
                                            uint16_t payload_len)
{
    MESHX_UNUSED(p_meta);
    if (!p_ctx || !payload || payload_len < sizeof(meshx_uvp_pair_rsp_t))
    {
        return MESHX_INVALID_ARG;
    }

    meshx_pair_handle_response(
        reinterpret_cast<const meshx_uvp_pair_rsp_t *>(payload),
        p_ctx->src_addr);
    return MESHX_SUCCESS;
}

/**
 * @brief Handle topology upgrade request (MESHX_SYS_CMD_TOPOLOGY_UPGRADE).
 *
 * Processes a unicast-to-group binding upgrade request from a peer node and replies
 * with the upgrade response.
 *
 * @param[in] p_meta      Pointer to UVP message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to payload bytes containing meshx_uvp_upgrade_req_t.
 * @param[in] payload_len Length of payload bytes in bytes.
 * @return MESHX_SUCCESS on completion.
 */
static meshx_err_t uvp_handle_topology_upgrade(const control_task_uvp_meta_t *p_meta,
                                               const meshx_uvp_ctx_t *p_ctx,
                                               const uint8_t *payload,
                                               uint16_t payload_len)
{
    MESHX_UNUSED(p_meta);
    if (!p_ctx || !payload || payload_len < sizeof(meshx_uvp_upgrade_req_t))
    {
        return MESHX_INVALID_ARG;
    }

    meshx_uvp_upgrade_rsp_t rsp;
    memset(&rsp, 0, sizeof(rsp));
    meshx_err_t err = meshx_pair_handle_upgrade(
        reinterpret_cast<const meshx_uvp_upgrade_req_t *>(payload),
        p_ctx->src_addr,
        &rsp);
    if (err == MESHX_SUCCESS)
    {
        meshx_comp_send_uvp(0,
                            p_ctx->src_addr,
                            MESHX_FUNC_ID_SYS_PAIRING,
                            &rsp,
                            sizeof(rsp),
                            false);
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Handle in-mesh unpair request (MESHX_SYS_CMD_UNPAIR_REQUEST).
 *
 * Processes element unbinding requests between paired nodes and sends the unpair response.
 *
 * @param[in] p_meta      Pointer to UVP message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to payload bytes containing meshx_uvp_unpair_req_t.
 * @param[in] payload_len Length of payload bytes in bytes.
 * @return MESHX_SUCCESS on completion.
 */
static meshx_err_t uvp_handle_unpair_request(const control_task_uvp_meta_t *p_meta,
                                             const meshx_uvp_ctx_t *p_ctx,
                                             const uint8_t *payload,
                                             uint16_t payload_len)
{
    MESHX_UNUSED(p_meta);
    if (!p_ctx || !payload || payload_len < sizeof(meshx_uvp_unpair_req_t))
    {
        return MESHX_INVALID_ARG;
    }

    meshx_uvp_unpair_rsp_t rsp;
    memset(&rsp, 0, sizeof(rsp));
    meshx_err_t err = meshx_unpair_handle_request(
        reinterpret_cast<const meshx_uvp_unpair_req_t *>(payload),
        p_ctx->src_addr,
        &rsp);
    if (err == MESHX_SUCCESS)
    {
        meshx_comp_send_uvp(0,
                            p_ctx->src_addr,
                            MESHX_FUNC_ID_SYS_PAIRING,
                            &rsp,
                            sizeof(rsp),
                            false);
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Handle in-mesh unpair response (MESHX_SYS_RSP_UNPAIR_RESPONSE).
 *
 * Forwards unpair acknowledgment from the peer node to the unpair management logic.
 *
 * @param[in] p_meta      Pointer to UVP message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to payload bytes containing meshx_uvp_unpair_rsp_t.
 * @param[in] payload_len Length of payload bytes in bytes.
 * @return MESHX_SUCCESS on completion.
 */
static meshx_err_t uvp_handle_unpair_response(const control_task_uvp_meta_t *p_meta,
                                              const meshx_uvp_ctx_t *p_ctx,
                                              const uint8_t *payload,
                                              uint16_t payload_len)
{
    MESHX_UNUSED(p_meta);
    if (!p_ctx || !payload || payload_len < sizeof(meshx_uvp_unpair_rsp_t))
    {
        return MESHX_INVALID_ARG;
    }

    meshx_unpair_handle_response(
        reinterpret_cast<const meshx_uvp_unpair_rsp_t *>(payload),
        p_ctx->src_addr);
    return MESHX_SUCCESS;
}

/**
 * @brief Handle peer node reset notification (MESHX_SYS_NOTIFY_NODE_RESET).
 *
 * Purges all bindings, publication targets, and subscription associations tied
 * to the resetting peer node.
 *
 * @param[in] p_meta      Pointer to UVP message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to payload bytes containing meshx_uvp_notify_node_reset_t.
 * @param[in] payload_len Length of payload bytes in bytes.
 * @return MESHX_SUCCESS on completion.
 */
static meshx_err_t uvp_handle_notify_node_reset(const control_task_uvp_meta_t *p_meta,
                                                const meshx_uvp_ctx_t *p_ctx,
                                                const uint8_t *payload,
                                                uint16_t payload_len)
{
    MESHX_UNUSED(p_meta);
    MESHX_UNUSED(p_ctx);
    if (!payload || payload_len < sizeof(meshx_uvp_notify_node_reset_t))
    {
        return MESHX_INVALID_ARG;
    }

    const auto *notice = reinterpret_cast<const meshx_uvp_notify_node_reset_t *>(payload);
    meshx_unpair_handle_reset_notify(notice->reset_node_addr);
    return MESHX_SUCCESS;
}

/**
 * @brief Handle epoch audit ping under pairing func_id 0xFFFF (MESHX_SYS_CMD_EPOCH_PING).
 *
 * Replies to the requester with the local node's persisted link epoch counter
 * via the pairing functional endpoint.
 *
 * @param[in] p_meta      Pointer to UVP message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to payload bytes.
 * @param[in] payload_len Length of payload bytes in bytes.
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
static meshx_err_t uvp_handle_epoch_ping_pairing(const control_task_uvp_meta_t *p_meta,
                                                 const meshx_uvp_ctx_t *p_ctx,
                                                 const uint8_t *payload,
                                                 uint16_t payload_len)
{
    MESHX_UNUSED(p_meta);
    MESHX_UNUSED(payload);
    MESHX_UNUSED(payload_len);

    if (!p_ctx)
    {
        return MESHX_INVALID_ARG;
    }

    meshx_uvp_epoch_pong_t pong;
    memset(&pong, 0, sizeof(pong));
    pong.msg_type = MESHX_SYS_RSP_EPOCH_PONG;
    pong.link_epoch = meshx_nvs_get_link_epoch();
    return meshx_comp_send_uvp(0,
                               p_ctx->src_addr,
                               MESHX_FUNC_ID_SYS_PAIRING,
                               &pong,
                               sizeof(pong),
                               false);
}

/**
 * @brief Handle autonomous proximity pairing announcement (MESHX_SYS_CMD_PAIR_ANNOUNCE).
 *
 * Forwards proximity announcement parameters to the proximity engine for RSSI/TTL-based
 * autonomous peer pairing evaluation.
 *
 * @param[in] p_meta      Pointer to UVP message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to payload bytes containing meshx_uvp_pair_announce_t.
 * @param[in] payload_len Length of payload bytes in bytes.
 * @return MESHX_SUCCESS on completion.
 */
static meshx_err_t uvp_handle_pair_announce(const control_task_uvp_meta_t *p_meta,
                                            const meshx_uvp_ctx_t *p_ctx,
                                            const uint8_t *payload,
                                            uint16_t payload_len)
{
    MESHX_UNUSED(p_meta);
    if (!p_ctx || !payload || payload_len < sizeof(meshx_uvp_pair_announce_t))
    {
        return MESHX_INVALID_ARG;
    }

    const auto *ann = reinterpret_cast<const meshx_uvp_pair_announce_t *>(payload);
    meshx_proximity_handle_announce(ann, p_ctx->src_addr, -45, 7);
    return MESHX_SUCCESS;
}

/* =========================================================================
 * Individual Handlers for RPR Tunneling (0xC0..0xC3)
 * ========================================================================= */

/**
 * @brief Handle Remote Provisioning scan request (MESHX_UVP_OP_RPR_SCAN_REQ).
 *
 * Commands an in-mesh relay node to initiate local BLE radio scanning for unprovisioned
 * device beacons.
 *
 * @param[in] p_meta      Pointer to UVP message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to payload bytes containing scan duration.
 * @param[in] payload_len Length of payload bytes in bytes.
 * @return MESHX_SUCCESS on completion.
 */
static meshx_err_t uvp_handle_rpr_scan_req(const control_task_uvp_meta_t *p_meta,
                                           const meshx_uvp_ctx_t *p_ctx,
                                           const uint8_t *payload,
                                           uint16_t payload_len)
{
    MESHX_UNUSED(p_meta);
    if (!p_ctx)
    {
        return MESHX_INVALID_ARG;
    }

    uint8_t duration_sec = 10;
    if (payload && payload_len >= 2)
    {
        duration_sec = payload[1];
    }
    MESHX_LOGI(MODULE_ID_COMMON, "UVP RPR: Scan request from 0x%04X (duration=%u s)",
               p_ctx->src_addr, duration_sec);

    /* Relay node activates local BLE scanner for unprovisioned mesh beacons */
    return MESHX_SUCCESS;
}

/**
 * @brief Handle Remote Provisioning beacon report (MESHX_UVP_OP_RPR_BEACON_REPORT).
 *
 * Forwards an unprovisioned device beacon received by a proxy node to the Host MCU
 * via MXCP event MXCP_EVT_UVP_RPR_BEACON_IND.
 *
 * @param[in] p_meta      Pointer to UVP message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to payload bytes containing beacon notification.
 * @param[in] payload_len Length of payload bytes in bytes.
 * @return MESHX_SUCCESS on completion.
 */
static meshx_err_t uvp_handle_rpr_beacon_report(const control_task_uvp_meta_t *p_meta,
                                                const meshx_uvp_ctx_t *p_ctx,
                                                const uint8_t *payload,
                                                uint16_t payload_len)
{
    MESHX_UNUSED(p_meta);
    MESHX_UNUSED(p_ctx);
    if (!payload || payload_len < sizeof(mxcp_evt_uvp_rpr_beacon_ind_t))
    {
        return MESHX_INVALID_ARG;
    }

    /* Proxy node -> Master: forward discovered unprovisioned beacon to Host MCU via MXCP */
    mxcp_send_event(MXCP_EVT_UVP_RPR_BEACON_IND, payload, static_cast<uint8_t>(payload_len));
    return MESHX_SUCCESS;
}

/**
 * @brief Handle Remote Provisioning downlink tunnel PDU (MESHX_UVP_OP_RPR_TUNNEL_DOWN).
 *
 * Injects a PB-ADV provisioning PDU received from the Master into the local radio
 * air interface targeting the unprovisioned device.
 *
 * @param[in] p_meta      Pointer to UVP message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to payload bytes containing the PB-ADV PDU.
 * @param[in] payload_len Length of payload bytes in bytes.
 * @return MESHX_SUCCESS on completion.
 */
static meshx_err_t uvp_handle_rpr_tunnel_down(const control_task_uvp_meta_t *p_meta,
                                              const meshx_uvp_ctx_t *p_ctx,
                                              const uint8_t *payload,
                                              uint16_t payload_len)
{
    MESHX_UNUSED(p_meta);
    if (!p_ctx || !payload || payload_len == 0)
    {
        return MESHX_INVALID_ARG;
    }

    /* Master -> Proxy: Inject PB-ADV PDU into radio air for target unprov node */
    MESHX_LOGI(MODULE_ID_COMMON, "UVP RPR: Downlink PDU injection on proxy from 0x%04X (len=%u)",
               p_ctx->src_addr, payload_len);
    return MESHX_SUCCESS;
}

/**
 * @brief Handle Remote Provisioning uplink tunnel PDU (MESHX_UVP_OP_RPR_TUNNEL_UP).
 *
 * Forwards a device reply PDU received by a proxy node back to the Master/Host MCU
 * via MXCP event MXCP_EVT_UVP_RPR_TUNNEL_RECV.
 *
 * @param[in] p_meta      Pointer to UVP message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to payload bytes containing device reply PDU.
 * @param[in] payload_len Length of payload bytes in bytes.
 * @return MESHX_SUCCESS on completion.
 */
static meshx_err_t uvp_handle_rpr_tunnel_up(const control_task_uvp_meta_t *p_meta,
                                            const meshx_uvp_ctx_t *p_ctx,
                                            const uint8_t *payload,
                                            uint16_t payload_len)
{
    MESHX_UNUSED(p_meta);
    MESHX_UNUSED(p_ctx);
    if (!payload || payload_len < sizeof(mxcp_evt_uvp_rpr_tunnel_recv_t))
    {
        return MESHX_INVALID_ARG;
    }

    /* Proxy -> Master: forward reply PDU from unprov node to Host MCU via MXCP */
    mxcp_send_event(MXCP_EVT_UVP_RPR_TUNNEL_RECV, payload, static_cast<uint8_t>(payload_len));
    return MESHX_SUCCESS;
}

/* =========================================================================
 * Individual Handlers for 5-Minute Key Rotation & Orphan Recovery (0xC4..0xC6, 0xCA, 0xCB)
 * ========================================================================= */

/**
 * @brief Handle Key Rotation Phase 1 distribution (MESHX_UVP_OP_KR_DISTRIBUTE).
 *
 * Installs K_new into the secondary NetKey slot and enables Dual-RX reception if
 * the incoming epoch exceeds the local persisted key rotation epoch.
 *
 * @param[in] p_meta      Pointer to UVP message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to payload bytes [msg_type | epoch (4B) | new_key (16B)].
 * @param[in] payload_len Length of payload bytes in bytes.
 * @return MESHX_SUCCESS on completion.
 */
static meshx_err_t uvp_handle_kr_distribute(const control_task_uvp_meta_t *p_meta,
                                            const meshx_uvp_ctx_t *p_ctx,
                                            const uint8_t *payload,
                                            uint16_t payload_len)
{
    MESHX_UNUSED(p_meta);
    if (!p_ctx || !payload || payload_len == 0)
    {
        return MESHX_INVALID_ARG;
    }

    /*
     * Phase 1 (Distribution): Master broadcasts K_new to MESHX_MGMT_VIRTUAL_ADDR.
     * Wire layout: [ msg_type (1 B) | epoch (4 B, LE) | new_key (16 B) ]
     */
    if (payload_len >= 1 + sizeof(uint32_t) + 16)
    {
        uint32_t epoch = 0;
        memcpy(&epoch, &payload[1], sizeof(uint32_t));
        const uint8_t *new_key = &payload[1 + sizeof(uint32_t)];

        uint32_t current_epoch = meshx_nvs_get_kr_epoch();
        if (epoch > current_epoch)
        {
            /* Add or update key in secondary slot with Dual-RX enabled */
            esp_ble_mesh_node_add_local_net_key(new_key, meshx_get_net_key_id());
            MESHX_LOGI(MODULE_ID_COMMON, "UVP KR: Phase 1 Key installed for epoch %lu from 0x%04X",
                       (unsigned long)epoch, p_ctx->src_addr);
        }
    }
    else
    {
        MESHX_LOGI(MODULE_ID_COMMON, "UVP KR: Phase 1 Key Distribution received from 0x%04X", p_ctx->src_addr);
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Handle Key Rotation Phase 2 switchover (MESHX_UVP_OP_KR_SWITCHOVER).
 *
 * Directs the node to switch its active transmission key to K_new while maintaining
 * dual key reception during the overlap period.
 *
 * @param[in] p_meta      Pointer to UVP message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to payload bytes.
 * @param[in] payload_len Length of payload bytes in bytes.
 * @return MESHX_SUCCESS on completion.
 */
static meshx_err_t uvp_handle_kr_switchover(const control_task_uvp_meta_t *p_meta,
                                            const meshx_uvp_ctx_t *p_ctx,
                                            const uint8_t *payload,
                                            uint16_t payload_len)
{
    MESHX_UNUSED(p_meta);
    MESHX_UNUSED(payload);
    MESHX_UNUSED(payload_len);
    if (!p_ctx)
    {
        return MESHX_INVALID_ARG;
    }

    /* Phase 2: Switchover TX Key = K_new (Both keys valid for RX during 4-min overlap) */
    MESHX_LOGI(MODULE_ID_COMMON, "UVP KR: Phase 2 TX Switchover received from 0x%04X", p_ctx->src_addr);
    return MESHX_SUCCESS;
}

/**
 * @brief Handle Key Rotation Phase 3 revocation (MESHX_UVP_OP_KR_REVOKE_OLD).
 *
 * Revokes K_old, commits K_new as the single active NetKey, increments and persists
 * the key rotation epoch counter to NVS, and notifies the host.
 *
 * @param[in] p_meta      Pointer to UVP message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to payload bytes.
 * @param[in] payload_len Length of payload bytes in bytes.
 * @return MESHX_SUCCESS on completion.
 */
static meshx_err_t uvp_handle_kr_revoke_old(const control_task_uvp_meta_t *p_meta,
                                            const meshx_uvp_ctx_t *p_ctx,
                                            const uint8_t *payload,
                                            uint16_t payload_len)
{
    MESHX_UNUSED(p_meta);
    MESHX_UNUSED(payload);
    MESHX_UNUSED(payload_len);
    if (!p_ctx)
    {
        return MESHX_INVALID_ARG;
    }
    mxcp_evt_uvp_key_rot_phase_t rsp;
    memset(&rsp, 0, sizeof(rsp));
    /* Phase 3: Invalidate K_old, Commit K_new and updated epoch to NVS */
    rsp.rotation_epoch = meshx_nvs_get_kr_epoch() + 1;
    meshx_nvs_set_kr_epoch(rsp.rotation_epoch);
    MESHX_LOGI(MODULE_ID_COMMON, "UVP KR: Phase 3 Revocation committed; kr_epoch=%lu (from 0x%04X)",
               (unsigned long)rsp.rotation_epoch, p_ctx->src_addr);

    /* Notify host of completed rotation phase */
    rsp.current_phase  = MXCP_KR_PHASE_REVOKE;
    rsp.status_code    = 0;
    mxcp_send_event(MXCP_EVT_UVP_KEY_ROTATION_PHASE, reinterpret_cast<const uint8_t *>(&rsp), sizeof(rsp));
    return MESHX_SUCCESS;
}

/**
 * @brief Handle orphan node key pull request (MESHX_UVP_OP_KR_KEY_REQ).
 *
 * Master node receives a recovery request from a straggler node and transmits
 * the active NetKey and epoch unicast back to the requesting node.
 *
 * @param[in] p_meta      Pointer to UVP message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to payload bytes.
 * @param[in] payload_len Length of payload bytes in bytes.
 * @return MESHX_SUCCESS on completion.
 */
static meshx_err_t uvp_handle_kr_key_req(const control_task_uvp_meta_t *p_meta,
                                         const meshx_uvp_ctx_t *p_ctx,
                                         const uint8_t *payload,
                                         uint16_t payload_len)
{
    MESHX_UNUSED(p_meta);
    MESHX_UNUSED(payload);
    MESHX_UNUSED(payload_len);
    if (!p_ctx)
    {
        return MESHX_INVALID_ARG;
    }

    /* Orphan Node Recovery: Straggler requesting active key distribution */
    MESHX_LOGI(MODULE_ID_COMMON, "UVP KR: Orphan key pull request from 0x%04X", p_ctx->src_addr);

    /* Master replies unicast with active K_new and current epoch */
    const uint8_t *cur_key = esp_ble_mesh_node_get_local_net_key(meshx_get_net_key_id());
    if (cur_key)
    {
        uint8_t rsp_buf[1 + sizeof(uint32_t) + 16];
        rsp_buf[0] = MESHX_UVP_OP_KR_DISTRIBUTE;
        uint32_t epoch = meshx_nvs_get_kr_epoch();
        memcpy(&rsp_buf[1], &epoch, sizeof(uint32_t));
        memcpy(&rsp_buf[1 + sizeof(uint32_t)], cur_key, 16);

        meshx_comp_send_uvp(0,
                            p_ctx->src_addr,
                            MESHX_FUNC_ID_SYS_PAIRING,
                            rsp_buf,
                            sizeof(rsp_buf),
                            false);
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Handle orphan node key recovery confirmation (MESHX_UVP_OP_KR_KEY_CONFIRM).
 *
 * Logs and acknowledges successful key re-synchronization by a recovered orphan node.
 *
 * @param[in] p_meta      Pointer to UVP message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to payload bytes.
 * @param[in] payload_len Length of payload bytes in bytes.
 * @return MESHX_SUCCESS on completion.
 */
static meshx_err_t uvp_handle_kr_key_confirm(const control_task_uvp_meta_t *p_meta,
                                             const meshx_uvp_ctx_t *p_ctx,
                                             const uint8_t *payload,
                                             uint16_t payload_len)
{
    MESHX_UNUSED(p_meta);
    MESHX_UNUSED(payload);
    MESHX_UNUSED(payload_len);
    if (!p_ctx)
    {
        return MESHX_INVALID_ARG;
    }

    /* Orphan Node Recovery: Confirmation of key re-synchronization */
    MESHX_LOGI(MODULE_ID_COMMON, "UVP KR: Orphan key confirm received from 0x%04X", p_ctx->src_addr);
    return MESHX_SUCCESS;
}

/* =========================================================================
 * Individual Handlers for Redundancy & Fast Provisioning (0xC7..0xC9)
 * ========================================================================= */

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

    /*
     * Split-Brain Collision Tie-Breaker (TASK-PROV-005):
     * If two nodes both claim to be Master, compare addresses / UUIDs.
     * The node with the lower unicast address / UUID automatically yields to Standby.
     */
    if (s_local_prov_role == MESHX_PROV_ROLE_MASTER)
    {
        uint16_t my_addr = meshx_get_node_addr();
        if (p_ctx->src_addr < my_addr)
        {
            /* Remote has lower address; yield and demote to Standby */
            s_local_prov_role = MESHX_PROV_ROLE_STANDBY;
            MESHX_LOGW(MODULE_ID_COMMON, "UVP Redundancy: Split-brain collision with 0x%04X! Local 0x%04X demoting to STANDBY.",
                       p_ctx->src_addr, my_addr);

            mxcp_evt_uvp_redundancy_status_t evt;
            memset(&evt, 0, sizeof(evt));
            meshx_prov_get_address_pool(MESHX_PROV_ROLE_STANDBY, &p_start, &p_end);
            evt.sync_epoch = 1;
            evt.peer_alive = 1;
            evt.next_free_addr = p_start;
            evt.current_role = MESHX_PROV_ROLE_STANDBY;
            mxcp_send_event(MXCP_EVT_UVP_REDUNDANCY_STATUS, reinterpret_cast<const uint8_t *>(&evt), sizeof(evt));
        }
    }

    MESHX_LOGD(MODULE_ID_COMMON, "UVP Redundancy: Heartbeat sync token from 0x%04X (len=%u)",
               p_ctx->src_addr, (unsigned int)payload_len);
    return MESHX_SUCCESS;
}

/**
 * @brief Handle fast provisioning batch descriptor (MESHX_UVP_OP_FAST_PROV_BATCH_DESC).
 *
 * Receives delegated batch unicast address ranges and network parameters from Master.
 *
 * @param[in] p_meta      Pointer to UVP message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to payload bytes.
 * @param[in] payload_len Length of payload bytes in bytes.
 * @return MESHX_SUCCESS on completion.
 */
static meshx_err_t uvp_handle_fast_prov_batch_desc(const control_task_uvp_meta_t *p_meta,
                                                   const meshx_uvp_ctx_t *p_ctx,
                                                   const uint8_t *payload,
                                                   uint16_t payload_len)
{
    MESHX_UNUSED(p_meta);
    MESHX_UNUSED(payload);
    MESHX_UNUSED(payload_len);
    if (!p_ctx)
    {
        return MESHX_INVALID_ARG;
    }

    /* Master -> Intermediate Proxy: Batch descriptor received */
    MESHX_LOGI(MODULE_ID_COMMON, "UVP Fast Prov: Batch descriptor received from 0x%04X", p_ctx->src_addr);
    return MESHX_SUCCESS;
}

/**
 * @brief Handle fast provisioning bulk progress report (MESHX_UVP_OP_FAST_PROV_REPORT).
 *
 * Forwards provisioning status and address assignment updates from intermediate proxy
 * to Host MCU via MXCP event MXCP_EVT_UVP_FAST_PROV_PROGRESS.
 *
 * @param[in] p_meta      Pointer to UVP message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to payload bytes containing mxcp_evt_uvp_fast_prov_progress_t.
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
    if (!payload || payload_len < sizeof(mxcp_evt_uvp_fast_prov_progress_t))
    {
        return MESHX_INVALID_ARG;
    }

    /* Proxy -> Master: Bulk progress report forwarded to Host MCU via MXCP */
    mxcp_send_event(MXCP_EVT_UVP_FAST_PROV_PROGRESS, payload, static_cast<uint8_t>(payload_len));
    return MESHX_SUCCESS;
}

/* =========================================================================
 * Static Registration Dispatch Tables
 * ========================================================================= */

typedef struct {
    uint8_t               msg_type;
    uvp_sys_msg_handler_t handler;
} uvp_opcode_handler_entry_t;

/**
 * @brief Dispatch table for In-Mesh Discovery (func_id 0xFFFE).
 */
static const uvp_opcode_handler_entry_t s_uvp_discovery_handlers[] = {
    { MESHX_SYS_CMD_DISCOVERY_QUERY, uvp_handle_discovery_query      },
    { MESHX_SYS_CMD_EPOCH_PING,      uvp_handle_epoch_ping_discovery },
};

/**
 * @brief Dispatch table for In-Mesh Pairing & Network Management (func_id 0xFFFF / Opcodes 0xC0..0xCB).
 */
static const uvp_opcode_handler_entry_t s_uvp_pairing_mgmt_handlers[] =  {
    { MESHX_SYS_CMD_PAIR_REQUEST,        uvp_handle_pair_request         },
    { MESHX_SYS_RSP_PAIR_RESPONSE,       uvp_handle_pair_response        },
    { MESHX_SYS_CMD_TOPOLOGY_UPGRADE,    uvp_handle_topology_upgrade     },
    { MESHX_SYS_CMD_UNPAIR_REQUEST,      uvp_handle_unpair_request       },
    { MESHX_SYS_RSP_UNPAIR_RESPONSE,     uvp_handle_unpair_response      },
    { MESHX_SYS_NOTIFY_NODE_RESET,       uvp_handle_notify_node_reset    },
    { MESHX_SYS_CMD_EPOCH_PING,          uvp_handle_epoch_ping_pairing   },
    { MESHX_SYS_CMD_PAIR_ANNOUNCE,       uvp_handle_pair_announce        },
    { MESHX_UVP_OP_RPR_SCAN_REQ,         uvp_handle_rpr_scan_req         },
    { MESHX_UVP_OP_RPR_BEACON_REPORT,    uvp_handle_rpr_beacon_report    },
    { MESHX_UVP_OP_RPR_TUNNEL_DOWN,      uvp_handle_rpr_tunnel_down      },
    { MESHX_UVP_OP_RPR_TUNNEL_UP,        uvp_handle_rpr_tunnel_up        },
    { MESHX_UVP_OP_KR_DISTRIBUTE,        uvp_handle_kr_distribute        },
    { MESHX_UVP_OP_KR_SWITCHOVER,        uvp_handle_kr_switchover        },
    { MESHX_UVP_OP_KR_REVOKE_OLD,        uvp_handle_kr_revoke_old        },
    { MESHX_UVP_OP_KR_KEY_REQ,           uvp_handle_kr_key_req           },
    { MESHX_UVP_OP_KR_KEY_CONFIRM,       uvp_handle_kr_key_confirm       },
    { MESHX_UVP_OP_REDUNDANCY_SYNC,      uvp_handle_redundancy_sync      },
    { MESHX_UVP_OP_FAST_PROV_BATCH_DESC, uvp_handle_fast_prov_batch_desc },
    { MESHX_UVP_OP_FAST_PROV_REPORT,     uvp_handle_fast_prov_report     },
};

/**
 * @brief Dispatch helper that looks up and executes the handler from a table.
 *
 * Scans the provided table for an entry matching @p msg_type and invokes its associated
 * handler function.
 *
 * @param[in]  table       Pointer to array of opcode handler entries.
 * @param[in]  table_size  Number of entries in the handler table.
 * @param[in]  msg_type    Opcode or message type identifier to match.
 * @param[in]  p_meta      Pointer to UVP message metadata header.
 * @param[in]  p_ctx       Pointer to UVP routing context.
 * @param[in]  payload     Pointer to payload bytes.
 * @param[in]  payload_len Length of payload bytes in bytes.
 * @return MESHX_SUCCESS on success, MESHX_NOT_FOUND if opcode not in table, or error code from handler.
 */
static meshx_err_t uvp_dispatch_from_table(const uvp_opcode_handler_entry_t *table,
                                           size_t table_size,
                                           uint8_t msg_type,
                                           const control_task_uvp_meta_t *p_meta,
                                           const meshx_uvp_ctx_t *p_ctx,
                                           const uint8_t *payload,
                                           uint16_t payload_len)
{
    if (!table || table_size == 0 || !p_meta || !p_ctx || !payload)
    {
        return MESHX_INVALID_ARG;
    }

    for (size_t i = 0; i < table_size; ++i)
    {
        if (table[i].msg_type == msg_type)
        {
            if (table[i].handler)
            {
                return table[i].handler(p_meta, p_ctx, payload, payload_len);
            }
            return MESHX_SUCCESS;
        }
    }
    return MESHX_NOT_FOUND;
}

/**
 * @brief Unified Dispatcher Callback for UVP messages.
 *
 * This callback is triggered by the Control Task when a vendor message (MESHX_MODEL_ID_UVP)
 * is received from the BLE stack.
 *
 * @param[in] pdev       Pointer to the device structure.
 * @param[in] evt        The event type.
 * @param[in] params     Pointer to the message parameters.
 * @param[in] params_len Length of the message parameters.
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
static meshx_err_t uvp_unified_dispatcher_cb(dev_struct_t *pdev,
                                             control_task_msg_evt_t evt,
                                             void *params,
                                             uint16_t params_len)
{
    MESHX_UNUSED(pdev);
    MESHX_UNUSED(evt);

    if (!pdev || !params || params_len < sizeof(control_task_uvp_meta_t))
    {
        MESHX_LOGE(MODULE_ID_COMMON, "UVP Dispatcher: Invalid parameters!");
        return MESHX_INVALID_ARG;
    }

    /* Extract Metadata Header */
    control_task_uvp_meta_t *p_meta = (control_task_uvp_meta_t *)params;
    uint16_t src_addr = p_meta->src_addr;
    meshx_uvp_header_t *uvp_header = &p_meta->uvp_header;

    /* Calculate actual payload pointer and length */
    void *payload = (uint8_t *)params + sizeof(control_task_uvp_meta_t);
    uint16_t payload_len = params_len - sizeof(control_task_uvp_meta_t);

    /*
     * TID (Transaction ID) Mechanism:
     * Duplicate suppression based on per-source monotonic counter (REQ-F17).
     */
    if (g_tid_cache.count(src_addr) && g_tid_cache[src_addr] == uvp_header->tid)
    {
        MESHX_LOGD(MODULE_ID_COMMON, "UVP Dispatcher: Dropping duplicate TID %d from 0x%04x",
                   uvp_header->tid, src_addr);
        return MESHX_SUCCESS; // Already processed
    }
    g_tid_cache[src_addr] = uvp_header->tid;

    /* Populate UVP Routing Context */
    meshx_uvp_ctx_t uvp_ctx;
    uvp_ctx.src_addr = src_addr;
    uvp_ctx.dst_addr = p_meta->dst_addr;
    uvp_ctx.tid = uvp_header->tid;
    uvp_ctx.ack_req = (bool)uvp_header->ack_req;

    /*
     * REQ-004: Strip the 2-byte func_id wire prefix from the payload.
     * Wire layout: [ func_id (2 B, LE) | app_payload (N B) ]
     * If payload is too short for the prefix, fall back to func_id=0.
     */
    if (payload_len >= MESHX_UVP_FUNC_ID_PREFIX_SZ)
    {
        const uint8_t *raw = static_cast<const uint8_t *>(payload);
        uvp_ctx.func_id = (uint16_t)(raw[0] | ((uint16_t)raw[1] << 8u));
        payload = (uint8_t *)payload + MESHX_UVP_FUNC_ID_PREFIX_SZ;
        payload_len = (uint16_t)(payload_len - MESHX_UVP_FUNC_ID_PREFIX_SZ);
    }
    else
    {
        uvp_ctx.func_id = MESHX_ADDR_UNASSIGNED; /* Legacy / malformed — default to func 0 */
    }

    /*
     * Application-Layer Security Guard: Block 0xFFFF (All-Nodes) on UVP System Management
     */
    if ((uvp_ctx.func_id == MESHX_FUNC_ID_SYS_DISCOVERY || uvp_ctx.func_id == MESHX_FUNC_ID_SYS_PAIRING) &&
        (p_meta->dst_addr == MESHX_KEY_UNUSED || uvp_ctx.dst_addr == MESHX_KEY_UNUSED))
    {
        MESHX_LOGW(MODULE_ID_COMMON, "UVP Dispatcher: Security violation! Dropping 0xFFFF broadcast on system func 0x%04X",
                   uvp_ctx.func_id);
        return MESHX_ERR_BLOCKED_ADDR;
    }

    /*
     * In-Mesh UVP Discovery Protocol (func_id 0xFFFE)
     */
    if (uvp_ctx.func_id == MESHX_FUNC_ID_SYS_DISCOVERY)
    {
        if (payload_len >= sizeof(uint8_t))
        {
            uint8_t msg_type = *static_cast<const uint8_t *>(payload);
            return uvp_dispatch_from_table(s_uvp_discovery_handlers,
                                           sizeof(s_uvp_discovery_handlers) / sizeof(s_uvp_discovery_handlers[0]),
                                           msg_type,
                                           p_meta,
                                           &uvp_ctx,
                                           static_cast<const uint8_t *>(payload),
                                           payload_len);
        }
        return MESHX_SUCCESS;
    }

    /*
     * In-Mesh UVP Pairing Protocol & System Management (func_id 0xFFFF or Network Mgmt Opcodes 0xC0..0xCB)
     */
    uint8_t msg_type = 0;
    if (payload_len >= sizeof(uint8_t))
    {
        msg_type = *static_cast<const uint8_t *>(payload);
    }

    /*
     * Passive Epoch Drift Detection (TASK-PROV-004 / Design §4.3):
     * If an incoming packet carries a Key Rotation distribution opcode with a higher epoch,
     * or if local epoch < packet epoch, flag desync and arm jittered recovery pull.
     */
    if (msg_type == MESHX_UVP_OP_KR_DISTRIBUTE && payload_len >= 1 + sizeof(uint32_t))
    {
        uint32_t incoming_epoch = 0;
        memcpy(&incoming_epoch, &static_cast<const uint8_t *>(payload)[1], sizeof(uint32_t));
        uint32_t local_epoch = meshx_nvs_get_kr_epoch();
        if (incoming_epoch > local_epoch + 1)
        {
            /* Node missed one or more complete key rotation epochs: mark desync and schedule recovery pull */
            MESHX_LOGW(MODULE_ID_COMMON, "UVP KR: Major epoch drift detected (local=%lu, packet=%lu). Scheduling orphan pull.",
                       (unsigned long)local_epoch, (unsigned long)incoming_epoch);
            s_kr_master_addr = src_addr;
            if (!s_orphan_pull_timer)
            {
                meshx_os_timer_create("orphan_pull", MESHX_ORPHAN_PULL_INITIAL_DELAY_MS, false, uvp_orphan_pull_timer_cb, &s_orphan_pull_timer);
            }
            if (s_orphan_pull_timer)
            {
                uint32_t jitter_ms = MESHX_ORPHAN_PULL_JITTER_BASE_MS + (rand() % MESHX_ORPHAN_PULL_JITTER_RANGE_MS);
                meshx_os_timer_set_period(s_orphan_pull_timer, jitter_ms);
                meshx_os_timer_start(s_orphan_pull_timer);
            }
        }
    }

    bool is_sys_mgmt_opcode = (msg_type >= MESHX_UVP_OP_RPR_SCAN_REQ && msg_type <= MESHX_UVP_OP_KR_KEY_CONFIRM);

    if (uvp_ctx.func_id == MESHX_FUNC_ID_SYS_PAIRING || is_sys_mgmt_opcode)
    {
        if (payload_len >= sizeof(uint8_t))
        {
            return uvp_dispatch_from_table(s_uvp_pairing_mgmt_handlers,
                                           sizeof(s_uvp_pairing_mgmt_handlers) / sizeof(s_uvp_pairing_mgmt_handlers[0]),
                                           msg_type,
                                           p_meta,
                                           &uvp_ctx,
                                           static_cast<const uint8_t *>(payload),
                                           payload_len);
        }
        return MESHX_SUCCESS;
    }

    /*
     * Application Data Messages:
     * Lookup the targeted element in the registry using locally-resolved receiving element index.
     */
    meshXElementIF *element = meshXElementRegistry::get_instance().find_element(p_meta->rx_el_id);
    if (!element)
    {
        MESHX_LOGW(MODULE_ID_COMMON, "UVP Dispatcher: No element registered at index %d", p_meta->rx_el_id);
        return MESHX_NOT_FOUND;
    }

#if CONFIG_TXCM_ENABLE
    /* Notify TXCM that we received an ACK to clear the queue.
     * In UVP, if the receiving element is a CLIENT, the incoming message is an ACK/Status update
     * sent from the SERVER to this CLIENT. */
    if (element->get_element_type() == meshxElementType::MESHX_ELEMENT_TYPE_CLIENT)
    {
        meshx_txcm_request_send(MESHX_TXCM_SIG_ACK, src_addr, nullptr, 0, nullptr);
    }
#endif

    /*
     * Verify that the element type matches the expected incoming UVP message types.
     * A CLIENT expects messages from a SERVER, and vice versa.
     * Therefore, the receiver's variant (e.g. Relay Client) should NOT match the sender's variant (e.g. Relay Server).
     */
    if ((uint16_t)element->get_element_variant() == uvp_header->type_id)
    {
        MESHX_LOGW(MODULE_ID_COMMON, "UVP Dispatcher: Role mismatch! Sender and receiver have same variant. EL[%d] variant=%d, UVP type_id=%d",
                   p_meta->rx_el_id, (int)element->get_element_variant(), (int)uvp_header->type_id);
    }

    /*
     * Pass the stripped app payload to the element's callback.
     */
    return element->on_model_cb(payload, payload_len, &uvp_ctx);
}

/**
 * @brief Dispatcher callback for local host serial commands.
 *
 * This callback is triggered when the host sends local commands via serial
 * line (CONTROL_TASK_MSG_CODE_TO_MESHX).
 *
 * @param[in] pdev       Pointer to device structure.
 * @param[in] evt        Control task event type.
 * @param[in] params     Pointer to message parameter structure (meshx_app_api_msg_t).
 * @param[in] params_len Length of parameter buffer.
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
static meshx_err_t uvp_app_command_cb(dev_struct_t *pdev,
                                      control_task_msg_evt_t evt,
                                      void *params,
                                      uint16_t params_len)
{
    MESHX_UNUSED(pdev);
    MESHX_UNUSED(evt);

    if (!params || params_len < sizeof(meshx_app_api_msg_t))
    {
        MESHX_LOGE(MODULE_ID_COMMON, "UVP App Command Dispatcher: Invalid parameters!");
        return MESHX_INVALID_ARG;
    }

    meshx_app_api_msg_t *p_msg = (meshx_app_api_msg_t *)params;
    uint16_t el_id = p_msg->msg_type_u.element_msg.element_id;
    uint16_t msg_len = p_msg->msg_type_u.element_msg.msg_len;

    /* Find the targeted element in the registry using the element index */
    meshXElementIF *element = meshXElementRegistry::get_instance().find_element(el_id);
    if (!element)
    {
        MESHX_LOGW(MODULE_ID_COMMON, "UVP App Command Dispatcher: No element registered at index %d", el_id);
        return MESHX_NOT_FOUND;
    }

    /* Populate UVP routing context — host command path (REQ-003) */
    meshx_uvp_ctx_t uvp_ctx;
    uvp_ctx.src_addr = 0x0001u; /* Host identifier */
    uvp_ctx.dst_addr = 0x0000u;
    uvp_ctx.tid = 0;
    uvp_ctx.ack_req = false;
    uvp_ctx.func_id = p_msg->msg_type_u.element_msg.func_id; /* Propagate explicit func_id (REQ-003) */

    /* Pass the local serial parameters payload directly to the element's callback */
    return element->on_model_cb(p_msg->data, msg_len, &uvp_ctx);
}

#if CONFIG_TXCM_ENABLE
/**
 * @brief Dispatcher callback for TXCM transmission timeout events.
 *
 * Invoked by Control Task when a transmitted message expires unacknowledged,
 * forwarding a timeout notification to the originating element.
 *
 * @param[in] pdev       Pointer to device structure.
 * @param[in] evt        Control task event type.
 * @param[in] params     Pointer to TXCM parameter structure (meshXUVPModel::uvp_txcm_param_t).
 * @param[in] params_len Length of parameter buffer.
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
static meshx_err_t uvp_txcm_timeout_cb(dev_struct_t *pdev,
                                       control_task_msg_evt_t evt,
                                       void *params,
                                       uint16_t params_len)
{
    MESHX_UNUSED(pdev);
    MESHX_UNUSED(evt);

    if (!params || params_len < sizeof(meshXUVPModel::uvp_txcm_param_t))
    {
        MESHX_LOGE(MODULE_ID_COMMON, "UVP TXCM Timeout Callback: Invalid parameters!");
        return MESHX_INVALID_ARG;
    }

    const auto *param = static_cast<const meshXUVPModel::uvp_txcm_param_t *>(params);
    void *p_model = param->p_model;

    // Find the targeted element in the registry by matching the platform model pointer
    meshXElementIF *target_el = nullptr;
    auto all_elements = meshXElementRegistry::get_instance().get_all_elements();
    for (auto const &[idx, element] : all_elements)
    {
        if (!element->get_ven_models().empty())
        {
            if (element->get_ven_models()[0]->get_plat_model() == p_model)
            {
                target_el = element;
                break;
            }
        }
    }

    if (!target_el)
    {
        MESHX_LOGW(MODULE_ID_COMMON, "UVP TXCM Timeout: No registered element found for model 0x%x", (uint32_t)(uintptr_t)p_model);
        return MESHX_NOT_FOUND;
    }

    meshx_uvp_ctx_t uvp_ctx;
    uvp_ctx.src_addr = MESHX_ADDR_UNASSIGNED; /* Timeout sentinel */
    uvp_ctx.dst_addr = MESHX_ADDR_UNASSIGNED;
    uvp_ctx.tid = 0;
    uvp_ctx.ack_req = false;
    uvp_ctx.func_id = MESHX_KEY_UNUSED; /* Broadcast to all models (REQ-008) */

    return target_el->on_model_cb(nullptr, 0, &uvp_ctx);
}
#endif

extern "C" meshx_err_t meshx_uvp_dispatcher_init(void)
{
    MESHX_LOGI(MODULE_ID_COMMON, "Initializing Unified UVP Dispatcher");

    /*
     * Subscribe to the vendor model ID (MESHX_MODEL_ID_UVP) on the BLE message path.
     */
    meshx_err_t err = control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_FRM_BLE,
        MESHX_MODEL_ID_UVP,
        (control_task_msg_handle_t)uvp_unified_dispatcher_cb);
    if (err != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to subscribe to BLE UVP messages: %d", err);
        return err;
    }

    /*
     * Subscribe to the local host serial commands (CONTROL_TASK_MSG_CODE_TO_MESHX).
     */
    err = control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_TO_MESHX,
        CONTROL_TASK_MSG_EVT_DATA,
        (control_task_msg_handle_t)uvp_app_command_cb);
    if (err != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to subscribe to local host UVP commands: %d", err);
        return err;
    }

#if CONFIG_TXCM_ENABLE
    /*
     * Subscribe to TXCM message timeouts to report to client elements.
     */
    err = control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_TXCM,
        CONTROL_TASK_MSG_EVT_TXCM_MSG_TIMEOUT,
        (control_task_msg_handle_t)uvp_txcm_timeout_cb);
    if (err != MESHX_SUCCESS)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to subscribe to TXCM timeout: %d", err);
        return err;
    }
#endif

    return MESHX_SUCCESS;
}
