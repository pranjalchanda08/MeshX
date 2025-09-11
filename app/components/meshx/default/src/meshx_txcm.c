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
#include "app_common.h"
#include "meshx_txcm.h"
#include "interface/rtos/meshx_task.h"
#include "sys/queue.h"

#if CONFIG_TXCM_ENABLE
/**
 * @brief Magic value used to check if the Tx Control module has been initialized.
 */
#define MESHX_TXCM_INIT_MAGIC       0x4455

typedef meshx_err_t (*meshx_txcm_sig_proc_t)(meshx_txcm_request_t *request);

static void meshx_txcm_task_handler(const dev_struct_t *args);
static meshx_err_t meshx_txcm_sig_ack(const meshx_txcm_request_t *request);
static meshx_err_t meshx_txcm_sig_resend(const meshx_txcm_request_t *request);
static meshx_err_t meshx_txcm_sig_enq_send(meshx_txcm_request_t *request);
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
    meshx_msg_q_t txcm_tx_queue;
    meshx_msg_q_t txcm_sig_queue;
} g_txcm = {
    .init_magic = 0,
    .txcm_tx_queue = {
        .max_msg_length = MESHX_TXCM_TX_Q_LEN,
        .max_msg_depth  = MESHX_TXCM_TX_Q_DEPTH,
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
    /* Peek at the Front of TX Queue and if the sequence no is equal to new_tx.seq_no then process send */
    if (meshx_msg_q_peek(&g_txcm.txcm_tx_queue, &front_tx, 0) == MESHX_SUCCESS)
    {
        if(resend || front_tx.msg_state == MESHX_TXCM_MSG_STATE_NEW)
        {
            err = meshx_msg_q_recv(&g_txcm.txcm_tx_queue, &front_tx, 0);
            if (err)
            {
                MESHX_LOGE(MODULE_ID_TXCM, "Failed to receive message from Tx Control Tx Queue: %p", (void *)err);
                return err;
            }
            front_tx.msg_state = MESHX_TXCM_MSG_STATE_SENDING;
            if (front_tx.send_fn != NULL)
            {
                front_tx.send_fn(front_tx.msg_param, front_tx.msg_param_len);
            }
            if(front_tx.msg_type == MESHX_TXCM_MSG_TYPE_ACKED)
            {
                /* Requeue to TX Front as we are waiting for Ack */
                front_tx.msg_state = MESHX_TXCM_MSG_STATE_WAITING_ACK;
                err = meshx_msg_q_send_front(&g_txcm.txcm_tx_queue, &front_tx, sizeof(front_tx), 0);
                if (err)
                {
                    MESHX_LOGE(MODULE_ID_TXCM, "Failed to send message to Tx Control Tx Queue: %p", (void *)err);
                    return err;
                }
            }
            else
            {
                /* Un-Acked msg need not to be retained */
                err = meshx_rtos_free(&front_tx.msg_param);
            }
        }
        else
        {
            /* Other states are dependent on further signals */
            MESHX_DO_NOTHING;
        }
    }
    else
    {
        /* Do nothing as the front msg is waiting on its respective signal */
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
static meshx_err_t meshx_txcm_proccess_request_msg(meshx_txcm_request_t *request, meshx_txcm_msg_type_t msg_type)
{
    meshx_err_t err = MESHX_SUCCESS;

    if (request == NULL || msg_type >= MESHX_TXCM_MSG_TYPE_MAX || request->send_fn == NULL)
    {
        return MESHX_INVALID_ARG;
    }

    static meshx_txcm_tx_q_t new_tx;
    memset(&new_tx, 0, sizeof(meshx_txcm_tx_q_t));

    new_tx.msg_state = MESHX_TXCM_MSG_STATE_NEW;
    new_tx.msg_type = msg_type;
    new_tx.send_fn = request->send_fn;
    new_tx.msg_param = request->msg_param;
    new_tx.msg_param_len = request->msg_param_len;

    /* Queue the message to the TX Queue back*/
    err = meshx_msg_q_send(&g_txcm.txcm_tx_queue, &new_tx, sizeof(new_tx), 0);
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
static meshx_err_t meshx_txcm_sig_resend(const meshx_txcm_request_t *request)
{
    MESHX_UNUSED(request);
    return meshx_txcm_msg_q_front_try_send(true);
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

    meshx_txcm_tx_q_t front_tx;
    if(meshx_msg_q_peek(&g_txcm.txcm_tx_queue, &front_tx, 0) == MESHX_SUCCESS)
    {
        err = meshx_msg_q_recv(&g_txcm.txcm_tx_queue, &front_tx, 0);
        if (err)
        {
            MESHX_LOGE(MODULE_ID_TXCM, "Failed to receive message from Tx Control Tx Queue: %p", (void *)err);
            return err;
        }
    }
    err = meshx_rtos_free(&front_tx.msg_param);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_TXCM, "RTOS Free failed: %p", (void *)err);
    }
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
    MESHX_LOGD(MODULE_ID_TXCM, "Initializing MeshX Tx Control Module");
    if(g_txcm.init_magic == MESHX_TXCM_INIT_MAGIC)
    {
        return MESHX_SUCCESS;
    }
    meshx_err_t err = MESHX_SUCCESS;
    g_txcm.init_magic = MESHX_TXCM_INIT_MAGIC;
    g_txcm.txcm_task.arg = pdev;

    /* Create TX Queue */
    err = meshx_msg_q_create(&g_txcm.txcm_tx_queue);
    if(err)
    {
        MESHX_LOGE(MODULE_ID_TXCM, "Failed to create Tx Control Tx Queue: %p", (void *)err);
        return err;
    }
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
 * @param[in] msg_param     Pointer to the message parameters
 * @param[in] msg_param_len Length of the message parameters
 * @param[in] send_fn       Function pointer to the send function to be used for the request
 *
 * @return meshx_err_t
 */
meshx_err_t meshx_txcm_request_send(
    meshx_txcm_sig_t request_type,
    meshx_cptr_t msg_param,
    size_t msg_param_len,
    meshx_txcm_fn_model_send_t send_fn
)
{
    meshx_err_t err = MESHX_SUCCESS;
    static meshx_txcm_request_t new_req;
    memset(&new_req, 0, sizeof(meshx_txcm_request_t));

    /* Prepare request message */
    new_req.send_fn       = send_fn;
    new_req.request_type  = request_type;
    new_req.msg_param_len = msg_param_len;

    if(msg_param_len != 0 && msg_param != NULL)
    {
        /* Retain message context */
        err = meshx_rtos_calloc(&new_req.msg_param, 1, msg_param_len);
        if (err)
        {
            MESHX_LOGE(MODULE_ID_TXCM, "Malloc Failure: %p", (void *)err);
            return err;
        }
        memcpy(new_req.msg_param, msg_param, msg_param_len);
    }
    err = meshx_msg_q_send(&g_txcm.txcm_sig_queue, &new_req, sizeof(meshx_txcm_request_t), 0);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_TXCM, "TXCM Signal failed: %p", (void *)err);
        return err;
    }
    return err;
}

#endif /* CONFIG_TXCM_ENABLE */
