/**
 * @file meshx_uvp_dispatcher.cpp
 * @brief Implementation of the Unified UVP Dispatcher.
 *
 * Dispatches MeshX Unified Vendor Protocol (UVP) messages received over the BLE mesh
 * or from the local host serial interface. Delegates system management opcodes to the
 * modular UVP registry and application data messages to registered element models.
 *
 * @author Pranjal Chanda
 * @date 2024-2026
 */

#include "meshx_uvp_dispatcher.hpp"
#include "meshx_uvp_registry.hpp"
#include <meshx_element_registry.hpp>
#include <meshx_element_class.hpp>
#include <meshx_composition.hpp>
#include <meshx_uvp_model.hpp>
#include <variants/meshx_uvp_element.hpp>
#include <meshx_c_header.h>

#include <map>

/* =========================================================================
 * Defines, Enums, Structs, and Global Variables
 * ========================================================================= */

/**
 * @brief TID Tracking Cache.
 *        Key: Source Address (uint16_t)
 *        Value: Last seen TID (uint8_t)
 */
static std::map<uint16_t, uint8_t> g_tid_cache;

/**
 * @brief Timer handle for orphan straggler key recovery pull.
 */
static meshx_os_timer_t *s_orphan_pull_timer = NULL;

/**
 * @brief Master node unicast address targeted for orphan key recovery.
 */
static uint16_t s_kr_master_addr = 0x0001;

/* --- Forward Declarations of Static Functions --- */
static void uvp_orphan_pull_timer_cb(const meshx_os_timer_t *p_timer);

static meshx_err_t uvp_unified_dispatcher_cb(dev_struct_t *pdev,
                                             control_task_msg_evt_t evt,
                                             void *params,
                                             uint16_t params_len);

static meshx_err_t uvp_app_command_cb(dev_struct_t *pdev,
                                      control_task_msg_evt_t evt,
                                      void *params,
                                      uint16_t params_len);

#if CONFIG_TXCM_ENABLE
static meshx_err_t uvp_txcm_timeout_cb(dev_struct_t *pdev,
                                       control_task_msg_evt_t evt,
                                       void *params,
                                       uint16_t params_len);
#endif

/* =========================================================================
 * Timer Callbacks
 * ========================================================================= */

/**
 * @brief Timer callback triggered when an orphan node needs to pull active network keys.
 *
 * @param[in] p_timer Pointer to the OS timer structure.
 */
static void uvp_orphan_pull_timer_cb(const meshx_os_timer_t *p_timer)
{
    MESHX_UNUSED(p_timer);
    MESHX_LOGD(MODULE_ID_COMMON, "UVP KR: Orphan pull timer fired, querying Master 0x%04X", s_kr_master_addr);

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

/* =========================================================================
 * Core Dispatcher Callbacks
 * ========================================================================= */

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
     * Element 0 Exclusivity Rule (REQ-002 / Design Sheet 01):
     * System management functional endpoints (0xFFFE discovery, 0xFFFF pairing)
     * are held strictly and exclusively on Element 0. Secondary elements drop management frames.
     */
    if (p_meta->rx_el_id != 0 &&
        (uvp_ctx.func_id == MESHX_FUNC_ID_SYS_DISCOVERY || uvp_ctx.func_id == MESHX_FUNC_ID_SYS_PAIRING))
    {
        MESHX_LOGW(MODULE_ID_COMMON, "UVP Dispatcher: Dropping management func 0x%04X on secondary element %u (Element 0 exclusive)",
                   uvp_ctx.func_id, p_meta->rx_el_id);
        return MESHX_ERR_BLOCKED_ADDR;
    }

    /* Extract Opcode / Message Type */
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
        /*
         * RFC 1982 Modular Distance Arithmetic:
         * Handles monotonic sequence advancement AND 32-bit integer rollover (0xFFFFFFFF -> 0x00000001).
         */
        int32_t delta = static_cast<int32_t>(incoming_epoch - local_epoch);

        if (delta > 1 && delta <= MESHX_KR_MAX_FORWARD_DRIFT)
        {
            /* Node missed one or more complete key rotation epochs: mark desync and schedule recovery pull */
            MESHX_LOGW(MODULE_ID_COMMON, "UVP KR: Major epoch drift detected (local=%lu, packet=%lu, delta=%ld). Scheduling orphan pull.",
                       (unsigned long)local_epoch, (unsigned long)incoming_epoch, static_cast<long>(delta));
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
        else if (delta < 0 || delta > MESHX_KR_MAX_FORWARD_DRIFT)
        {
            MESHX_LOGD(MODULE_ID_COMMON, "UVP KR: Stale or wild epoch received (local=%lu, packet=%lu, delta=%ld). Ignoring for drift.",
                       (unsigned long)local_epoch, (unsigned long)incoming_epoch, static_cast<long>(delta));
        }
    }

    /*
     * Delegate System Management and Pairing Opcodes to Dynamic UVP Registry.
     */
    bool is_sys_mgmt_opcode = (msg_type >= MESHX_UVP_OP_RPR_SCAN_REQ && msg_type <= MESHX_UVP_OP_REDUNDANCY_CLAIM_ACK);

    if (uvp_ctx.func_id == MESHX_FUNC_ID_SYS_DISCOVERY ||
        uvp_ctx.func_id == MESHX_FUNC_ID_SYS_PAIRING ||
        is_sys_mgmt_opcode)
    {
        if (payload_len >= sizeof(uint8_t))
        {
            return meshx_uvp_dispatch_handler(uvp_ctx.func_id,
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
    MESHX_LOGI(MODULE_ID_COMMON, "Initializing Unified UVP Dispatcher and registering modular handlers");

    /* Register all modular UVP opcode handlers */
    meshx_uvp_register_kr_handlers();
    meshx_uvp_register_rpr_handlers();
    meshx_uvp_register_reheal_handlers();
    meshx_uvp_register_pairing_handlers();
    meshx_uvp_register_discovery_handlers();
    meshx_uvp_register_redundancy_handlers();

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
