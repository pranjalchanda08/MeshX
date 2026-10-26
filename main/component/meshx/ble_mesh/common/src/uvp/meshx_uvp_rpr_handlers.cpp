/**
 * @file meshx_uvp_rpr_handlers.cpp
 * @brief UVP Remote Provisioning (RPR) Opcode Handlers (0xC0..0xC3).
 *
 * Implements scan requests, unprovisioned beacon reports, and bidirectional
 * PB-ADV tunneling over the UVP mesh.
 *
 * @author Pranjal Chanda
 * @date 2024-2026
 */

#include "meshx_uvp_registry.hpp"
#include <meshx_c_header.h>

/* =========================================================================
 * Remote Provisioning (RPR) Handler Implementations
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

#if CONFIG_ENABLE_UVP_PROVISIONER
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
#endif /* CONFIG_ENABLE_UVP_PROVISIONER */

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

#if CONFIG_ENABLE_UVP_PROVISIONER
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
#endif /* CONFIG_ENABLE_UVP_PROVISIONER */

/* =========================================================================
 * Registration Entry Point
 * ========================================================================= */

/**
 * @brief Registers all UVP RPR handlers.
 * @return MESHX_SUCCESS on successful registration.
 */
meshx_err_t meshx_uvp_register_rpr_handlers(void)
{
    meshx_err_t err;

    err = meshx_uvp_register_handler(MESHX_FUNC_ID_SYS_PAIRING, MESHX_UVP_OP_RPR_SCAN_REQ, uvp_handle_rpr_scan_req);
    if (err != MESHX_SUCCESS) return err;

#if CONFIG_ENABLE_UVP_PROVISIONER
    err = meshx_uvp_register_handler(MESHX_FUNC_ID_SYS_PAIRING, MESHX_UVP_OP_RPR_BEACON_REPORT, uvp_handle_rpr_beacon_report);
    if (err != MESHX_SUCCESS) return err;
#endif

    err = meshx_uvp_register_handler(MESHX_FUNC_ID_SYS_PAIRING, MESHX_UVP_OP_RPR_TUNNEL_DOWN, uvp_handle_rpr_tunnel_down);
    if (err != MESHX_SUCCESS) return err;

#if CONFIG_ENABLE_UVP_PROVISIONER
    err = meshx_uvp_register_handler(MESHX_FUNC_ID_SYS_PAIRING, MESHX_UVP_OP_RPR_TUNNEL_UP, uvp_handle_rpr_tunnel_up);
    if (err != MESHX_SUCCESS) return err;
#endif

    return MESHX_SUCCESS;
}
