/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file FreeRTOS_meshx_msg_q.c
 * @brief Implementation of MeshX Message Queue using FreeRTOS queues.
 *        This file provides functions to create, delete, send, and receive
 *        messages using a message queue abstraction for the MeshX framework.
 *
 * @author Pranjal Chanda
 *
 */

#include "interface/rtos/meshx_msg_q.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

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
    QueueHandle_t queue = xQueueCreate(msg_q_handle->max_msg_length, msg_q_handle->max_msg_depth);

    if (queue == NULL)
    {
        // Failed to create queue
        return MESHX_NO_MEM;
    }

    // Store the queue handle in the msg_q_handle
    msg_q_handle->__msg_q_handle = queue;

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
meshx_err_t meshx_msg_q_send(meshx_msg_q_t *msg_q_handle, void const *msg, size_t msg_len, uint32_t delay_ms)
{
    if (msg_q_handle == NULL || msg == NULL || msg_len == 0)
    {
        return MESHX_INVALID_ARG;
    }

    BaseType_t pxHigherPriorityTaskWoken = pdFALSE;

    BaseType_t ret =  xPortInIsrContext() ? xQueueSendFromISR(msg_q_handle->__msg_q_handle, msg, &pxHigherPriorityTaskWoken)
        : xQueueSend(msg_q_handle->__msg_q_handle, msg, pdMS_TO_TICKS(delay_ms));

    if (ret!= pdPASS)
    {
        return MESHX_FAIL;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Send a Message to the front of a MeshX Message Queue
 *
 * This function sends a message to the front of a MeshX Message Queue.
 *
 * @param[in] msg_q_handle Message Queue Handle
 * @param[in] msg Message
 * @param[in] msg_len Message Length
 * @param[in] delay_ms Delay in milliseconds
 *
 * @return None
 */
meshx_err_t meshx_msg_q_send_front(meshx_msg_q_t *msg_q_handle, void const *msg, size_t msg_len, uint32_t delay_ms)
{
    if (msg_q_handle == NULL || msg == NULL || msg_len == 0)
    {
        return MESHX_INVALID_ARG;
    }

    BaseType_t pxHigherPriorityTaskWoken = pdFALSE;

    BaseType_t ret =  xPortInIsrContext() ? xQueueSendToFrontFromISR(msg_q_handle->__msg_q_handle, msg, &pxHigherPriorityTaskWoken)
        : xQueueSendToFront(msg_q_handle->__msg_q_handle, msg, pdMS_TO_TICKS(delay_ms));

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
 * @param[in] msg_q_handle  Message Queue Handle
 * @param[in] msg           Message
 * @param[in] delay_ms      Delay in milliseconds
 *
 * @return Message Queue Handle
 */
meshx_err_t meshx_msg_q_recv(meshx_msg_q_t *msg_q_handle, void *msg, uint32_t delay_ms)
{
    if (msg_q_handle == NULL || msg == NULL)
    {
        return MESHX_INVALID_ARG;
    }
    BaseType_t pxHigherPriorityTaskWoken = pdFALSE;

    BaseType_t ret =  xPortInIsrContext() ? xQueueReceiveFromISR(msg_q_handle->__msg_q_handle, msg, &pxHigherPriorityTaskWoken)
        : xQueueReceive(msg_q_handle->__msg_q_handle, msg, pdMS_TO_TICKS(delay_ms));

    if (ret!= pdPASS)
    {
        return MESHX_FAIL;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Peek a Message from a MeshX Message Queue
 *
 * This function peeks a message from a MeshX Message Queue.
 *
 * @param[in] msg_q_handle Message Queue Handle
 * @param[in] msg Message
 * @param[in] delay_ms Delay in milliseconds
 *
 * @return Message Queue Handle
 */
meshx_err_t meshx_msg_q_peek(meshx_msg_q_t *msg_q_handle, void *msg, uint32_t delay_ms)
{
    if (msg_q_handle == NULL || msg == NULL)
    {
        return MESHX_INVALID_ARG;
    }

    BaseType_t ret =  xPortInIsrContext() ? xQueuePeekFromISR(msg_q_handle->__msg_q_handle, msg)
        : xQueuePeek(msg_q_handle->__msg_q_handle, msg, pdMS_TO_TICKS(delay_ms));

    if (ret!= pdPASS)
    {
        return MESHX_FAIL;
    }

    return MESHX_SUCCESS;
}

