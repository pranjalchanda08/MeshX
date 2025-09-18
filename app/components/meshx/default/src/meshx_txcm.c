/**
 * @copyright Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_txcm.c
 * @brief MeshX Tx Control Module
 * This file contains the implementation of the MeshX Tx Control Module,
 * which is responsible for managing transmission control in the MeshX BLE Mesh stack.
 *
 * @author Pranjal Chanda
 *
 */
#include "meshx.h"
#include "sys/queue.h"
#include "string.h"
#include "meshx_txcm.h"

#if CONFIG_TXCM_ENABLE
/**
 * @brief Magic value used to check if the Tx Control module has been initialized.
 */
#define MESHX_TXCM_INIT_MAGIC       0x4455
/**
 * @brief Stack size for the Tx Control task in bytes.
 */
#define MESHX_TXCM_TASK_STACK_SIZE  2048
/**
 * @brief Priority level for the Tx Control task.
 */
#define MESHX_TXCM_TASK_PRIO        5
/**
 * @brief Maximum number of signals in the Tx Control signal queue.
 */
#define MESHX_TXCM_SIG_Q_LEN        10
/**
 * @brief Depth (size) of each signal queue entry.
 */
#define MESHX_TXCM_SIG_Q_DEPTH      sizeof(meshx_txcm_request_t)

/**
 * @brief Type definition for signal processing functions in the Tx Control module.
 *
 * A signal processing function takes a pointer to a transmission control request structure
 * and processes the request accordingly.
 */
typedef meshx_err_t (*meshx_txcm_sig_proc_t)(meshx_txcm_request_t *request);

/**
 * @brief Structure for queued transmission items in the Tx Control module.
 *
 * This structure represents an item in the transmission queue, containing the send callback
 * and message parameters ready for transmission.
 */
typedef struct meshx_txcm_tx_q
{
    uint16_t dest_addr;                                 /**< Destination address of the message */
    uint16_t retry_cnt;                                 /**< Retry counter per msg context */
    uint16_t msg_param_len;                             /**< Length of the msg_param */
    meshx_txcm_msg_type_t msg_type;                     /**< Type of message (acknowledged or unacknowledged) */
    meshx_txcm_msg_state_t msg_state;                   /**< State of the message in the transmission queue */
    meshx_txcm_fn_model_send_t send_fn;                 /**< Function pointer to the model-specific send function */
    uint8_t msg_param[MESHX_TXCM_MSG_PARAM_MAX_LEN];    /**< Pointer to model specific parameter structure */
}meshx_txcm_tx_q_t;

/**
 * @brief Structure for the transmission queue in the Tx Control module.
 *
 * This structure represents the circular buffer used to store queued transmission items.
 */
typedef struct {
    int16_t head;                                      /**< Head of the transmission queue */
    int16_t tail;                                      /**< Tail of the transmission queue */
    uint16_t count;                                    /**< Number of items in the transmission queue */
    meshx_txcm_tx_q_t q_param[MESHX_TXCM_TX_Q_LEN];    /**< Array of transmission queue items */
} meshx_tx_queue_t;

/****************************************************************************
 * Private functions Ring buffer
 ****************************************************************************/
static meshx_err_t meshx_tx_queue_is_full(const meshx_tx_queue_t *q);
static meshx_err_t meshx_tx_queue_is_empty(const meshx_tx_queue_t *q);
static meshx_err_t meshx_tx_queue_dequeue(meshx_tx_queue_t *q, meshx_txcm_tx_q_t *item);
static meshx_err_t meshx_tx_queue_peek(const meshx_tx_queue_t *q, meshx_txcm_tx_q_t *item);
static meshx_err_t meshx_tx_queue_enqueue(meshx_tx_queue_t *q, const meshx_txcm_tx_q_t *item);
static meshx_err_t meshx_tx_queue_enqueue_front(meshx_tx_queue_t *q, const meshx_txcm_tx_q_t *item);

/****************************************************************************
 * Private functions Tx Control
 ****************************************************************************/
static void meshx_txcm_task_handler(const dev_struct_t *args);
static meshx_err_t meshx_txcm_sig_resend(meshx_txcm_request_t *request);
static meshx_err_t meshx_txcm_sig_enq_send(meshx_txcm_request_t *request);
static meshx_err_t meshx_txcm_sig_ack(const meshx_txcm_request_t *request);
static meshx_err_t meshx_txcm_sig_direct_send(meshx_txcm_request_t *request);

/**
 * @brief Global structure for Tx Control module state and resources.
 *
 * This static structure manages the initialization state, task configuration,
 * and queues for signals and transmissions in the Tx Control module.
 */
static struct{
    uint16_t init_magic;
    meshx_task_t txcm_task;
    meshx_msg_q_t txcm_sig_queue;
    meshx_tx_queue_t txcm_tx_queue;
} g_txcm = {
    .init_magic = 0,
    .txcm_tx_queue = {
        .head = 0,
        .tail = 0,
        .count = 0,
    },
    .txcm_sig_queue = {
        .max_msg_length = MESHX_TXCM_SIG_Q_LEN,
        .max_msg_depth  = MESHX_TXCM_SIG_Q_DEPTH,
    },
    .txcm_task = {
        .task_name  = "meshx_txcm_task",
        .priority   = MESHX_TXCM_TASK_PRIO,
        .stack_size = MESHX_TXCM_TASK_STACK_SIZE,
        .task_cb    = (meshx_task_cb_t) &meshx_txcm_task_handler,
    }
};

/**
 * @brief Signal processing function table for the Tx Control module.
 *
 * This table maps signal types to their corresponding processing functions.
 * Each entry in the table is a function pointer that handles a specific type of signal.
 * The table is indexed by the signal type (meshx_txcm_sig_t) and contains function pointers
 * to the respective signal processing functions.
 */
static meshx_txcm_sig_proc_t g_sig_proc_table[MESHX_TXCM_SIG_MAX] =
{
    [MESHX_TXCM_SIG_ACK]         = (meshx_txcm_sig_proc_t)&meshx_txcm_sig_ack,
    [MESHX_TXCM_SIG_RESEND]      = (meshx_txcm_sig_proc_t)&meshx_txcm_sig_resend,
    [MESHX_TXCM_SIG_ENQ_SEND]    = (meshx_txcm_sig_proc_t)&meshx_txcm_sig_enq_send,
    [MESHX_TXCM_SIG_DIRECT_SEND] = (meshx_txcm_sig_proc_t)&meshx_txcm_sig_direct_send,
};

/**
 * @brief Checks if the transmission queue is full.
 *
 * This function checks if the count of messages in the transmission queue has reached
 * the maximum capacity.
 *
 * @param[in] q Pointer to the transmission queue structure to be checked.
 *
 * @return true if the queue is full, false otherwise.
 */
static meshx_err_t meshx_tx_queue_is_full(const meshx_tx_queue_t *q)
{
    return q->count == MESHX_TXCM_TX_Q_LEN ? MESHX_SUCCESS : MESHX_NO_MEM;
}

/**
 * @brief Checks if the transmission queue is empty.
 *
 * This function checks if the count of messages in the transmission queue has reached zero.
 *
 * @param[in] q Pointer to the transmission queue structure to be checked.
 *
 * @return true if the queue is empty, false otherwise.
 */
static meshx_err_t meshx_tx_queue_is_empty(const meshx_tx_queue_t *q)
{
    return q->count == 0 ? MESHX_SUCCESS : MESHX_INVALID_STATE;
}

/**
 * @brief Adds an item to the transmission queue.
 *
 * This function adds an item to the transmission queue. The item is a pointer to the
 * meshx_txcm_tx_q_t structure to be added.
 *
 * @param[in] q Pointer to the transmission queue structure to add the item to.
 * @param[in] item Pointer to the meshx_txcm_tx_q_t structure to be added.
 *
 * @return true if the item was successfully added, false if the queue is full.
 */
static meshx_err_t meshx_tx_queue_enqueue(meshx_tx_queue_t *q, const meshx_txcm_tx_q_t *item)
{
    meshx_err_t err = MESHX_SUCCESS;
    err = meshx_tx_queue_is_full(q);
    if (err == MESHX_SUCCESS)
    {
        return MESHX_NO_MEM;
    }

    // Copy the entire item structure into the q_param.
    memcpy(q->q_param + q->tail, item, sizeof(meshx_txcm_tx_q_t));

    q->tail = (q->tail + 1) % MESHX_TXCM_TX_Q_LEN;

    // Increment count.
    q->count++;

    return MESHX_SUCCESS;
}

/**
 * @brief Adds an item to the front of the transmission queue.
 *
 * This function adds an item to the front of the transmission queue. The item is a pointer to the
 * meshx_txcm_tx_q_t structure to be added.
 *
 * @param[in] q Pointer to the transmission queue structure to add the item to.
 * @param[in] item Pointer to the meshx_txcm_tx_q_t structure to be added.
 *
 * @return true if the item was successfully added, false if the queue is full.
 */
static meshx_err_t meshx_tx_queue_enqueue_front(meshx_tx_queue_t *q, const meshx_txcm_tx_q_t *item)
{
    meshx_err_t err = MESHX_SUCCESS;
    err = meshx_tx_queue_is_full(q);
    if (err == MESHX_SUCCESS)
    {
        return MESHX_NO_MEM;
    }
    // Increment tail and wrap around if necessary.
    q->head = (q->head - 1 + MESHX_TXCM_TX_Q_LEN) % MESHX_TXCM_TX_Q_LEN;

    // Copy the entire item structure into the q_param.
    memcpy(q->q_param + q->head, item, sizeof(meshx_txcm_tx_q_t));

    // Increment count.
    q->count++;

    return MESHX_SUCCESS;
}
/**
 * @brief Returns the item at the front of the transmission queue without removing it.
 *
 * This function checks if the transmission queue is empty and if so, returns an error.
 * If not empty, it copies the item at the front of the transmission queue into the provided
 * item pointer.
 *
 * @param[in] q Pointer to the transmission queue structure to peek from.
 * @param[out] item Pointer to the meshx_txcm_tx_q_t structure to store the peeked item.
 *
 * @return meshx_err_t
 */
static meshx_err_t meshx_tx_queue_peek(const meshx_tx_queue_t *q, meshx_txcm_tx_q_t *item)
{
    meshx_err_t err = MESHX_SUCCESS;
    err = meshx_tx_queue_is_empty(q);
    if (err == MESHX_SUCCESS)
    {
        return MESHX_INVALID_STATE;
    }

    // Copy the entire item structure from the q_param.
    memcpy(item, q->q_param + q->head, sizeof(meshx_txcm_tx_q_t));

    return MESHX_SUCCESS;
}

/**
 * @brief Searches for a parameter in the transmission queue.
 *
 * This function searches for a parameter in the transmission queue. The parameter is a pointer to
 * a uint8_t array with a length of param_len. The function searches the queue from the tail
 * to the head and returns MESHX_SUCCESS if the parameter is found. If not found, it returns
 * MESHX_NOT_FOUND.
 *
 * @param[in] q Pointer to the transmission queue structure to search in.
 * @param[in] param Pointer to the uint8_t array to search for.
 *
 * @return meshx_err_t
 */
static meshx_err_t meshx_tx_queue_search(
    const meshx_tx_queue_t *q,
    const uint8_t *param,
    uint16_t dest_addr)
{
    meshx_err_t err = MESHX_SUCCESS;
    err = meshx_tx_queue_is_empty(q);
    if (err == MESHX_SUCCESS)
    {
        return MESHX_INVALID_STATE;
    }

    int16_t head = q->head;
    int16_t tail = q->tail;

    while(head != tail)
    {
        // Move tail back
        tail = (tail - 1 + MESHX_TXCM_TX_Q_LEN) % MESHX_TXCM_TX_Q_LEN;
        // compare param with q_param[tail]
        if (strcmp((const char*)param, (const char*)q->q_param[tail].msg_param) == 0
            && q->q_param[tail].dest_addr == dest_addr)
        {
            MESHX_LOGD(MODULE_ID_TXCM, "Found param in queue");
            return MESHX_SUCCESS;
        }
    }

    return MESHX_NOT_FOUND;
}
/**
 * @brief Removes an item from the transmission queue.
 *
 * This function removes an item from the transmission queue. The item is a pointer to the
 * meshx_txcm_tx_q_t structure to be removed.
 *
 * @param[in] q Pointer to the transmission queue structure to remove the item from.
 * @param[out] item Pointer to the meshx_txcm_tx_q_t structure to store the removed item.
 *
 * @return meshx_err_t
 */
static meshx_err_t meshx_tx_queue_dequeue(meshx_tx_queue_t *q, meshx_txcm_tx_q_t *item)
{
    meshx_err_t err = meshx_tx_queue_peek(q, item);
    if (err != MESHX_SUCCESS)
    {
        return err;
    }

    // Move head forward
    q->head = (q->head + 1) % MESHX_TXCM_TX_Q_LEN;

    // Decrement count
    q->count--;

    return MESHX_SUCCESS;
}

/**
 * @brief Tickles the Tx Control module to process the front of the transmission queue.
 *
 * This function checks if the front message in the transmission queue is ready to be sent.
 * If the message state is 'NEW', it processes the send function and updates the message state
 * to 'WAITING_ACK' before requeuing the message to the front of the transmission queue.
 *
 * @param[in] resend    Is resend
 *
 * @return meshx_err_t
 */
static meshx_err_t meshx_txcm_msg_q_front_try_send(bool resend)
{
    meshx_err_t err = MESHX_SUCCESS;
    meshx_txcm_tx_q_t front_tx;

    MESHX_LOGD(MODULE_ID_TXCM, "TXCM_Q Stat: %x|%x|%x", g_txcm.txcm_tx_queue.head, g_txcm.txcm_tx_queue.tail, g_txcm.txcm_tx_queue.count);
    if (resend == false && (meshx_tx_queue_peek(&g_txcm.txcm_tx_queue, &front_tx) != MESHX_SUCCESS))
    {
        return MESHX_SUCCESS;
    }

    if (!(resend || front_tx.msg_state == MESHX_TXCM_MSG_STATE_NEW))
    {
        return MESHX_SUCCESS;
    }
    if(resend)
    {
        MESHX_LOGD(MODULE_ID_TXCM, "Try to send message from Tx Control Tx Queue resend|state:%d|%d", resend,front_tx.msg_state);
    }
    err = meshx_tx_queue_dequeue(&g_txcm.txcm_tx_queue, &front_tx);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_TXCM, "Failed to receive message from Tx Control Tx Queue: %p", (void *)err);
        return err;
    }

    if ((int16_t)front_tx.retry_cnt-- <= 0)
    {
        front_tx.msg_state = MESHX_TXCM_MSG_STATE_NACK;
        return MESHX_TIMEOUT;
    }

    front_tx.msg_state = MESHX_TXCM_MSG_STATE_SENDING;
    if (front_tx.send_fn != NULL)
    {
        err = front_tx.send_fn(front_tx.msg_param, front_tx.msg_param_len);
        if(err != MESHX_SUCCESS)
        {
            front_tx.msg_state = MESHX_TXCM_MSG_STATE_NACK;
            return err;
        }
    }

    if (front_tx.msg_type == MESHX_TXCM_MSG_TYPE_ACKED)
    {
        /* Requeue to TX Front as we are waiting for Ack */
        front_tx.msg_state = MESHX_TXCM_MSG_STATE_WAITING_ACK;
        err = meshx_tx_queue_enqueue_front(&g_txcm.txcm_tx_queue, &front_tx);
        if (err)
        {
            MESHX_LOGE(MODULE_ID_TXCM, "Failed to send message to Tx Control Tx Queue: %p", (void *)err);
            return err;
        }
    }
    else
    {
        /* Un-Acked msg need not to be retained */
        MESHX_DO_NOTHING;
    }
    return err;
}

/**
 * @brief Processes a request message for the Tx Control module.
 *
 * This function handles the processing of a request message for the Tx Control module.
 * It checks if the request is valid, creates a new transmission entry with a sequence number,
 * and then queues the message to the transmission queue. If the queued message is at the front
 * of the queue, it immediately processes the send function.
 *
 * @param[in] request Pointer to the transmission control request structure containing the send msg
 * @param[in] msg_type Type of the message (ACKED or UNACKED)
 *
 * @return meshx_err_t
 */
static meshx_err_t meshx_txcm_proccess_request_msg(
    meshx_txcm_request_t *request,
    meshx_txcm_msg_type_t msg_type)
{
    meshx_err_t err = MESHX_SUCCESS;
    static meshx_txcm_tx_q_t new_tx;

    if (   request == NULL
        || request->send_fn == NULL
        || msg_type >= MESHX_TXCM_MSG_TYPE_MAX
        || request->msg_param_len >= MESHX_TXCM_MSG_PARAM_MAX_LEN
    )
    {
        return MESHX_INVALID_ARG;
    }

    MESHX_LOGD(MODULE_ID_TXCM, "Processing a new request");

    err = meshx_tx_queue_search(&g_txcm.txcm_tx_queue, request->msg_param, request->dest_addr);
    if (err == MESHX_SUCCESS)
    {
        MESHX_LOGD(MODULE_ID_TXCM, "Message already in queue");
        return MESHX_SUCCESS;
    }

    memset(&new_tx, 0, sizeof(meshx_txcm_tx_q_t));

    new_tx.msg_type      = msg_type;
    new_tx.send_fn       = request->send_fn;
    new_tx.dest_addr     = request->dest_addr;
    new_tx.msg_param_len = request->msg_param_len;
    new_tx.msg_state     = MESHX_TXCM_MSG_STATE_NEW;
    new_tx.retry_cnt     = MESHX_TXCM_MSG_RETRY_MAX;

    memcpy(new_tx.msg_param, request->msg_param, request->msg_param_len);

    err = meshx_tx_queue_enqueue(&g_txcm.txcm_tx_queue, &new_tx);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_TXCM, "Failed to send message to Tx Control Tx Queue: %p", (void *)err);
        return err;
    }

    err = meshx_txcm_msg_q_front_try_send(false);
    if(err)
    {
        MESHX_LOGE(MODULE_ID_TXCM, "Failed to process front of Tx Control Tx Queue: %p", (void *)err);
    }
    return err;
}

/**
 * @brief Enqueues a send request to the Tx Control module.
 *
 * This function adds a send request to the transmission queue of the Tx Control module.
 * It checks if the request is valid, creates a new transmission entry with a sequence number,
 * and then queues the message to the transmission queue. If the queued message is at the front
 * of the queue, it immediately processes the send function.
 *
 * @param[in] request Pointer to the transmission control request structure containing the send msg
 *
 * @return meshx_err_t
 */
static meshx_err_t meshx_txcm_sig_enq_send(meshx_txcm_request_t *request)
{
    MESHX_LOGD(MODULE_ID_TXCM, "Enqueuing a new request");
    return meshx_txcm_proccess_request_msg(request, MESHX_TXCM_MSG_TYPE_ACKED);
}

/**
 * @brief Enqueues a direct send request to the Tx Control module.
 *
 * This function adds a direct send request to the transmission queue of the Tx Control module.
 * It checks if the request is valid, creates a new transmission entry with a sequence number,
 * and then queues the message to the transmission queue. If the queued message is at the front
 * of the queue, it immediately processes the send function.
 *
 * @param[in] request Pointer to the transmission control request structure containing the send msg
 *
 * @return meshx_err_t
 */
static meshx_err_t meshx_txcm_sig_direct_send(meshx_txcm_request_t *request)
{
    MESHX_LOGD(MODULE_ID_TXCM, "Processing a new direct request");
    /* The same path can take care as the msg type  */
    return meshx_txcm_proccess_request_msg(request, MESHX_TXCM_MSG_TYPE_UNACKED);
}

/**
 * @brief Handles the resend signal for the Tx Control module.
 *
 * @param[in] request   Pointer to the transmission control request structure containing the send msg
 *
 * @return meshx_err_t
 */
static meshx_err_t meshx_txcm_sig_resend(meshx_txcm_request_t *request)
{
    MESHX_UNUSED(request);
    meshx_err_t err = MESHX_SUCCESS;

    MESHX_LOGD(MODULE_ID_TXCM, "Processing a retry");
    err = meshx_txcm_msg_q_front_try_send(true);
    if(err == MESHX_TIMEOUT)
    {
        MESHX_LOGD(MODULE_ID_TXCM, "Timeout");
        err = control_task_msg_publish(
            CONTROL_TASK_MSG_CODE_TXCM,
            CONTROL_TASK_MSG_EVT_TXCM_MSG_TIMEOUT,
            request->msg_param,
            request->msg_param_len
        );
        if(err)
        {
            MESHX_LOGE(MODULE_ID_TXCM, "Failed to process front of Tx Control Tx Queue: %p", (void *)err);
        }
        err = meshx_txcm_msg_q_front_try_send(false);
    }
    meshx_rtos_free(&request->msg_param);
    return err;
}

/**
 * @brief Handles the flush signal for the Tx Control module.
 *
 * This function clears all messages from the transmission queue of the Tx Control module.
 * It removes only front message from the queue and returns the status of the operation.
 *
 * @param[in] request Pointer to the transmission control request structure containing the flush request
 *
 * @return meshx_err_t
 */
static meshx_err_t meshx_txcm_sig_ack(const meshx_txcm_request_t *request)
{
    MESHX_UNUSED(request);
    meshx_err_t err = MESHX_SUCCESS;

    MESHX_LOGD(MODULE_ID_TXCM, "Processing an ack");
    meshx_txcm_tx_q_t front_tx;
    if(meshx_tx_queue_peek(&g_txcm.txcm_tx_queue, &front_tx) == MESHX_SUCCESS)
    {
        if(front_tx.dest_addr == request->dest_addr)
        {
            err = meshx_tx_queue_dequeue(&g_txcm.txcm_tx_queue, &front_tx);
            if (err)
            {
                MESHX_LOGE(MODULE_ID_TXCM, "Failed to receive message from Tx Control Tx Queue: %p", (void *)err);
                return err;
            }
            MESHX_LOGD(MODULE_ID_TXCM, "Received message from Tx Control Tx Queue");
            front_tx.msg_state = MESHX_TXCM_MSG_STATE_ACK;
        }
        else
        {
            MESHX_LOGW(MODULE_ID_TXCM, "ACK received from unknown address, dropping packet: %d", front_tx.dest_addr);
            MESHX_DO_NOTHING;
        }
    }
    err = meshx_rtos_free(request->msg_param);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_TXCM, "RTOS Free failed: %p", (void *)err);
    }
    err = meshx_txcm_msg_q_front_try_send(false);
    return err;
}

/**
 * @brief Handler function for the MeshX TXCM task.
 *
 * This static function processes transmission commands for the MeshX component.
 * It is intended to be run as a task handler, typically within a FreeRTOS or similar
 * multitasking environment.
 *
 * @param[in] args Pointer to a device structure (dev_struct_t) containing context or parameters
 *             required for the task operation.
 *
 * @note This function is not intended to be called directly from outside this source file.
 */
static void meshx_txcm_task_handler(const dev_struct_t *args)
{
    MESHX_UNUSED(args);
    MESHX_LOGI(MODULE_ID_TXCM, "MeshX Tx Control Task started");
    meshx_err_t err = MESHX_SUCCESS;

    while(true)
    {
        meshx_txcm_request_t request;
        /* Wait for a signal from the signal queue */
        if(meshx_msg_q_recv(&g_txcm.txcm_sig_queue, &request, UINT32_MAX) != MESHX_SUCCESS)
        {
            MESHX_LOGE(MODULE_ID_TXCM, "Failed to receive signal from Tx Control Signal Queue");
            continue;
        }
        MESHX_LOGD(MODULE_ID_TXCM, "Processing sig: %d", request.request_type);

        /* Process the signal based on the request type */
        err = g_sig_proc_table[request.request_type](&request);
        if(err)
        {
            MESHX_LOGE(MODULE_ID_TXCM, "Failed to process the request (%p): %p",(void*)request.request_type, (void *)err);
        }
    }
}

/**
 * @brief Initializes the MeshX Tx Control Module.
 *
 * This function sets up the transmission control module for MeshX. It checks if the module
 * has already been initialized using an initialization magic value. If not initialized,
 * it sets the magic value, assigns the provided device structure to the task argument,
 * and creates the Tx Control task.
 *
 * @param[in] pdev Pointer to the device structure to be used by the Tx Control Module.
 *
 * @return
 *      - MESHX_SUCCESS on successful initialization or if already initialized.
 *      - Error code (meshx_err_t) if task creation fails.
 */
meshx_err_t meshx_txcm_init(dev_struct_t *pdev)
{
    MESHX_LOGI(MODULE_ID_TXCM, "Initializing MeshX Tx Control Module");
    if(g_txcm.init_magic == MESHX_TXCM_INIT_MAGIC)
    {
        return MESHX_SUCCESS;
    }
    meshx_err_t err = MESHX_SUCCESS;
    g_txcm.init_magic = MESHX_TXCM_INIT_MAGIC;
    g_txcm.txcm_task.arg = pdev;

    /* Create Control Signal Queue */
    err = meshx_msg_q_create(&g_txcm.txcm_sig_queue);
    if(err)
    {
        MESHX_LOGE(MODULE_ID_TXCM, "Failed to create Tx Control Signal Queue: %p", (void *)err);
        return err;
    }

    err = meshx_task_create(&g_txcm.txcm_task);
    if(err)
    {
        MESHX_LOGE(MODULE_ID_TXCM, "Failed to create Tx Control task: %p", (void *)err);
        return err;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Sends a request to the Tx Control module.
 *
 * This function sends a request to the Tx Control module with the specified parameters.
 * It creates a new request structure, copies the message parameters, and sends the request
 * to the signal queue of the Tx Control module.
 *
 * @param[in] request_type  Type of the request (ACK, RESEND, ENQ_SEND, DIRECT_SEND)
 * @param[in] dest_addr     Destination address of the message
 * @param[in] msg_param     Pointer to the message parameters
 * @param[in] msg_param_len Length of the message parameters
 * @param[in] send_fn       Function pointer to the send function to be used for the request
 *
 * @return meshx_err_t
 */
meshx_err_t meshx_txcm_request_send(
    meshx_txcm_sig_t request_type,
    uint16_t dest_addr,
    meshx_cptr_t msg_param,
    uint16_t msg_param_len,
    meshx_txcm_fn_model_send_t send_fn
)
{
    meshx_err_t err = MESHX_SUCCESS;
    static meshx_txcm_request_t new_req;
    memset(&new_req, 0, sizeof(meshx_txcm_request_t));

    /* Prepare request message */
    new_req.dest_addr     = dest_addr;
    new_req.send_fn       = send_fn;
    new_req.request_type  = request_type;
    new_req.msg_param_len = msg_param_len;

    if(msg_param_len != 0 && msg_param != NULL)
    {
        /* Retain message context */
        err = meshx_rtos_malloc(&new_req.msg_param, msg_param_len);
        if (err)
        {
            MESHX_LOGE(MODULE_ID_TXCM, "Malloc Failure: %p", (void *)err);
            return err;
        }
        memcpy(new_req.msg_param, msg_param, msg_param_len);
    }
    else
    {
        new_req.msg_param = NULL;
    }
    err = meshx_msg_q_send(&g_txcm.txcm_sig_queue, &new_req, sizeof(meshx_txcm_request_t), 0);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_TXCM, "TXCM Signal failed: %p", (void *)err);
        meshx_rtos_free(&new_req.msg_param);
        return err;
    }
    return err;
}

/**
 * @brief Registers a callback function for handling Tx Control module events.
 *
 * This function registers a callback function to handle specific events from the Tx Control module.
 * The callback will be invoked when the specified event occurs.
 *
 * @param[in] event_cb Pointer to the callback function to be registered for event handling.
 *
 * @return meshx_err_t
 *      - MESHX_SUCCESS on successful registration.
 *      - Error code (meshx_err_t) if registration fails.
 */
meshx_err_t meshx_txcm_event_cb_reg(meshx_txcm_cb_t event_cb)
{
    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_TXCM,
        CONTROL_TASK_MSG_EVT_TXCM_MSG_TIMEOUT,
        event_cb);
}
#endif /* CONFIG_TXCM_ENABLE */
