/**
 * Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_msg_q.c
 * @brief Implementation of MeshX Message Queue using Zephyr RTOS k_msgq.
 *        This file provides functions to create, delete, send, and receive
 *        messages using Zephyr message queue primitives.
 *
 * @author Pranjal Chanda
 */

#include "interface/rtos/meshx_msg_q.h"
#include <zephyr/kernel.h>
#include <stdlib.h>

/**
 * @brief Create a MeshX Message Queue on Zephyr
 *
 * @param[in,out] msg_q_handle Message Queue Handle
 * @return meshx_err_t MESHX_SUCCESS on success, error code otherwise
 */
meshx_err_t meshx_msg_q_create(meshx_msg_q_t *msg_q_handle)
{
    if (msg_q_handle == NULL || msg_q_handle->max_msg_depth <= 0 || msg_q_handle->max_msg_length <= 0) {
        return MESHX_INVALID_ARG;
    }

    struct k_msgq *queue = (struct k_msgq *)k_malloc(sizeof(struct k_msgq));
    if (queue == NULL) {
        return MESHX_NO_MEM;
    }

    int ret = k_msgq_alloc_init(queue, (size_t)msg_q_handle->max_msg_depth, (uint32_t)msg_q_handle->max_msg_length);
    if (ret != 0) {
        k_free(queue);
        return MESHX_NO_MEM;
    }

    msg_q_handle->__msg_q_handle = (void *)queue;
    return MESHX_SUCCESS;
}

/**
 * @brief Delete a MeshX Message Queue
 *
 * @param[in] msg_q_handle Message Queue Handle
 * @return meshx_err_t
 */
meshx_err_t meshx_msg_q_delete(meshx_msg_q_t *msg_q_handle)
{
    if (msg_q_handle == NULL || msg_q_handle->__msg_q_handle == NULL) {
        return MESHX_INVALID_ARG;
    }

    struct k_msgq *queue = (struct k_msgq *)msg_q_handle->__msg_q_handle;
    k_msgq_cleanup(queue);
    k_free(queue);
    msg_q_handle->__msg_q_handle = NULL;

    return MESHX_SUCCESS;
}

/**
 * @brief Send a Message to a MeshX Message Queue Back
 *
 * @param[in] msg_q_handle Message Queue Handle
 * @param[in] msg Message buffer
 * @param[in] msg_len Message Length
 * @param[in] delay_ms Delay in milliseconds
 * @return meshx_err_t
 */
meshx_err_t meshx_msg_q_send(meshx_msg_q_t *msg_q_handle, void const *msg, size_t msg_len, uint32_t delay_ms)
{
    if (msg_q_handle == NULL || msg_q_handle->__msg_q_handle == NULL || msg == NULL || msg_len == 0) {
        return MESHX_INVALID_ARG;
    }

    struct k_msgq *queue = (struct k_msgq *)msg_q_handle->__msg_q_handle;
    k_timeout_t timeout = k_is_in_isr() ? K_NO_WAIT : ((delay_ms == UINT32_MAX) ? K_FOREVER : K_MSEC(delay_ms));

    int ret = k_msgq_put(queue, msg, timeout);
    if (ret != 0) {
        return MESHX_FAIL;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Send a Message to the front of a MeshX Message Queue
 *
 * @param[in] msg_q_handle Message Queue Handle
 * @param[in] msg Message
 * @param[in] msg_len Message Length
 * @param[in] delay_ms Delay in milliseconds
 * @return meshx_err_t
 */
meshx_err_t meshx_msg_q_send_front(meshx_msg_q_t *msg_q_handle, void const *msg, size_t msg_len, uint32_t delay_ms)
{
    /* Zephyr k_msgq puts items into FIFO order; route via k_msgq_put */
    return meshx_msg_q_send(msg_q_handle, msg, msg_len, delay_ms);
}

/**
 * @brief Receive a Message from a MeshX Message Queue
 *
 * @param[in] msg_q_handle Message Queue Handle
 * @param[out] msg Destination buffer
 * @param[in] delay_ms Delay in milliseconds
 * @return meshx_err_t
 */
meshx_err_t meshx_msg_q_recv(meshx_msg_q_t *msg_q_handle, void *msg, uint32_t delay_ms)
{
    if (msg_q_handle == NULL || msg_q_handle->__msg_q_handle == NULL || msg == NULL) {
        return MESHX_INVALID_ARG;
    }

    struct k_msgq *queue = (struct k_msgq *)msg_q_handle->__msg_q_handle;
    k_timeout_t timeout = k_is_in_isr() ? K_NO_WAIT : ((delay_ms == UINT32_MAX) ? K_FOREVER : K_MSEC(delay_ms));

    int ret = k_msgq_get(queue, msg, timeout);
    if (ret != 0) {
        return MESHX_FAIL;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Peek a Message from a MeshX Message Queue
 *
 * @param[in] msg_q_handle Message Queue Handle
 * @param[out] msg Destination buffer
 * @param[in] delay_ms Delay in milliseconds
 * @return meshx_err_t
 */
meshx_err_t meshx_msg_q_peek(meshx_msg_q_t *msg_q_handle, void *msg, uint32_t delay_ms)
{
    ARG_UNUSED(delay_ms);
    if (msg_q_handle == NULL || msg_q_handle->__msg_q_handle == NULL || msg == NULL) {
        return MESHX_INVALID_ARG;
    }

    struct k_msgq *queue = (struct k_msgq *)msg_q_handle->__msg_q_handle;
    int ret = k_msgq_peek(queue, msg);
    if (ret != 0) {
        return MESHX_FAIL;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Get the number of messages in a MeshX Message Queue
 *
 * @param[in] msg_q_handle Message Queue Handle
 * @return int Number of messages in queue
 */
int meshx_msg_q_get_count(meshx_msg_q_t *msg_q_handle)
{
    if (msg_q_handle == NULL || msg_q_handle->__msg_q_handle == NULL) {
        return 0;
    }

    struct k_msgq *queue = (struct k_msgq *)msg_q_handle->__msg_q_handle;
    return (int)k_msgq_num_used_get(queue);
}
