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

#define MESSAGE_BUFF_CLEAR(buff)        memset(&buff, 0, sizeof(buff))

static struct{

    meshx_app_data_cb_t app_data_cb;    /**< BLE Mesh application data callback */
    meshx_app_ctrl_cb_t app_ctrl_cb;    /**< BLE Mesh application control callback */
    meshx_app_api_msg_t msg_buff;       /**< BLE Mesh application message buffer */
}meshx_api_ctrl;

/**
 * @brief Control task handler for BLE Mesh application messages.
 *
 * This function handles BLE Mesh application messages.
 *
 * @param[in] pdev      Pointer to the device structure.
 * @param[in] evt       Event type.
 * @param[in] params    Pointer to the message parameters.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
static meshx_err_t meshx_api_control_task_handler(const dev_struct_t *pdev, control_task_msg_evt_t evt, const void *params)
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

/**
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

    meshx_api_ctrl.msg_buff.msg_type_u.element_msg.func_id = func_id;
    meshx_api_ctrl.msg_buff.msg_type_u.element_msg.msg_len = msg_len;
    meshx_api_ctrl.msg_buff.msg_type_u.element_msg.element_id = element_id;
    meshx_api_ctrl.msg_buff.msg_type_u.element_msg.element_type = element_type;

    memcpy(meshx_api_ctrl.msg_buff.data, msg, msg_len);

    return MESHX_SUCCESS;
}

/**
 * @brief Sends a message to the BLE Mesh application.
 *
 * This function sends a message to the BLE Mesh application.
 *
 * @param[in] element_id    The element ID.
 * @param[in] element_type  The element type.
 * @param[in] func_id       The function ID.
 * @param[in] msg_len       The message length.
 * @param[in] msg           Pointer to the message.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_send_msg_to_app(uint16_t element_id, uint16_t element_type, uint16_t func_id, uint16_t msg_len, const void *msg)
{
    meshx_err_t err = MESHX_SUCCESS;

    err = meshx_prepare_data_message(element_id, element_type, func_id, msg_len,msg);
    if(err)
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to create message: (0x%x)", err);

    err = control_task_msg_publish(CONTROL_TASK_MSG_CODE_TO_APP, CONTROL_TASK_MSG_EVT_DATA, &meshx_api_ctrl.msg_buff, sizeof(meshx_app_api_msg_t));
    if(err)
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to send message to app: (0x%x)", err);

    return err;
}

/**
 * @brief Sends a message to the element
 *
 * This function sends a message to the element from BLE mesh Application
 *
 * @param[in] element_id    The element ID.
 * @param[in] element_type  The element type.
 * @param[in] func_id       The function ID.
 * @param[in] msg_len       The message length.
 * @param[in] msg           Pointer to the message.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_send_msg_to_element(uint16_t element_id, uint16_t element_type, uint16_t func_id, uint16_t msg_len, const void *msg)
{
    meshx_err_t err = MESHX_SUCCESS;

    err = meshx_prepare_data_message(element_id, element_type, func_id, msg_len,msg);
    if(err)
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to create message: (0x%x)", err);

    err = control_task_msg_publish(CONTROL_TASK_MSG_CODE_TO_MESHX, CONTROL_TASK_MSG_EVT_DATA, &meshx_api_ctrl.msg_buff, sizeof(meshx_app_api_msg_t));
    if(err)
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to send message to app: (0x%x)", err);

    return err;
}

/**
 * @brief Registers the BLE Mesh application callback.
 *
 * This function registers the BLE Mesh application data path callback.
 *
 * @param[in] cb Pointer to the application callback.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_app_reg_element_callback(meshx_app_data_cb_t cb)
{
    meshx_err_t err = MESHX_SUCCESS;

    err = control_task_msg_subscribe(
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

/**
 * @brief Registers the BLE Mesh application control callback.
 *
 * This function registers the BLE Mesh application control callback.
 *
 * @param[in] cb Pointer to the control callback.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_app_reg_system_events_callback(meshx_app_ctrl_cb_t cb)
{
    meshx_err_t err = MESHX_SUCCESS;

    err = control_task_msg_subscribe(
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
