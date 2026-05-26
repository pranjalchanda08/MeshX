/**
 * @file meshx_uvp_dispatcher.cpp
 * @brief Implementation of the Unified UVP Dispatcher.
 */

#include "meshx_uvp_dispatcher.hpp"
#include <meshx_api.h>
#include "meshx_element_registry.hpp"
#include "meshx_element_class.hpp"
#include <variants/meshx_uvp_element.hpp>

#if CONFIG_TXCM_ENABLE
#include <meshx_txcm.h>
#endif

#include <map>

/**
 * @brief TID Tracking Cache.
 *        Key: Source Address (uint16_t)
 *        Value: Last seen TID (uint8_t)
 */
static std::map<uint16_t, uint8_t> g_tid_cache;

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

    if (!pdev || !params || params_len < sizeof(control_task_uvp_meta_t)) {
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
    if (g_tid_cache.count(src_addr) && g_tid_cache[src_addr] == uvp_header->tid) {
        MESHX_LOGD(MODULE_ID_COMMON, "UVP Dispatcher: Dropping duplicate TID %d from 0x%04x",
                   uvp_header->tid, src_addr);
        return MESHX_SUCCESS; // Already processed
    }
    g_tid_cache[src_addr] = uvp_header->tid;

    /* Find the targeted element in the registry using the locally-resolved receiving element index */
    meshXElementIF* element = meshXElementRegistry::get_instance().find_element(p_meta->rx_el_id);
    if (!element) {
        MESHX_LOGW(MODULE_ID_COMMON, "UVP Dispatcher: No element registered at index %d", p_meta->rx_el_id);
        return MESHX_NOT_FOUND;
    }

#if CONFIG_TXCM_ENABLE
    /* Notify TXCM that we received an ACK to clear the queue.
     * In UVP, if the receiving element is a CLIENT, the incoming message is an ACK/Status update
     * sent from the SERVER to this CLIENT. */
    if (element->get_element_type() == meshxElementType::MESHX_ELEMENT_TYPE_CLIENT) {
        meshx_txcm_request_send(MESHX_TXCM_SIG_ACK, src_addr, nullptr, 0, nullptr);
    }
#endif

    /*
     * Verify that the element type matches the expected incoming UVP message types.
     * A CLIENT expects messages from a SERVER, and vice versa.
     * Therefore, the receiver's variant (e.g. Relay Client) should NOT match the sender's variant (e.g. Relay Server).
     */
    if ((uint16_t)element->get_element_variant() == uvp_header->type_id) {
         MESHX_LOGW(MODULE_ID_COMMON, "UVP Dispatcher: Role mismatch! Sender and receiver have same variant. EL[%d] variant=%d, UVP type_id=%d",
                   p_meta->rx_el_id, (int)element->get_element_variant(), (int)uvp_header->type_id);
    }

    /* Populate UVP Routing Context */
    meshx_uvp_ctx_t uvp_ctx;
    uvp_ctx.src_addr = src_addr;
    uvp_ctx.dst_addr = p_meta->dst_addr;
    uvp_ctx.tid      = uvp_header->tid;
    uvp_ctx.ack_req  = (bool)uvp_header->ack_req;

    /*
     * REQ-004: Strip the 2-byte func_id wire prefix from the payload.
     * Wire layout: [ func_id (2 B, LE) | app_payload (N B) ]
     * If payload is too short for the prefix, fall back to func_id=0.
     */
    if (payload_len >= MESHX_UVP_FUNC_ID_PREFIX_SZ) {
        const uint8_t *raw = static_cast<const uint8_t*>(payload);
        uvp_ctx.func_id = (uint16_t)(raw[0] | ((uint16_t)raw[1] << 8u));
        payload     = (uint8_t*)payload + MESHX_UVP_FUNC_ID_PREFIX_SZ;
        payload_len = (uint16_t)(payload_len - MESHX_UVP_FUNC_ID_PREFIX_SZ);
    } else {
        uvp_ctx.func_id = 0x0000u; /* Legacy / malformed — default to func 0 */
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
 */
static meshx_err_t uvp_app_command_cb(dev_struct_t *pdev,
                                      control_task_msg_evt_t evt,
                                      void *params,
                                      uint16_t params_len)
{
    MESHX_UNUSED(pdev);
    MESHX_UNUSED(evt);

    if (!params || params_len < sizeof(meshx_msg_data_t)) {
        MESHX_LOGE(MODULE_ID_COMMON, "UVP App Command Dispatcher: Invalid parameters!");
        return MESHX_INVALID_ARG;
    }

    meshx_msg_data_t *p_msg = (meshx_msg_data_t *)params;
    uint16_t el_id = p_msg->element_id;
    uint16_t msg_len = p_msg->payload_len;

    /* Find the targeted element in the registry using the element index */
    meshXElementIF* element = meshXElementRegistry::get_instance().find_element(el_id);
    if (!element) {
        MESHX_LOGW(MODULE_ID_COMMON, "UVP App Command Dispatcher: No element registered at index %d", el_id);
        return MESHX_NOT_FOUND;
    }

    /* Populate UVP routing context — host command path (REQ-003) */
    meshx_uvp_ctx_t uvp_ctx;
    uvp_ctx.src_addr = 0x0001u; /* Host identifier */
    uvp_ctx.dst_addr = 0x0000u;
    uvp_ctx.tid      = 0;
    uvp_ctx.ack_req  = false;
    uvp_ctx.func_id  = p_msg->func_id; /* Propagate explicit func_id (REQ-003) */

    /* Pass the local serial parameters payload directly to the element's callback */
    return element->on_model_cb(p_msg->payload, msg_len, &uvp_ctx);
}

#if CONFIG_TXCM_ENABLE
static meshx_err_t uvp_txcm_timeout_cb(dev_struct_t *pdev,
                                       control_task_msg_evt_t evt,
                                       void *params,
                                       uint16_t params_len)
{
    MESHX_UNUSED(pdev);
    MESHX_UNUSED(evt);

    if (!params || params_len < sizeof(meshXUVPModel::uvp_txcm_param_t)) {
        MESHX_LOGE(MODULE_ID_COMMON, "UVP TXCM Timeout Callback: Invalid parameters!");
        return MESHX_INVALID_ARG;
    }

    const auto *param = static_cast<const meshXUVPModel::uvp_txcm_param_t*>(params);
    void *p_model = param->p_model;

    // Find the targeted element in the registry by matching the platform model pointer
    meshXElementIF* target_el = nullptr;
    auto all_elements = meshXElementRegistry::get_instance().get_all_elements();
    for (auto const& [idx, element] : all_elements) {
        if (!element->get_ven_models().empty()) {
            if (element->get_ven_models()[0]->get_plat_model() == p_model) {
                target_el = element;
                break;
            }
        }
    }

    if (!target_el) {
        MESHX_LOGW(MODULE_ID_COMMON, "UVP TXCM Timeout: No registered element found for model 0x%x", (uint32_t)(uintptr_t)p_model);
        return MESHX_NOT_FOUND;
    }

    meshx_uvp_ctx_t uvp_ctx;
    uvp_ctx.src_addr = MESHX_ADDR_UNASSIGNED; /* Timeout sentinel */
    uvp_ctx.dst_addr = 0x0000u;
    uvp_ctx.tid      = 0;
    uvp_ctx.ack_req  = false;
    uvp_ctx.func_id  = 0xFFFFu; /* Broadcast to all models (REQ-008) */

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
        (control_task_msg_handle_t)uvp_unified_dispatcher_cb
    );
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to subscribe to BLE UVP messages: %d", err);
        return err;
    }

    /*
     * Subscribe to the local host serial commands (CONTROL_TASK_MSG_CODE_TO_MESHX).
     */
    err = control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_TO_MESHX,
        CONTROL_TASK_MSG_EVT_DATA,
        (control_task_msg_handle_t)uvp_app_command_cb
    );
    if (err != MESHX_SUCCESS) {
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
        (control_task_msg_handle_t)uvp_txcm_timeout_cb
    );
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to subscribe to TXCM timeout: %d", err);
        return err;
    }
#endif

    return MESHX_SUCCESS;
}

