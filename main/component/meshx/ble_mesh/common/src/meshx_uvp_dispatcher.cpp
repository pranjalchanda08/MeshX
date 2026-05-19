/**
 * @file meshx_uvp_dispatcher.cpp
 * @brief Implementation of the Unified UVP Dispatcher.
 */

#include "meshx_uvp_dispatcher.hpp"
#include "meshx_element_registry.hpp"
#include "meshx_element_class.hpp"
#include <meshx_api.h>

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

    /*
     * Verify that the element type matches the UVP type_id.
     * This provides an extra layer of safety.
     */
    if (element->get_element_variant() != (meshx_element_type_t)uvp_header->type_id) {
         MESHX_LOGW(MODULE_ID_COMMON, "UVP Dispatcher: Type mismatch! EL[%d] variant=%d, UVP type_id=%d",
                   p_meta->rx_el_id, (int)element->get_element_variant(), (int)uvp_header->type_id);
    }

    /* Populate UVP Routing Context */
    meshx_uvp_ctx_t uvp_ctx = {
        .src_addr = src_addr,
        .dst_addr = p_meta->dst_addr,
        .tid      = uvp_header->tid,
        .ack_req  = (bool)uvp_header->ack_req
    };

    /*
     * Pass the actual TLV payload to the element's callback.
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

    if (!params || params_len < sizeof(meshx_app_api_msg_t)) {
        MESHX_LOGE(MODULE_ID_COMMON, "UVP App Command Dispatcher: Invalid parameters!");
        return MESHX_INVALID_ARG;
    }

    meshx_app_api_msg_t *p_msg = (meshx_app_api_msg_t *)params;
    uint16_t el_id = p_msg->msg_type_u.element_msg.element_id;
    uint16_t msg_len = p_msg->msg_type_u.element_msg.msg_len;

    /* Find the targeted element in the registry using the element index */
    meshXElementIF* element = meshXElementRegistry::get_instance().find_element(el_id);
    if (!element) {
        MESHX_LOGW(MODULE_ID_COMMON, "UVP App Command Dispatcher: No element registered at index %d", el_id);
        return MESHX_NOT_FOUND;
    }

    /* Populate a dummy UVP routing context to satisfy the element callbacks */
    meshx_uvp_ctx_t uvp_ctx = {
        .src_addr = 0x0001, /* Host identifier */
        .dst_addr = 0x0000,
        .tid      = 0,
        .ack_req  = false
    };

    /* Pass the local serial parameters payload directly to the element's callback */
    return element->on_model_cb(p_msg->data, msg_len, &uvp_ctx);
}

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

    return MESHX_SUCCESS;
}
