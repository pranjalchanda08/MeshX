/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_uvp_island_handlers.cpp
 * @brief UVP Island Subnet Coexistence Handlers (0xD1..0xDE).
 */

#include "meshx_uvp_registry.hpp"
#include <meshx_c_header.h>

/* =========================================================================
 * Island Subnet Coexistence integration bindings
 *
 * The coexistence facade is a self-contained control-plane library. These
 * bindings connect it to the live stack:
 *   1. UVP TX injection (heartbeats, claims, NACKs, discovery reports, ...).
 *   2. Periodic tick (Stage 2 pump, heartbeat cadence, 12s failover watchdog).
 *   3. Manufacturer-data scan observer (foreign-subnet boundary witness).
 * ========================================================================= */

#define MESHX_ISLAND_TICK_PERIOD_MS 250U

static meshx_os_timer_t *s_island_tick_timer = NULL;
static uint32_t          s_island_now_ms     = 0U;

/**
 * @brief Inject an island-subnet UVP frame through the local composition model.
 *
 * @param[in] net_idx     Ingress NetKey index (advisory).
 * @param[in] dst_addr    Destination unicast, group or virtual address.
 * @param[in] opcode      UVP opcode (0xD1..0xDE).
 * @param[in] payload     Wire payload whose leading byte is @p opcode.
 * @param[in] payload_len Payload length in bytes.
 * @param[in] ack_req     Request a UVP acknowledgement.
 * @param[in] user_ctx    Unused user context.
 * @return MESHX_SUCCESS on success, or an error code.
 */
static meshx_err_t island_uvp_tx(uint16_t net_idx,
                                 uint16_t dst_addr,
                                 uint8_t opcode,
                                 const uint8_t *payload,
                                 uint16_t payload_len,
                                 bool ack_req,
                                 void *user_ctx)
{
    MESHX_UNUSED(user_ctx);
    MESHX_UNUSED(opcode);
    if (!payload || payload_len < 1U)
    {
        return MESHX_INVALID_ARG;
    }

    return meshx_comp_send_uvp_on_subnet(net_idx,
                                         0,
                                         dst_addr,
                                         MESHX_FUNC_ID_SYS_PAIRING,
                                         payload,
                                         payload_len,
                                         ack_req);
}

/**
 * @brief Periodic coexistence tick driving heartbeat/watchdog and Stage 2 egress.
 *
 * @param[in] p_timer Timer handle.
 */
static void island_tick_timer_cb(const meshx_os_timer_t *p_timer)
{
    MESHX_UNUSED(p_timer);
    unsigned int now_ms = 0U;
    if (meshx_rtos_get_sys_time(&now_ms) == MESHX_SUCCESS)
    {
        s_island_now_ms = (uint32_t)now_ms;
    }
    else
    {
        s_island_now_ms += MESHX_ISLAND_TICK_PERIOD_MS;
    }
    meshx_err_t tick_err = meshx_subnet_coex_tick(s_island_now_ms);
    if (tick_err != MESHX_SUCCESS && tick_err != MESHX_INVALID_STATE)
    {
        MESHX_LOGW(MODULE_ID_COMMON, "Island coex tick failed: %d", (int)tick_err);
    }
}

/**
 * @brief Forward scanned manufacturer data to the boundary witness detector.
 *
 * @param[in] cid         Company Identifier.
 * @param[in] data        Manufacturer data payload.
 * @param[in] len         Payload length in bytes.
 * @param[in] rssi        Measured RSSI in dBm.
 */
static void island_mfg_data_observer(uint16_t cid, const uint8_t *data, uint16_t len, int8_t rssi)
{
    meshx_subnet_coex_on_beacon_rx(cid, data, len, rssi);
}

static meshx_err_t uvp_handle_island_coex(const control_task_uvp_meta_t *p_meta,
                                          const meshx_uvp_ctx_t *p_ctx,
                                          const uint8_t *payload,
                                          uint16_t payload_len)
{
    if (!p_ctx || !payload || payload_len < 1U)
    {
        return MESHX_INVALID_ARG;
    }

    uint8_t opcode = payload[0];
    uint16_t rx_net_idx = p_meta ? p_meta->net_idx : meshx_get_net_key_id();

    return meshx_subnet_coex_handle_uvp(opcode,
                                        p_ctx->src_addr,
                                        p_ctx->dst_addr,
                                        rx_net_idx,
                                        payload,
                                        payload_len);
}

meshx_err_t meshx_uvp_register_island_handlers(void)
{
    static const uint8_t k_island_opcodes[] = {
        MESHX_UVP_OP_SUBNET_HEARTBEAT,
        MESHX_UVP_OP_SUBNET_TAKEOVER_CLAIM,
        MESHX_UVP_OP_SUBNET_CANDIDATE_RPT,
        MESHX_UVP_OP_SUBNET_BRIDGE_INVITE,
        MESHX_UVP_OP_SUBNET_NAT_SYNC,
        MESHX_UVP_OP_SUBNET_ROUTE_ADV,
        MESHX_UVP_OP_SUBNET_ROUTER_NACK,
        MESHX_UVP_OP_SUBNET_EPOCH_SYNC,
        MESHX_UVP_OP_SUBNET_NACK_ACK,
        MESHX_UVP_OP_SUBNET_INVITE_ACK,
        MESHX_UVP_OP_SUBNET_DEMOTE,
        MESHX_UVP_OP_SUBNET_DISCOVER_REQ,
        MESHX_UVP_OP_SUBNET_DISCOVER_RSP,
        MESHX_UVP_OP_QUERY_SUBNET_DIRECTORY,
        MESHX_UVP_OP_SUBNET_DATA_TUNNEL,
    };

    for (size_t i = 0; i < sizeof(k_island_opcodes) / sizeof(k_island_opcodes[0]); i++)
    {
        meshx_err_t err = meshx_uvp_register_handler(MESHX_FUNC_ID_SYS_PAIRING,
                                                     k_island_opcodes[i],
                                                     uvp_handle_island_coex);
        if (err != MESHX_SUCCESS)
        {
            return err;
        }
    }

    /* Bind UVP TX injection so the coexistence engine can emit control frames. */
    meshx_subnet_coex_register_uvp_tx(island_uvp_tx, nullptr);

    /* Start the periodic tick (heartbeat cadence, watchdog, Stage 2 pump). */
    if (!s_island_tick_timer)
    {
        meshx_err_t terr = meshx_os_timer_create("island_tick",
                                                 MESHX_ISLAND_TICK_PERIOD_MS,
                                                 true,
                                                 island_tick_timer_cb,
                                                 &s_island_tick_timer);
        if (terr == MESHX_SUCCESS && s_island_tick_timer)
        {
            meshx_err_t start_err = meshx_os_timer_start(s_island_tick_timer);
            if (start_err != MESHX_SUCCESS)
            {
                return start_err;
            }
        }
    }

    /* Register the foreign-subnet boundary witness scan observer. */
    meshx_err_t mfg_err = meshx_plat_ble_register_mfg_data_cb(island_mfg_data_observer);
    if (mfg_err != MESHX_SUCCESS && mfg_err != MESHX_NOT_SUPPORTED)
    {
        return mfg_err;
    }

    return MESHX_SUCCESS;
}

