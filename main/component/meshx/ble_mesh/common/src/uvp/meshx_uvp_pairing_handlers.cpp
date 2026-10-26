/**
 * @file meshx_uvp_pairing_handlers.cpp
 * @brief UVP In-Mesh Pairing and Unpairing Opcode Handlers (func_id 0xFFFF).
 *
 * Implements pair request/response, topology upgrade, unpair, reset notify,
 * epoch audit ping, and autonomous proximity announcement.
 *
 * @author Pranjal Chanda
 * @date 2024-2026
 */

#include "meshx_uvp_registry.hpp"
#include <meshx_composition.hpp>
#include <meshx_c_header.h>

/* =========================================================================
 * Pairing Handler Implementations
 * ========================================================================= */

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
 * Registration Entry Point
 * ========================================================================= */

/**
 * @brief Register UVP pairing handlers with the UVP module.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_uvp_register_pairing_handlers(void)
{
    meshx_err_t err;

    err = meshx_uvp_register_handler(MESHX_FUNC_ID_SYS_PAIRING, MESHX_SYS_CMD_PAIR_REQUEST, uvp_handle_pair_request);
    if (err != MESHX_SUCCESS) return err;

    err = meshx_uvp_register_handler(MESHX_FUNC_ID_SYS_PAIRING, MESHX_SYS_RSP_PAIR_RESPONSE, uvp_handle_pair_response);
    if (err != MESHX_SUCCESS) return err;

    err = meshx_uvp_register_handler(MESHX_FUNC_ID_SYS_PAIRING, MESHX_SYS_CMD_TOPOLOGY_UPGRADE, uvp_handle_topology_upgrade);
    if (err != MESHX_SUCCESS) return err;

    err = meshx_uvp_register_handler(MESHX_FUNC_ID_SYS_PAIRING, MESHX_SYS_CMD_UNPAIR_REQUEST, uvp_handle_unpair_request);
    if (err != MESHX_SUCCESS) return err;

    err = meshx_uvp_register_handler(MESHX_FUNC_ID_SYS_PAIRING, MESHX_SYS_RSP_UNPAIR_RESPONSE, uvp_handle_unpair_response);
    if (err != MESHX_SUCCESS) return err;

    err = meshx_uvp_register_handler(MESHX_FUNC_ID_SYS_PAIRING, MESHX_SYS_NOTIFY_NODE_RESET, uvp_handle_notify_node_reset);
    if (err != MESHX_SUCCESS) return err;

    err = meshx_uvp_register_handler(MESHX_FUNC_ID_SYS_PAIRING, MESHX_SYS_CMD_EPOCH_PING, uvp_handle_epoch_ping_pairing);
    if (err != MESHX_SUCCESS) return err;

    err = meshx_uvp_register_handler(MESHX_FUNC_ID_SYS_PAIRING, MESHX_SYS_CMD_PAIR_ANNOUNCE, uvp_handle_pair_announce);
    return err;
}
