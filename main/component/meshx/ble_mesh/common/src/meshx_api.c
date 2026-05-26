/**
 * @file meshx_api.c
 * @brief Implementation of the BLE Mesh application API.
 */
#include <meshx_api.h>
#include "meshx_mxcp.h"
#include <stdlib.h>
#include <string.h>

static struct {
    meshx_api_data_cb_t app_data_cb;    /**< Callback function to be invoked when a data message is received. */
    meshx_api_ctrl_cb_t app_ctrl_cb;    /**< Callback function to be invoked when a control message is received. */
} meshx_api_ctrl;

/**
 * @brief Handler for incoming messages from the control task.
 *
 * @param pdev Pointer to the device structure.
 * @param evt Event type.
 * @param params Pointer to the message parameters.
 * @param params_len Length of the parameters.
 * @return MESHX_SUCCESS on success.
 */
static meshx_err_t meshx_api_control_task_handler(dev_struct_t *pdev, control_task_msg_evt_t evt, void *params, uint16_t params_len)
{
    if (!pdev) return MESHX_INVALID_ARG;

    if (evt == CONTROL_TASK_MSG_EVT_DATA) {
        if (meshx_api_ctrl.app_data_cb) {
            meshx_api_ctrl.app_data_cb((const meshx_msg_data_t *)params);
        }
    } else {
        if (meshx_api_ctrl.app_ctrl_cb) {
            meshx_api_ctrl.app_ctrl_cb((const meshx_msg_ctrl_t *)params);
        }
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Registers a callback function to be invoked when a data message is received.
 *
 * This function subscribes to the data message event from the control task and sets the application data callback.
 * The callback will be invoked for all data messages received from the mesh network.
 *
 * @param cb Callback function to be invoked when a data message is received.
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t meshx_api_register_data_cb(meshx_api_data_cb_t cb)
{
    meshx_err_t err = control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_TO_APP,
        CONTROL_TASK_MSG_EVT_DATA,
        (control_task_msg_handle_t)&meshx_api_control_task_handler);
    if (err) {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to register control task callback: (%d)", err);
        return err;
    }
    meshx_api_ctrl.app_data_cb = cb;
    return MESHX_SUCCESS;
}

/**
 * @brief Registers a callback function to be invoked when a control message is received.
 *
 * This function subscribes to the control message event from the control task and sets the application control callback.
 * The callback will be invoked for all control messages received from the mesh network.
 *
 * @param cb Callback function to be invoked when a control message is received.
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t meshx_api_register_ctrl_cb(meshx_api_ctrl_cb_t cb)
{
    meshx_err_t err = control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_TO_APP,
        CONTROL_TASK_MSG_EVT_CTRL,
        (control_task_msg_handle_t)&meshx_api_control_task_handler);
    if (err) {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to register control task callback: (%d)", err);
        return err;
    }
    meshx_api_ctrl.app_ctrl_cb = cb;
    return MESHX_SUCCESS;
}

/**
 * @brief Sends a data message to the mesh network.
 *
 * This function sends a data message to the mesh network. The message will be sent to the mesh network.
 *
 * @param msg Pointer to the data message to be sent.
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t meshx_api_data_send(const meshx_msg_data_t *msg)
{
    if (!msg) return MESHX_INVALID_ARG;
    uint16_t total_len = sizeof(meshx_msg_data_t) + msg->payload_len;

    if (msg->msg_id & MESHX_MSG_DIR_EVT) {
        mxcp_send_event(msg->msg_id, (const uint8_t*)msg + 2, total_len - 2);
        control_task_msg_publish(CONTROL_TASK_MSG_CODE_TO_APP, CONTROL_TASK_MSG_EVT_DATA, (void*)msg, total_len);
    } else {
        control_task_msg_publish(CONTROL_TASK_MSG_CODE_TO_MESHX, CONTROL_TASK_MSG_EVT_DATA, (void*)msg, total_len);
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Sends a control message to the mesh network.
 *
 * This function sends a control message to the mesh network. The message will be sent to the mesh network.
 *
 * @param msg Pointer to the control message to be sent.
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t meshx_api_ctrl_send(const meshx_msg_ctrl_t *msg)
{
    if (!msg) return MESHX_INVALID_ARG;
    uint16_t total_len = sizeof(meshx_msg_ctrl_t) + msg->payload_len;

    if (msg->msg_id & MESHX_MSG_DIR_EVT) {
        mxcp_send_event(msg->msg_id, (const uint8_t*)msg + 2, total_len - 2);
        control_task_msg_publish(CONTROL_TASK_MSG_CODE_TO_APP, CONTROL_TASK_MSG_EVT_CTRL, (void*)msg, total_len);
    } else {
        control_task_msg_publish(CONTROL_TASK_MSG_CODE_TO_MESHX, CONTROL_TASK_MSG_EVT_CTRL, (void*)msg, total_len);
    }
    return MESHX_SUCCESS;
}

meshx_err_t meshx_api_ctrl_send_with_payload(uint16_t msg_id, const void *payload, uint16_t payload_len)
{
    uint16_t total_len = sizeof(meshx_msg_ctrl_t) + payload_len;
    meshx_msg_ctrl_t *msg = (meshx_msg_ctrl_t*)malloc(total_len);
    if (!msg) return MESHX_FAIL;

    msg->msg_id = msg_id;
    msg->payload_len = payload_len;
    if (payload && payload_len > 0) {
        memcpy(msg->payload, payload, payload_len);
    }

    meshx_err_t err = meshx_api_ctrl_send(msg);
    free(msg);
    return err;
}
