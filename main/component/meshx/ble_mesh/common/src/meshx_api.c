/**
 * @file meshx_api.c
 * @brief Implementation of the BLE Mesh application API.
 *
 * This file contains the implementation of the BLE Mesh application API.
 * It includes functions to send messages to the BLE Mesh application and
 * register the BLE Mesh application callback.
 *
 * @author Pranjal Chanda
 */
#include <meshx_api.h>
#include "meshx_mxcp.h"
#include <stdlib.h>
#include "interface/rtos/meshx_rtos_utils.h"

#define MESSAGE_BUFF_CLEAR(buff)        memset(&buff, 0, sizeof(buff))

static struct{

    meshx_app_data_cb_t app_data_cb;    /*< BLE Mesh application data callback */
    meshx_app_ctrl_cb_t app_ctrl_cb;    /*< BLE Mesh application control callback */
    meshx_app_api_msg_t msg_buff;       /*< BLE Mesh application message buffer */
}meshx_api_ctrl;

/*
 * @brief Control task handler for BLE Mesh application messages.
 *
 * This function handles BLE Mesh application messages.
 *
 * @param[in] pdev       Pointer to the device structure.
 * @param[in] evt        Event type.
 * @param[in] params     Pointer to the message parameters.
 * @param[in] params_len Length of the message parameters.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
static meshx_err_t meshx_api_control_task_handler(dev_struct_t *pdev, control_task_msg_evt_t evt, void *params, uint16_t params_len)
{
    const meshx_app_api_msg_t *msg = (const meshx_app_api_msg_t *)params;

    meshx_err_t err = MESHX_SUCCESS;

    if (!pdev)
        return MESHX_INVALID_ARG;

    if(evt == CONTROL_TASK_MSG_EVT_DATA)
        err = meshx_api_ctrl.app_data_cb ? meshx_api_ctrl.app_data_cb(&msg->msg_type_u.element_msg,
                (const meshx_data_payload_t*) msg->data) : MESHX_SUCCESS;

    else
        err = meshx_api_ctrl.app_ctrl_cb ? meshx_api_ctrl.app_ctrl_cb(&msg->msg_type_u.ctrl_msg,
                (const meshx_ctrl_payload_t*) msg->data) : MESHX_SUCCESS;

    return err;
}

/*
 * @brief Prepares a message to be sent to the BLE Mesh application.
 *
 * This function prepares a message to be sent to the BLE Mesh application.
 *
 * @param[in] element_id    The element ID.
 * @param[in] element_type  The element type.
 * @param[in] func_id       The function ID.
 * @param[in] msg_len       The message length.
 * @param[in] msg           Pointer to the message.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
static meshx_err_t meshx_prepare_data_message(uint16_t element_id, uint16_t element_type, uint16_t func_id, uint16_t msg_len, const void *msg)
{
    if (!msg || msg_len > MESHX_APP_API_MSG_MAX_SIZE)
        return MESHX_INVALID_ARG;

    MESSAGE_BUFF_CLEAR(meshx_api_ctrl.msg_buff);

    meshx_api_ctrl.msg_buff.msg_type_u.element_msg.func_id      = func_id;
    meshx_api_ctrl.msg_buff.msg_type_u.element_msg.msg_len      = msg_len;
    meshx_api_ctrl.msg_buff.msg_type_u.element_msg.element_id   = element_id;
    meshx_api_ctrl.msg_buff.msg_type_u.element_msg.element_type = element_type;

    if (msg && msg_len > 0)
    {
        memcpy(meshx_api_ctrl.msg_buff.data, msg, msg_len);
    }

    return MESHX_SUCCESS;
}

/*
 * @brief Prepares a control message to be sent to the BLE Mesh application.
 *
 * This function prepares a control message to be sent to the BLE Mesh application.
 *
 * @param[in] evt       The event ID.
 * @param[in] msg_len   The message length.
 * @param[in] msg       Pointer to the message.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
static meshx_err_t meshx_prepare_ctrl_message(meshx_ctrl_evt_t evt, uint16_t msg_len, const void *msg)
{
    if (msg_len > MESHX_APP_API_MSG_MAX_SIZE)
        return MESHX_INVALID_ARG;

    MESSAGE_BUFF_CLEAR(meshx_api_ctrl.msg_buff);

    meshx_api_ctrl.msg_buff.msg_type_u.ctrl_msg.evt_id = evt;
    meshx_api_ctrl.msg_buff.msg_type_u.ctrl_msg.reserved = 0;

    if (msg && msg_len > 0)
    {
        memcpy(meshx_api_ctrl.msg_buff.data, msg, msg_len);
    }

    return MESHX_SUCCESS;
}

/*
 * @brief Send element data upwards to App and Host MCU (Data Path - REQ-001, REQ-002, REQ-003).
 *
 * This function handles upstream telemetry from an element instance:
 * 1. Serializes payload with canonical 8-byte meshx_data_msg_hdr_t directly to the
 *    hosted serial interface (MXCP_EVT_EL_DATA_RX_NOTIFY 0x10) for external Host MCUs.
 * 2. Packages message into control task queue (CONTROL_TASK_MSG_CODE_TO_APP /
 *    CONTROL_TASK_MSG_EVT_DATA) for local registered application callbacks.
 *
 * @param[in] hdr       Pointer to canonical data message header (element_id, element_type, func_id, msg_len).
 * @param[in] payload   Pointer to element data payload (opaque buffer or meshx_data_payload_t).
 *
 * @return MESHX_SUCCESS on success, or an appropriate error code (e.g. MESHX_INVALID_ARG, MESHX_NO_MEM).
 */
meshx_err_t meshx_data_send_to_app(const meshx_data_msg_hdr_t *hdr, const void *payload)
{
    if (!hdr) {
        return MESHX_INVALID_ARG;
    }

    uint32_t buf_size = sizeof(meshx_data_msg_hdr_t) + hdr->msg_len;
    uint8_t *buf = NULL;
    meshx_err_t err = MESHX_MALLOC(&buf, buf_size);
    if (err != MESHX_SUCCESS || !buf) {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to allocate memory for app msg");
        return MESHX_NO_MEM;
    }

    memcpy(buf, hdr, sizeof(meshx_data_msg_hdr_t));
    if (hdr->msg_len > 0 && payload != NULL) {
        memcpy(buf + sizeof(meshx_data_msg_hdr_t), payload, hdr->msg_len);
    }
    mxcp_send_event(MXCP_EVT_EL_DATA_RX_NOTIFY, buf, (uint8_t)buf_size);
    MESHX_FREE(&buf);

    err = meshx_prepare_data_message(hdr->element_id, hdr->element_type, hdr->func_id, hdr->msg_len, payload);
    if (err) {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to create message: (0x%x)", err);
        return err;
    }

    err = control_task_msg_publish(CONTROL_TASK_MSG_CODE_TO_APP, CONTROL_TASK_MSG_EVT_DATA,
                                   &meshx_api_ctrl.msg_buff, sizeof(meshx_app_api_msg_t));
    if (err) {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to send message to app: (0x%x)", err);
    }
    return err;
}

/*
 * @brief Send element command downwards from App or Host MCU into the MeshX Element Engine.
 *
 * Enqueues a command destined for a local element via the control task
 * (CONTROL_TASK_MSG_CODE_TO_MESHX / CONTROL_TASK_MSG_EVT_DATA), routing directly
 * to the UVP Command Dispatcher and targeted model callback.
 *
 * @param[in] hdr       Pointer to canonical data message header.
 * @param[in] payload   Pointer to element command payload buffer.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_data_send_to_element(const meshx_data_msg_hdr_t *hdr, const void *payload)
{
    if (!hdr) {
        return MESHX_INVALID_ARG;
    }

    meshx_err_t err = meshx_prepare_data_message(hdr->element_id, hdr->element_type, hdr->func_id, hdr->msg_len, payload);
    if (err) {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to create message: (0x%x)", err);
        return err;
    }

    err = control_task_msg_publish(CONTROL_TASK_MSG_CODE_TO_MESHX, CONTROL_TASK_MSG_EVT_DATA,
                                   &meshx_api_ctrl.msg_buff, sizeof(meshx_app_api_msg_t));
    if (err) {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to send message to element: (0x%x)", err);
    }

    return err;
}

/*
 * @brief Dispatch a system control event to registered app callback and MXCP (Control Path).
 *
 * Sends lifecycle, provisioning, and pairing notifications through both:
 * 1. Serial MXCP event wire notifications to external Host MCU.
 * 2. Control task queue (CONTROL_TASK_MSG_CODE_TO_APP / CONTROL_TASK_MSG_EVT_CTRL)
 *    to invoke the local registered application control callback.
 *
 * @param[in] evt_id       System control event identifier (meshx_ctrl_evt_t).
 * @param[in] status       Operation status or result code (0 = success).
 * @param[in] payload      Pointer to control event payload structure.
 * @param[in] payload_len  Length of payload in bytes.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_ctrl_send_event(meshx_ctrl_evt_t evt_id, uint16_t status, const void *payload, uint16_t payload_len)
{
    MESHX_UNUSED(status);
    mxcp_evt_id_t mxcp_evt;
    switch (evt_id) {
        case MESHX_CTRL_EVT_NODE_RESET:
            mxcp_evt = MXCP_EVT_NODE_RESET_IND;
            break;
        case MESHX_CTRL_EVT_PROV_COMP:
            mxcp_evt = MXCP_EVT_PROV_COMP;
            break;
        case MESHX_CTRL_EVT_PROV_FAILED:
            mxcp_evt = MXCP_EVT_PROV_FAILED;
            break;
        case MESHX_CTRL_EVT_PROV_START:
            mxcp_evt = MXCP_EVT_PROV_START;
            break;
        case MESHX_CTRL_EVT_IDENTIFY_START:
            mxcp_evt = MXCP_EVT_IDENTIFY_START;
            break;
        case MESHX_CTRL_EVT_IDENTIFY_STOP:
            mxcp_evt = MXCP_EVT_IDENTIFY_STOP;
            break;
        default:
            mxcp_evt = (mxcp_evt_id_t)evt_id;
            break;
    }
    mxcp_send_event(mxcp_evt, (const uint8_t *)payload, (uint8_t)payload_len);

    meshx_err_t err = meshx_prepare_ctrl_message(evt_id, payload_len, payload);
    if (err) {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to create ctrl message: (0x%x)", err);
        return err;
    }

    err = control_task_msg_publish(CONTROL_TASK_MSG_CODE_TO_APP, CONTROL_TASK_MSG_EVT_CTRL,
                                   &meshx_api_ctrl.msg_buff, sizeof(meshx_app_api_msg_t));
    if (err) {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to send ctrl message to app: (0x%x)", err);
    }
    return err;
}

/*
 * @brief Registers the BLE Mesh application data path callback.
 *
 * Subscribes to CONTROL_TASK_MSG_CODE_TO_APP with CONTROL_TASK_MSG_EVT_DATA
 * to receive element telemetry messages and state updates.
 *
 * @param[in] cb Application data callback function pointer.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_app_reg_data_callback(meshx_app_data_cb_t cb)
{
    meshx_err_t err = control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_TO_APP,
        CONTROL_TASK_MSG_EVT_DATA,
        (control_task_msg_handle_t)&meshx_api_control_task_handler);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to register control task callback: (%d)", err);
        return err;
    }

    meshx_api_ctrl.app_data_cb = cb;
    return err;
}

/*
 * @brief Registers the BLE Mesh application control callback.
 *
 * Subscribes to CONTROL_TASK_MSG_CODE_TO_APP with CONTROL_TASK_MSG_EVT_CTRL
 * to receive system lifecycle and provisioning events.
 *
 * @param[in] cb Application control callback function pointer.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_app_reg_ctrl_callback(meshx_app_ctrl_cb_t cb)
{
    meshx_err_t err = control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_TO_APP,
        CONTROL_TASK_MSG_EVT_CTRL,
        (control_task_msg_handle_t)&meshx_api_control_task_handler);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to register control task callback: (%d)", err);
        return err;
    }

    meshx_api_ctrl.app_ctrl_cb = cb;
    return err;
}

/* ====================================================================
 * Legacy API Implementations (Delegated to Unified APIs)
 * ==================================================================== */

/*
 * @brief Sends an element data message to the BLE Mesh application (legacy wrapper).
 *
 * @deprecated Use meshx_data_send_to_app() instead.
 *
 * @param[in] element_id    The element ID.
 * @param[in] element_type  The element type.
 * @param[in] func_id       The function ID.
 * @param[in] msg_len       The message payload length.
 * @param[in] msg           Pointer to the message payload.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_send_msg_to_app(uint16_t element_id, uint16_t element_type, uint16_t func_id, uint16_t msg_len, const void *msg)
{
    meshx_data_msg_hdr_t hdr;
    hdr.element_id = element_id;
    hdr.element_type = element_type;
    hdr.func_id = func_id;
    hdr.msg_len = msg_len;
    return meshx_data_send_to_app(&hdr, msg);
}

/*
 * @brief Sends a system control message to the BLE Mesh application (legacy wrapper).
 *
 * @deprecated Use meshx_ctrl_send_event() instead.
 *
 * @param[in] evt       The control event ID.
 * @param[in] msg_len   The message payload length.
 * @param[in] msg       Pointer to the message payload.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_send_ctrl_msg_to_app(meshx_ctrl_evt_t evt, uint16_t msg_len, const void *msg)
{
    return meshx_ctrl_send_event(evt, 0, msg, msg_len);
}

/*
 * @brief Sends a command downwards to a targeted element (legacy wrapper).
 *
 * @deprecated Use meshx_data_send_to_element() instead.
 *
 * @param[in] element_id    The element ID.
 * @param[in] element_type  The element type.
 * @param[in] func_id       The function ID.
 * @param[in] msg_len       The message payload length.
 * @param[in] msg           Pointer to the message payload.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_send_msg_to_element(uint16_t element_id, uint16_t element_type, uint16_t func_id, uint16_t msg_len, const void *msg)
{
    meshx_data_msg_hdr_t hdr;
    hdr.element_id = element_id;
    hdr.element_type = element_type;
    hdr.func_id = func_id;
    hdr.msg_len = msg_len;
    return meshx_data_send_to_element(&hdr, msg);
}

/*
 * @brief Registers the BLE Mesh element callback (legacy wrapper).
 *
 * @deprecated Use meshx_app_reg_data_callback() instead.
 *
 * @param[in] cb Callback function pointer.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_app_reg_element_callback(meshx_app_data_cb_t cb)
{
    return meshx_app_reg_data_callback(cb);
}

/*
 * @brief Registers the BLE Mesh system events callback (legacy wrapper).
 *
 * @deprecated Use meshx_app_reg_ctrl_callback() instead.
 *
 * @param[in] cb Callback function pointer.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_app_reg_system_events_callback(meshx_app_ctrl_cb_t cb)
{
    return meshx_app_reg_ctrl_callback(cb);
}


