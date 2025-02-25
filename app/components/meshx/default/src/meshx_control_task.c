/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file control_task.c
 * @brief Implementation of control task for event handling and messaging.
 *
 * This file contains the implementation of a control task, including functions for
 * creating the task, sending messages, registering message handlers, and handling events.
 * The control task uses FreeRTOS for inter-task communication and event-driven architecture.
 *
 */

#include <meshx_control_task.h>

static void control_task_handler(void *args);

/**
 * @brief Queue handle for control task messages.
 */
static QueueHandle_t control_task_queue;

/**
 * @brief Linked list heads for registered callbacks per message code.
 */
static control_task_evt_cb_reg_t * control_task_msg_code_list_heads [CONTROL_TASK_MSG_CODE_MAX];

/**
 * @brief Create the control task.
 *
 * This function creates a FreeRTOS task to handle control events.
 *
 * @param[in] pdev Pointer to the device structure (dev_struct_t).
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t create_control_task(dev_struct_t * pdev)
{
    BaseType_t err;
    err = xTaskCreate(
        &control_task_handler,
        CONFIG_CONTROL_TASK_NAME,
        CONFIG_CONTROL_TASK_STACK_SIZE,
        pdev,
        CONFIG_CONTROL_TASK_PRIO,
        NULL);
    if (err != pdPASS)
        return ESP_FAIL;

    return ESP_OK;
}

/**
 * @brief Send a message to the control task.
 *
 * This function enqueues a message to the control task queue. If the message has parameters,
 * they are dynamically allocated and copied.
 *
 * @param[in] msg_code The message code to identify the message type.
 * @param[in] msg_evt The event associated with the message.
 * @param[in] msg_evt_params Pointer to the message parameters.
 * @param[in] sizeof_msg_evt_params Size of the message parameters.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t control_task_msg_publish(control_task_msg_code_t msg_code,
                                control_task_msg_evt_t msg_evt,
                                const void* msg_evt_params,
                                size_t sizeof_msg_evt_params)
{
    control_task_msg_t send_msg;
    BaseType_t pxHigherPriorityTaskWoken = pdFALSE;

    if (sizeof_msg_evt_params != 0) {
        send_msg.msg_evt_params = pvPortMalloc(sizeof_msg_evt_params);
        if (!send_msg.msg_evt_params)
            return ESP_ERR_NO_MEM;
        /* Copy the params to allocated space */
        memcpy(send_msg.msg_evt_params, msg_evt_params, sizeof_msg_evt_params);
    }

    send_msg.msg_code = msg_code;
    send_msg.msg_evt = msg_evt;

    xPortInIsrContext() ? xQueueSendFromISR(control_task_queue, &send_msg, &pxHigherPriorityTaskWoken) :
        xQueueSend(control_task_queue, &send_msg, portMAX_DELAY);

    return ESP_OK;
}

/**
 * @brief Register a callback for a specific message code and event bitmap.
 *
 * This function allows registering a callback handler for a specific message code and event type.
 *
 * @param[in] msg_code  The message code to register the handler for.
 * @param[in] evt_bmap  Bitmap of events to register for.
 * @param[in] callback        Callback function to handle the message and event.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t control_task_msg_subscribe(control_task_msg_code_t msg_code,
            control_task_msg_evt_t evt_bmap,
            control_task_msg_handle_t callback)
{
    if (callback == NULL || evt_bmap == 0 || msg_code >= CONTROL_TASK_MSG_CODE_MAX)
        return ESP_ERR_INVALID_ARG; // Invalid arguments

    control_task_evt_cb_reg_t *new_node = (control_task_evt_cb_reg_t *)malloc(sizeof(control_task_evt_cb_reg_t));
    if (new_node == NULL)
        return ESP_ERR_NO_MEM; // Memory allocation failed

    new_node->cb = callback;
    new_node->msg_evt_bmap = evt_bmap;
    new_node->next = control_task_msg_code_list_heads[msg_code];
    control_task_msg_code_list_heads[msg_code] = new_node;

    return ESP_OK;
}

/**
 * @brief Deregister a callback for a specific message code and event bitmap.
 *
 * This function allows deregistering a callback handler for a specific message code and event type.
 *
 * @param[in] msg_code  The message code to deregister the handler for.
 * @param[in] evt_bmap  Bitmap of events to deregister for.
 * @param[in] callback        Callback function to deregister.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t control_task_msg_unsubscribe(control_task_msg_code_t msg_code,
            control_task_msg_evt_t evt_bmap,
            control_task_msg_handle_t callback)
{
    if (callback == NULL || evt_bmap == 0 || msg_code >= CONTROL_TASK_MSG_CODE_MAX)
        return ESP_ERR_INVALID_ARG; // Invalid arguments

    control_task_evt_cb_reg_t *prev = NULL;
    control_task_evt_cb_reg_t *curr = control_task_msg_code_list_heads[msg_code];

    while (curr) {
        if (curr->cb == callback && curr->msg_evt_bmap == evt_bmap) {
            if (prev == NULL) {
                control_task_msg_code_list_heads[msg_code] = curr->next;
            } else {
                prev->next = curr->next;
            }
            free(curr);
            return ESP_OK;
        }
        prev = curr;
        curr = curr->next;
    }

    return ESP_ERR_NOT_FOUND;
}

/**
 * @brief Dispatch a message to the registered callbacks.
 *
 * This function dispatches a message to the registered handlers based on the message code
 * and event type.
 *
 * @param[in] pdev Pointer to the device structure (dev_struct_t).
 * @param[in] msg_code The message code of the received message.
 * @param[in] evt The event type of the received message.
 * @param[in] params Pointer to the message parameters.
 * @return ESP_OK on success, or an error code on failure.
 */
static esp_err_t control_task_msg_dispatch(
    dev_struct_t * pdev,
    control_task_msg_code_t msg_code,
    control_task_msg_evt_t evt,
    void* params)
{
    if (!pdev)
        return ESP_ERR_INVALID_ARG;

    control_task_evt_cb_reg_t *ptr = control_task_msg_code_list_heads[msg_code];
    bool evt_handled = false;

    if (ptr == NULL) {
        ESP_LOGW(TAG, "No control task msg callback registered for msg: %p", (void*)msg_code);
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGD(TAG, "msg|evt: %p|%p", (void*) msg_code, (void*) evt);

    while (ptr) {
        if ((evt & ptr->msg_evt_bmap) && (ptr->cb != NULL)) {
            ptr->cb(pdev, evt, params); // Call the registered callback
            evt_handled = true;
        }
        ptr = ptr->next; // Move to the next registration
    }
    if (!evt_handled)
        ESP_LOGD(TAG, "No handler reg for EVT %p", (void*) evt);

    return ESP_OK;
}

/**
 * @brief Create the control task message queue.
 *
 * This function initializes the FreeRTOS queue for handling control task messages.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
static esp_err_t create_control_task_msg_q(void)
{
    control_task_queue = xQueueCreate(CONFIG_CONTROL_TASK_QUEUE_LEN, sizeof(control_task_msg_t));

    if (control_task_queue == NULL) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief Task handler function for processing control task messages.
 *
 * This function runs in a loop, receiving messages from the queue and dispatching them
 * to registered handlers. Allocated memory for message parameters is freed after processing.
 *
 * @param args Pointer to the device structure (dev_struct_t) passed during task creation.
 */
static void control_task_handler(void *args)
{
    esp_err_t err;
    control_task_msg_t recv_msg;
    dev_struct_t * pdev = (dev_struct_t *) args;
    err = create_control_task_msg_q();
    if (err)
        ESP_LOGE(TAG, "Failed to initialise Control Task Msg Q Err: 0x%x", err);

    while (true) {
        if (xQueueReceive(control_task_queue, &recv_msg, portMAX_DELAY) == pdTRUE) {
            err = control_task_msg_dispatch(pdev, recv_msg.msg_code, recv_msg.msg_evt, recv_msg.msg_evt_params);
            if (err)
                ESP_LOGE(TAG, "Err: 0x%x", err);
            if (recv_msg.msg_evt_params) {
                /* If Params were passed Free the allocated memory */
                vPortFree(recv_msg.msg_evt_params);
                ESP_LOGD(TAG, "ESP Heap available: %d", xPortGetFreeHeapSize());
            }
        }
    }
}
