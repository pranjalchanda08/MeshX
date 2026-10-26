/**
 * @file meshx_uvp_discovery_handlers.cpp
 * @brief UVP In-Mesh Discovery Opcode Handlers (func_id 0xFFFE).
 *
 * Implements MESHX_SYS_CMD_DISCOVERY_QUERY and MESHX_SYS_CMD_EPOCH_PING
 * for local capability reporting and link epoch auditing.
 *
 * @author Pranjal Chanda
 * @date 2024-2026
 */

#include "meshx_uvp_registry.hpp"
#include <meshx_composition.hpp>
#include <meshx_c_header.h>

/* =========================================================================
 * Discovery Handler Implementations
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

/* =========================================================================
 * Registration Entry Point
 * ========================================================================= */

/**
 * @brief Register UVP discovery handlers for MeshX discovery func_id 0xFFFE.
 *
 * Registers the in-mesh discovery query handler and epoch ping handler for
 * topology discovery and link epoch synchronization.
 *
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
meshx_err_t meshx_uvp_register_discovery_handlers(void)
{
    meshx_err_t err = meshx_uvp_register_handler(MESHX_FUNC_ID_SYS_DISCOVERY,
                                                 MESHX_SYS_CMD_DISCOVERY_QUERY,
                                                 uvp_handle_discovery_query);
    if (err != MESHX_SUCCESS) return err;

    err = meshx_uvp_register_handler(MESHX_FUNC_ID_SYS_DISCOVERY,
                                     MESHX_SYS_CMD_EPOCH_PING,
                                     uvp_handle_epoch_ping_discovery);
    return err;
}
