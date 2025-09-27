/**
 * @file meshx_msg_q.h
 * @brief MeshX Message Queue Interface
 *
 * This file contains the MeshX Message Queue Interface.
 *
 * @author  Pranjal Chanda
 * @version 1.0
 */

#ifndef __MESHX_MSG_Q_H
#define __MESHX_MSG_Q_H

#include <stdint.h>
#include <stddef.h>
#include "meshx_err.h"

/**
 * @brief MeshX Message Queue Structure
 */
typedef struct meshx_msg_q
{
    /* Public */
    int max_msg_depth;        /**< Maximum Message Length */
    int max_msg_length;      /**< Maximum Message Count */
    /* Private */
    void *__msg_q_handle;   /**< Message Queue Handle */
} meshx_msg_q_t;

/**
 * @brief Create a MeshX Message Queue
 *
 * This function creates a MeshX Message Queue.
 *
 * @param[in,out] msg_q_handle Message Queue Handle
 *
 * @return Message Queue Handle
 */
meshx_err_t meshx_msg_q_create(meshx_msg_q_t *msg_q_handle);

/**
 * @brief Delete a MeshX Message Queue
 *
 * This function deletes a MeshX Message Queue.
 *
 * @param[in] msg_q_handle Message Queue Handle
 */
meshx_err_t meshx_msg_q_delete(meshx_msg_q_t *msg_q_handle);

/**
 * @brief Send a Message to a MeshX Message Queue Back
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
meshx_err_t meshx_msg_q_send(meshx_msg_q_t *msg_q_handle, void const *msg, size_t msg_len, uint32_t delay_ms);

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
meshx_err_t meshx_msg_q_send_front(meshx_msg_q_t *msg_q_handle, void const *msg, size_t msg_len, uint32_t delay_ms);

/**
 * @brief Receive a Message from a MeshX Message Queue
 *
 * This function receives a message from a MeshX Message Queue.
 *
 * @param[in] msg_q_handle Message Queue Handle
 * @param[in] msg Message
 * @param[in] delay_ms Delay in milliseconds
 *
 * @return Message Queue Handle
 */
meshx_err_t meshx_msg_q_recv(meshx_msg_q_t *msg_q_handle, void *msg, uint32_t delay_ms);

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
meshx_err_t meshx_msg_q_peek(meshx_msg_q_t *msg_q_handle, void *msg, uint32_t delay_ms);

#endif /* __MESHX_MSG_Q_H */
