/**
 * @copyright Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_txcm.h
 * @brief MeshX Tx Control Module
 * This header file contains the definitions and function prototypes for the MeshX Tx Control Module,
 * which is responsible for managing transmission control in the MeshX BLE Mesh stack.
 *
 * @author Pranjal Chanda
 *
 */

#ifndef __MESHX_TXCM_H__
#define __MESHX_TXCM_H__

#include <stdint.h>
#include <stddef.h>
#include "meshx_err.h"
#include "app_common.h"
#include "meshx_control_task.h"
#include "interface/ble_mesh/meshx_ble_mesh_cmn_def.h"

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
 * @brief Maximum number of transmission items in the Tx queue.
 */
#define MESHX_TXCM_TX_Q_LEN         10
/**
 * @brief Depth (size) of each transmission queue entry.
 */
#define MESHX_TXCM_TX_Q_DEPTH       sizeof(meshx_txcm_tx_q_t)

typedef control_task_msg_handle_t meshx_txcm_cb_t;
/**
 * @brief Enumeration of signal types for the Tx Control Module.
 *
 * This enumeration defines the different types of signals that can be sent to the Tx Control Module,
 * each representing a different type of transmission command.
 */
typedef enum
{
    MESHX_TXCM_SIG_ENQ_SEND = 0,      /**< Signal to enqueue a transmission command */
    MESHX_TXCM_SIG_DIRECT_SEND,       /**< Signal to directly send a transmission command without queuing */
    MESHX_TXCM_SIG_RESEND,            /**< Signal to resend the last transmission queued message */
    MESHX_TXCM_SIG_ACK,               /**< Signal to acknowledge the last transmission message */
    MESHX_TXCM_SIG_MAX,               /**< Maximum signal type value */
} meshx_txcm_sig_t;

/**
 * @brief Enumeration of message states for the Tx Control Module.
 *
 * This enumeration defines the different states a message can be in during transmission,
 * including new messages, waiting for acknowledgment, acknowledged messages, and the maximum state value.
 */
typedef enum
{
    MESHX_TXCM_MSG_STATE_NEW = 0,       /**< New message state */
    MESHX_TXCM_MSG_STATE_SENDING,       /**< Message sending state */
    MESHX_TXCM_MSG_STATE_WAITING_ACK,   /**< Message waiting for acknowledgment state */
    MESHX_TXCM_MSG_STATE_ACK,           /**< Message acknowledged state */
    MESHX_TXCM_MSG_STATE_MAX,
} meshx_txcm_msg_state_t;

typedef enum
{
    MESHX_TXCM_MSG_TYPE_ACKED,
    MESHX_TXCM_MSG_TYPE_UNACKED,
    MESHX_TXCM_MSG_TYPE_MAX
}meshx_txcm_msg_type_t;
/**
 * @brief Function pointer the Model client layer needs to provide for the msg to be sent for both MESHX_TXCM_SIG_ENQ_SEND and MESHX_TXCM_SIG_DIRECT_SEND
 *
 * @param[in] param Pointer to the model specific parameter structure
 */
typedef meshx_err_t (*meshx_txcm_fn_model_send_t)(meshx_cptr_t msg_param, size_t msg_param_len);

/**
 * @brief Structure for Tx Control module requests.
 *
 * This structure holds the details of a transmission request, including the type of signal,
 * the send function callback, and parameters for the message to be transmitted.
 */
typedef struct meshx_txcm_request
{
    meshx_txcm_sig_t request_type;      /**< Type of transmission command request */
    uint16_t dest_addr;                 /**< Destination address of the message */
    meshx_ptr_t msg_param;              /**< Pointer to model specific parameter structure */
    size_t msg_param_len;               /**< Length of the msg_param */
    meshx_txcm_fn_model_send_t send_fn; /**< Function pointer to the model-specific send function */
} meshx_txcm_request_t;

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
meshx_err_t meshx_txcm_init(dev_struct_t *pdev);

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
    size_t msg_param_len,
    meshx_txcm_fn_model_send_t send_fn
);

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
meshx_err_t meshx_txcm_event_cb_reg(meshx_txcm_cb_t event_cb);

#endif /* __MESHX_TXCM_H__ */
