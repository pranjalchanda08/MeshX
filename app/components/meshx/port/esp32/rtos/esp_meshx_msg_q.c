/**
 * @file esp_meshx_msg_q.c
 * @brief MeshX Message Queue Implementation
 *
 * This file implements the MeshX Message Queue.
 *
 * @author Pranjal Chanda
 */

#include "meshx_msg_q.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_system.h"

/**
 * @brief Create a MeshX Message Queue
 *
 * This function creates a MeshX Message Queue.
 *
 * @param[in,out] msg_q_handle Message Queue Handle
 *
 * @return Message Queue Handle
 */
meshx_err_t meshx_msg_q_create(meshx_msg_q_t *msg_q_handle)
{
    if (msg_q_handle == NULL)
    {
        return MESHX_INVALID_ARG;
    }

    // Create a FreeRTOS queue
    QueueHandle_t queue = xQueueCreate(MESHX_MSG_Q_LENGTH, sizeof(meshx_msg_t));

    if (queue == NULL)
    {
        // Failed to create queue
        return MESHX_NO_MEM;
    }

    // Store the queue handle in the msg_q_handle
    *msg_q_handle->__msg_q_handle = (meshx_msg_q_t)queue;

    return MESHX_SUCCESS;
}

/**
 * @brief Delete a MeshX Message Queue
 *
 * This function deletes a MeshX Message Queue.
 *
 * @param[in] msg_q_handle Message Queue Handle
 */
meshx_err_t meshx_msg_q_delete(meshx_msg_q_t *msg_q_handle)
{
    if (msg_q_handle == NULL)
    {
        return MESHX_INVALID_ARG;
    }

    // Delete the FreeRTOS queue
    vQueueDelete((QueueHandle_t)msg_q_handle->__msg_q_handle);

    return MESHX_SUCCESS;
}

/**
 * @brief Send a Message to a MeshX Message Queue
 *
 * This function sends a message to a MeshX Message Queue.
 *
 * @param[in] msg_q_handle Message Queue Handle
 * @param[in] msg Message
 * @param[in] msg_len Message Length
 * @param[in] delay_ms Delay in milliseconds
 *
 * @return None
 */
meshx_err_t meshx_msg_q_send(meshx_msg_q_t *msg_q_handle, void *msg, size_t msg_len, uint32_t delay_ms)
{
    if (msg_q_handle == NULL || msg == NULL || msg_len == 0)
    {
        return MESHX_INVALID_ARG;
    }

    BaseType ret =  xPortInIsrContext() ? xQueueSendFromISR(control_task_queue, &send_msg, &pxHigherPriorityTaskWoken) : xQueueSend(control_task_queue, &send_msg, delay_ms);

    if (ret!= pdPASS)
    {
        return MESHX_FAIL;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Receive a Message from a MeshX Message Queue
 *
 * This function receives a message from a MeshX Message Queue.
 *
 * @param[in] msg_q_handle Message Queue Handle
 * @param[in] msg Message
 * @param[in] msg_len Message Length
 * @param[in] delay_ms Delay in milliseconds
 *
 * @return Message Queue Handle
 */
meshx_err_t meshx_msg_q_recv(meshx_msg_q_t *msg_q_handle, void *msg, size_t msg_len, uint32_t delay_ms)
{
    if (msg_q_handle == NULL || msg == NULL || msg_len == 0)
    {
        return MESHX_INVALID_ARG;
    }

    BaseType ret =  xPortInIsrContext() ? xQueueReceiveFromISR(control_task_queue, &recv_msg, &pxHigherPriorityTaskWoken) : xQueueReceive(control_task_queue, &recv_msg, delay_ms);

    if (ret!= pdPASS)
    {
        return MESHX_FAIL;
    }

    memcpy(msg, recv_msg.msg, msg_len);

    return MESHX_SUCCESS;
}
