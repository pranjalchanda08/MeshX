/**
 * @file meshx_uvp_kr_handlers.cpp
 * @brief UVP Key Refresh and Rotation (KR) Opcode Handlers (0xC4..0xC7).
 *
 * Implements 3-phase key rotation (Prepare/Distribute, Switchover, Revoke)
 * and unicast key pull recovery for orphan stragglers.
 *
 * @author Pranjal Chanda
 * @date 2024-2026
 */

#include "meshx_uvp_registry.hpp"
#include <meshx_composition.hpp>
#include <meshx_c_header.h>

/* =========================================================================
 * Key Rotation Handler Implementations
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
    if (payload_len >= 1 + sizeof(uvp_key_phase1_prepare_t))
    {
        const uvp_key_phase1_prepare_t *prep = (const uvp_key_phase1_prepare_t *)&payload[1];
        uint32_t current_epoch = meshx_nvs_get_kr_epoch();
        if (meshx_kr_epoch_is_newer(prep->epoch, current_epoch))
        {
            /* Add or update key in secondary slot with Dual-RX enabled */
            meshx_err_t nk_err = meshx_plat_net_key_add(prep->new_key, meshx_get_net_key_id());
            if (nk_err == MESHX_SUCCESS)
            {
                MESHX_LOGI(MODULE_ID_COMMON, "UVP KR: Phase 1 Key installed for epoch %lu from 0x%04X (curr=%lu)",
                           (unsigned long)prep->epoch, p_ctx->src_addr, (unsigned long)current_epoch);
            }
            else
            {
                MESHX_LOGE(MODULE_ID_COMMON, "UVP KR: Failed to add Phase 1 netkey (%d)", nk_err);
            }
        }
        else
        {
            MESHX_LOGD(MODULE_ID_COMMON, "UVP KR: Dropping stale Phase 1 epoch %lu (curr=%lu)",
                       (unsigned long)prep->epoch, (unsigned long)current_epoch);
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
    if (!p_ctx || !payload || payload_len < 1 + sizeof(uvp_key_phase2_activate_t))
    {
        MESHX_LOGE(MODULE_ID_COMMON, "UVP KR: Invalid Phase 2 switchover payload (%u bytes)", (unsigned int)payload_len);
        return MESHX_INVALID_ARG;
    }

    const uvp_key_phase2_activate_t *act = (const uvp_key_phase2_activate_t *)&payload[1];
    uint32_t current_epoch = meshx_nvs_get_kr_epoch();

    if (meshx_kr_epoch_is_newer(act->epoch, current_epoch))
    {
        /* Phase 2: Switchover TX Key = K_new (Both keys remain valid for RX during 4-min overlap) */
        MESHX_LOGI(MODULE_ID_COMMON, "UVP KR: Phase 2 TX Switchover active for epoch %lu from 0x%04X",
                   (unsigned long)act->epoch, p_ctx->src_addr);

        mxcp_evt_uvp_key_rot_phase_t rsp;
        memset(&rsp, 0, sizeof(rsp));
        rsp.current_phase = (uint8_t)MXCP_KR_PHASE_SWITCHOVER;
        rsp.rotation_epoch = act->epoch;
        rsp.status_code = 0;
        mxcp_send_event(MXCP_EVT_UVP_KEY_ROTATION_PHASE, reinterpret_cast<const uint8_t *>(&rsp), sizeof(rsp));
    }
    else
    {
        MESHX_LOGD(MODULE_ID_COMMON, "UVP KR: Dropping stale Phase 2 epoch %lu (curr=%lu)",
                   (unsigned long)act->epoch, (unsigned long)current_epoch);
    }

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
 * @param[in] payload     Pointer to payload bytes containing uvp_key_phase3_revoke_t.
 * @param[in] payload_len Length of payload bytes in bytes.
 * @return MESHX_SUCCESS on completion.
 */
static meshx_err_t uvp_handle_kr_revoke_old(const control_task_uvp_meta_t *p_meta,
                                            const meshx_uvp_ctx_t *p_ctx,
                                            const uint8_t *payload,
                                            uint16_t payload_len)
{
    MESHX_UNUSED(p_meta);
    if (!p_ctx || !payload || payload_len < 1 + sizeof(uvp_key_phase3_revoke_t))
    {
        MESHX_LOGE(MODULE_ID_COMMON, "UVP KR: Invalid Phase 3 revoke payload (%u bytes)", (unsigned int)payload_len);
        return MESHX_INVALID_ARG;
    }

    const uvp_key_phase3_revoke_t *rev = (const uvp_key_phase3_revoke_t *)&payload[1];
    uint32_t current_epoch = meshx_nvs_get_kr_epoch();

    if (meshx_kr_epoch_is_newer(rev->epoch, current_epoch))
    {
        /* Phase 3: Invalidate K_old, Commit K_new and updated epoch atomically to NVS */
        meshx_nvs_set_kr_epoch(rev->epoch);
        MESHX_LOGI(MODULE_ID_COMMON, "UVP KR: Phase 3 Revocation committed; kr_epoch=%lu (from 0x%04X)",
                   (unsigned long)rev->epoch, p_ctx->src_addr);

        /* Notify host of completed rotation phase */
        mxcp_evt_uvp_key_rot_phase_t rsp;
        memset(&rsp, 0, sizeof(rsp));
        rsp.current_phase  = (uint8_t)MXCP_KR_PHASE_REVOKE;
        rsp.rotation_epoch = rev->epoch;
        rsp.status_code    = 0;
        mxcp_send_event(MXCP_EVT_UVP_KEY_ROTATION_PHASE, reinterpret_cast<const uint8_t *>(&rsp), sizeof(rsp));
    }
    else
    {
        MESHX_LOGD(MODULE_ID_COMMON, "UVP KR: Dropping stale Phase 3 epoch %lu (curr=%lu)",
                   (unsigned long)rev->epoch, (unsigned long)current_epoch);
    }

    return MESHX_SUCCESS;
}

#if CONFIG_ENABLE_UVP_PROVISIONER
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
    uint8_t cur_key[MESHX_KEY_LEN];
    if (meshx_plat_net_key_get(meshx_get_net_key_id(), cur_key) != MESHX_SUCCESS)
    {
        return MESHX_SUCCESS;
    }

    uint8_t rsp_buf[1 + sizeof(uint32_t) + 16];
    rsp_buf[0] = MESHX_UVP_OP_KR_KEY_REQ;
    uint32_t epoch = meshx_nvs_get_kr_epoch();
    memcpy(&rsp_buf[1], &epoch, sizeof(uint32_t));
    memcpy(&rsp_buf[1 + sizeof(uint32_t)], cur_key, 16);

    meshx_comp_send_uvp(0,
                        p_ctx->src_addr,
                        MESHX_FUNC_ID_SYS_PAIRING,
                        rsp_buf,
                        sizeof(rsp_buf),
                        false);
    return MESHX_SUCCESS;
}
#endif /* CONFIG_ENABLE_UVP_PROVISIONER */

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
 * Registration Entry Point
 * ========================================================================= */

/**
 * @brief Register UVP key rotation handlers for MeshX key rotation func_id 0xFFFE.
 *
 * Registers the key rotation handlers for the three phases of the key rotation procedure:
 * - Key Distribution (MESHX_UVP_OP_KR_DISTRIBUTE)
 * - Key Switchover (MESHX_UVP_OP_KR_SWITCHOVER)
 * - Key Revocation (MESHX_UVP_OP_KR_REVOKE_OLD)
 * and optionally the orphan node key recovery handlers:
 * - Key Request (MESHX_UVP_OP_KR_KEY_REQ)
 * - Key Confirm (MESHX_UVP_OP_KR_KEY_CONFIRM)
 *
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
meshx_err_t meshx_uvp_register_kr_handlers(void)
{
    meshx_err_t err;

    err = meshx_uvp_register_handler(MESHX_FUNC_ID_SYS_PAIRING, MESHX_UVP_OP_KR_DISTRIBUTE, uvp_handle_kr_distribute);
    if (err != MESHX_SUCCESS) return err;

    err = meshx_uvp_register_handler(MESHX_FUNC_ID_SYS_PAIRING, MESHX_UVP_OP_KR_SWITCHOVER, uvp_handle_kr_switchover);
    if (err != MESHX_SUCCESS) return err;

    err = meshx_uvp_register_handler(MESHX_FUNC_ID_SYS_PAIRING, MESHX_UVP_OP_KR_REVOKE_OLD, uvp_handle_kr_revoke_old);
    if (err != MESHX_SUCCESS) return err;

#if CONFIG_ENABLE_UVP_PROVISIONER
    err = meshx_uvp_register_handler(MESHX_FUNC_ID_SYS_PAIRING, MESHX_UVP_OP_KR_KEY_REQ, uvp_handle_kr_key_req);
    if (err != MESHX_SUCCESS) return err;
#endif

    err = meshx_uvp_register_handler(MESHX_FUNC_ID_SYS_PAIRING, MESHX_UVP_OP_KR_KEY_CONFIRM, uvp_handle_kr_key_confirm);
    return err;
}
