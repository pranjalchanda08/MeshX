/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_control_task.c
 * @brief Implementation of control task for event handling and messaging.
 *
 * This file contains the implementation of a control task, including functions for
 * creating the task, sending messages, registering message handlers, and handling events.
 * The control task uses FreeRTOS for inter-task communication and event-driven architecture.
 *
 * @author Pranjal Chanda
 */

#include <meshx_control_task.h>

static void control_task_handler(void *args);

/**
 * @brief Queue handle for control task messages.
 */
static meshx_msg_q_t control_task_queue =
{
    .max_msg_count = CONFIG_CONTROL_TASK_QUEUE_LEN,
    .max_msg_len = sizeof(control_task_msg_t)
};

/**
 * @brief Linked list heads for registered callbacks per message code.
 */
static control_task_evt_cb_reg_t *control_task_msg_code_list_heads[CONTROL_TASK_MSG_CODE_MAX];

/**
 * @brief Create the control task.
 *
 * This function creates a FreeRTOS task to handle control events.
 *
 * @param[in] pdev Pointer to the device structure (dev_struct_t).
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
meshx_err_t create_control_task(dev_struct_t *pdev)
{

    meshx_task_t task_handle = {
        .arg = pdev,
        .task_cb = control_task_handler,
        .priority = CONFIG_CONTROL_TASK_PRIO,
        .task_name = CONFIG_CONTROL_TASK_NAME,
        .stack_size = CONFIG_CONTROL_TASK_STACK_SIZE,
    };

    return meshx_task_create(&task_handle);
}

/* @brief Publish a control task message.
 *
 * This function allows you to publish a control task message with the given
 * message code, event, and event parameters.
 * The message will be sent to the control task for processing.
 *
 * @param[in] msg_code              The message code to publish.
 * @param[in] msg_evt               The event associated with the message.
 * @param[in] msg_evt_params        Pointer to the event parameters.
 * @param[in] sizeof_msg_evt_params Size of the event parameters.
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
meshx_err_t control_task_msg_publish(control_task_msg_code_t msg_code,
                                   control_task_msg_evt_t msg_evt,
                                   const void *msg_evt_params,
                                   size_t sizeof_msg_evt_params)
{
    control_task_msg_t send_msg;

    if (sizeof_msg_evt_params != 0)
    {
        meshx_err_t err = meshx_rtos_malloc(&send_msg.msg_evt_params, sizeof_msg_evt_params);
        if (err)
            return err;
        /* Copy the params to allocated space */
        memcpy(send_msg.msg_evt_params, msg_evt_params, sizeof_msg_evt_params);
    }

    send_msg.msg_code = msg_code;
    send_msg.msg_evt = msg_evt;

    return meshx_msg_q_send(&control_task_queue, &send_msg, sizeof(send_msg), UINT32_MAX);
}

/**
 * @brief Subscribe to a control task message.
 *
 * This function allows you to subscribe to a specific control task message
 * identified by the given message code. When the message is received, the
 * specified callback function will be invoked.
 *
 * @param[in] msg_code  The message code to subscribe to.
 * @param[in] evt_bmap  The event bitmap associated with the message.
 * @param[in] callback  The callback function to be called when the message is received.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_INVALID_ARG: Invalid argument
 *     - MESHX_FAIL: Other failures
 */
meshx_err_t control_task_msg_subscribe(control_task_msg_code_t msg_code,
                                     control_task_msg_evt_t evt_bmap,
                                     control_task_msg_handle_t callback)
{
    if (callback == NULL || evt_bmap == 0 || msg_code >= CONTROL_TASK_MSG_CODE_MAX)
        return MESHX_INVALID_ARG; // Invalid arguments

    control_task_evt_cb_reg_t *new_node = NULL;
    meshx_err_t err = meshx_rtos_malloc((void**)&new_node, sizeof(control_task_evt_cb_reg_t));
    if (err || !new_node)
        return err; // Memory allocation failed

    new_node->cb = callback;
    new_node->msg_evt_bmap = evt_bmap;
    new_node->next = control_task_msg_code_list_heads[msg_code];
    control_task_msg_code_list_heads[msg_code] = new_node;

    return MESHX_SUCCESS;
}

/**
 * @brief Deregister a callback for a specific message code and event bitmap.
 *
 * This function allows deregistering a callback handler for a specific message code and event type.
 *
 * @param[in] msg_code  The message code to deregister the handler for.
 * @param[in] evt_bmap  Bitmap of events to deregister for.
 * @param[in] callback  Callback function to deregister.
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
meshx_err_t control_task_msg_unsubscribe(control_task_msg_code_t msg_code,
                                       control_task_msg_evt_t evt_bmap,
                                       control_task_msg_handle_t callback)
{
    if (callback == NULL || evt_bmap == 0 || msg_code >= CONTROL_TASK_MSG_CODE_MAX)
        return MESHX_INVALID_ARG; // Invalid arguments

    control_task_evt_cb_reg_t *prev = NULL;
    control_task_evt_cb_reg_t *curr = control_task_msg_code_list_heads[msg_code];

    while (curr)
    {
        if (curr->cb == callback && curr->msg_evt_bmap == evt_bmap)
        {
            if (prev == NULL)
                control_task_msg_code_list_heads[msg_code] = curr->next;
            else
                prev->next = curr->next;

            return meshx_rtos_free((void**)&curr);
        }
        prev = curr;
        curr = curr->next;
    }

    return MESHX_NOT_FOUND;
}

/**
 * @brief Dispatch a message to the registered callbacks.
 *
 * This function dispatches a message to the registered handlers based on the message code
 * and event type.
 *
 * @param[in] pdev      Pointer to the device structure (dev_struct_t).
 * @param[in] msg_code  The message code of the received message.
 * @param[in] evt       The event type of the received message.
 * @param[in] params    Pointer to the message parameters.
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
static meshx_err_t control_task_msg_dispatch(
    dev_struct_t *pdev,
    control_task_msg_code_t msg_code,
    control_task_msg_evt_t evt,
    void *params)
{
    if (!pdev)
        return MESHX_INVALID_ARG;

    control_task_evt_cb_reg_t *ptr = control_task_msg_code_list_heads[msg_code];
    bool evt_handled = false;

    if (ptr == NULL)
    {
        MESHX_LOGW(MODULE_ID_COMMON, "No control task msg callback registered for msg: %p", (void *)msg_code);
        return MESHX_INVALID_STATE;
    }

    MESHX_LOGD(MODULE_ID_COMMON, "msg|evt: %p|%p", (void *)msg_code, (void *)evt);

    while (ptr)
    {
        if ((evt & ptr->msg_evt_bmap) && (ptr->cb != NULL))
        {
            ptr->cb(pdev, evt, params); // Call the registered callback
            evt_handled = true;
        }
        ptr = ptr->next; // Move to the next registration
    }
    if (!evt_handled)
        MESHX_LOGD(MODULE_ID_COMMON, "No handler reg for EVT %p", (void *)evt);

    return MESHX_SUCCESS;
}

/**
 * @brief Create the control task message queue.
 *
 * This function initializes the FreeRTOS queue for handling control task messages.
 *
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
static meshx_err_t create_control_task_msg_q(void)
{
    return meshx_msg_q_create(&control_task_queue);
}

/**
 * @brief Task handler function for processing control task messages.
 *
 * This function runs in a loop, receiving messages from the queue and dispatching them
 * to registered handlers. Allocated memory for message parameters is freed after processing.
 *
 * @param[in] args Pointer to the device structure (dev_struct_t) passed during task creation.
 */
static void control_task_handler(void *args)
{
    meshx_err_t err;
    control_task_msg_t recv_msg;
    dev_struct_t *pdev = (dev_struct_t *)args;
    err = create_control_task_msg_q();
    if (err)
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to initialise Control Task Msg Q Err: 0x%x", err);

    while (true)
    {
        if (meshx_msg_q_recv(&control_task_queue, &recv_msg, UINT32_MAX) == MESHX_SUCCESS)
        {
            err = control_task_msg_dispatch(pdev, recv_msg.msg_code, recv_msg.msg_evt, recv_msg.msg_evt_params);
            if (err)
                MESHX_LOGE(MODULE_ID_COMMON, "Err: 0x%x", err);
            if (recv_msg.msg_evt_params)
            {
                /* If Params were passed Free the allocated memory */
                meshx_rtos_free(&recv_msg.msg_evt_params);
                MESHX_LOGD(MODULE_ID_COMMON, "ESP Heap available: %d", meshx_rtos_get_free_heap());
            }
        }
    }
}
