/**
 * @copyright Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_uvp_island_handlers.cpp
 * @brief UVP Island Subnet Coexistence Handlers (0xD1..0xDE).
 */

#include "meshx_uvp_registry.hpp"
#include <meshx_c_header.h>

static meshx_err_t uvp_handle_island_coex(const control_task_uvp_meta_t *p_meta,
                                          const meshx_uvp_ctx_t *p_ctx,
                                          const uint8_t *payload,
                                          uint16_t payload_len)
{
    MESHX_UNUSED(p_meta);
    if (!p_ctx || !payload || payload_len < 1U)
    {
        return MESHX_INVALID_ARG;
    }

    uint8_t opcode = payload[0];
    const uint8_t *body = (payload_len > 1U) ? (payload + 1) : payload;
    uint16_t body_len = (uint16_t)(payload_len - 1U);

    return meshx_subnet_coex_handle_uvp(opcode,
                                        p_ctx->src_addr,
                                        p_ctx->dst_addr,
                                        meshx_get_net_key_id(),
                                        body,
                                        body_len);
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
    return MESHX_SUCCESS;
}

