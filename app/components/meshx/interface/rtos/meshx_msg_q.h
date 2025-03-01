/**
 * @file meshx_msg_q.h
 * @brief MeshX Message Queue Interface
 *
 * This file contains the MeshX Message Queue Interface.
 *
 * @auther  Pranjal Chanda
 * @version 1.0
 */

#ifndef __MESHX_MSG_Q_H
#define __MESHX_MSG_Q_H

#include <stdint.h>
#include <stddef.h>
#include "meshx_err_t"

/**
 * @brief MeshX Message Queue Structure
 */
typedef struct meshx_msg_q
{
    /* Public */
    const char *msg_q_name; /**< Message Queue Name */
    int max_msg_len;        /**< Maximum Message Length */
    int max_msg_count;      /**< Maximum Message Count */
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
meshx_err_t meshx_msg_q_send(meshx_msg_q_t *msg_q_handle, void *msg, size_t msg_len, uint32_t delay_ms);

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
meshx_err_t meshx_msg_q_recv(meshx_msg_q_t *msg_q_handle, void *msg, size_t msg_len, uint32_t delay_ms);

#endif /* __MESHX_MSG_Q_H */
