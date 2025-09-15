/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file meshx_gen_client.c
 * @brief Implementation of the MeshX generic client model for BLE mesh nodes.
 *        This file contains functions for registering, deregistering, and
 *        initializing the generic client model.
 *
 * The MeshX generic client model provides an interface for handling BLE mesh
 * client operations, including callback registration and initialization.
 *
 * @author Pranjal Chanda
 *
 */
#include "stdlib.h"
#include "meshx_txcm.h"
#include "meshx_gen_client.h"

#if CONFIG_ENABLE_GEN_CLIENT
#define MESHX_CLIENT_INIT_MAGIC_NO 0x1121
/**
 * @struct meshx_gen_cli_cb_reg
 * @brief Structure containing the model ID and callback function for generic client model registrations.
 *
 * This structure is used to store the model ID and callback function associated with a generic client model registration.
 */
typedef struct meshx_gen_cli_cb_reg
{
    uint16_t model_id;          /**< Model ID associated with the registration. */
    meshx_gen_client_cb_t cb;   /**< Callback function associated with the registration. */
}meshx_gen_cli_cb_reg_t;

/**
 * @struct meshx_gen_client_msg_ctx
 * @brief Structure containing the message context for generic client model messages.
 *
 * This structure is used to store the message context for generic client model messages, including the model context, state parameters, opcode, destination address, network index, and application key index.
 */
typedef struct meshx_gen_client_msg_ctx
{
    meshx_ptr_t model;              /**< Model context associated with the message. */
    uint16_t opcode;                /**< Opcode associated with the message. */
    uint16_t addr;                  /**< Destination address associated with the message. */
    uint16_t net_idx;               /**< Network index associated with the message. */
    uint16_t app_idx;               /**< Application key index associated with the message. */
    meshx_gen_cli_set_t state;      /**< State parameters associated with the message. */
}meshx_gen_client_msg_ctx_t;

/**
 * @struct g_meshx_client_control
 * @brief Structure containing control variables for the generic client model.
 *
 * This structure is used to store control variables for the generic client model, including the mesh client initialization flag and the number of messages waiting for acknowledgement.
 */
static struct{
    uint16_t meshx_client_init;                 /**< Flag indicating whether the mesh client has been initialized. */
    uint16_t gen_client_msg_waiting_for_ack;    /**< Number of messages waiting for acknowledgement. */
} g_meshx_client_control;

/**
 * @struct meshx_gen_cli_cb_reg_node
 * @brief Structure containing a node in the linked list of registered callbacks.
 *
 * This structure is used to store a node in the linked list of registered callbacks, including a pointer to the next node and the registration information.
 */
typedef struct meshx_gen_cli_cb_reg_node
{
    meshx_gen_cli_cb_reg_t reg;             /**< Registration information associated with the node. */
    struct meshx_gen_cli_cb_reg_node *next; /**< Pointer to the next node in the linked list. */
} meshx_gen_cli_cb_reg_node_t;

static meshx_gen_cli_cb_reg_node_t *g_meshx_gen_cli_cb_reg_head = NULL;

/**
 * @brief Adds a new callback registration to the linked list of registered callbacks.
 *
 * This function allocates a new node in the linked list of registered callbacks and
 * initializes it with the provided registration information.
 *
 * @param[in] reg The structure containing the model ID and callback function to add to the linked list.
 */
static meshx_err_t meshx_gen_cli_cb_reg_add(meshx_gen_cli_cb_reg_t reg)
{
    meshx_gen_cli_cb_reg_node_t *node = (meshx_gen_cli_cb_reg_node_t*) MESHX_MALLOC(sizeof(meshx_gen_cli_cb_reg_node_t));
    if (!node)
        return MESHX_NO_MEM;

    node->reg = reg;
    node->next = g_meshx_gen_cli_cb_reg_head;
    g_meshx_gen_cli_cb_reg_head = node;

    return MESHX_SUCCESS;
}

/**
 * @brief Checks if the given opcode corresponds to a GET request in the Generic Client group.
 *
 * This function determines whether the provided opcode is part of the set of GET requests
 * defined for the Generic Client group in the MeshX protocol.
 *
 * @param[in] opcode The opcode to check.
 * @return meshx_err_t Returns an error code indicating whether the opcode is a GET request in the Generic Client group.
 */
static meshx_err_t meshx_is_gen_cli_get_opcode(uint16_t opcode)
{
    switch(opcode)
    {
        case MESHX_MODEL_OP_GEN_ONOFF_GET:
        case MESHX_MODEL_OP_GEN_LEVEL_GET:
        case MESHX_MODEL_OP_GEN_ONPOWERUP_GET:
        case MESHX_MODEL_OP_GEN_POWER_LEVEL_GET:
        case MESHX_MODEL_OP_GEN_BATTERY_GET:
        case MESHX_MODEL_OP_GEN_LOC_GLOBAL_GET:
        case MESHX_MODEL_OP_GEN_LOC_LOCAL_GET:
        case MESHX_MODEL_OP_GEN_MANUFACTURER_PROPERTIES_GET:
        case MESHX_MODEL_OP_GEN_MANUFACTURER_PROPERTY_GET:
        case MESHX_MODEL_OP_GEN_ADMIN_PROPERTIES_GET:
        case MESHX_MODEL_OP_GEN_ADMIN_PROPERTY_GET:
        case MESHX_MODEL_OP_GEN_USER_PROPERTIES_GET:
        case MESHX_MODEL_OP_GEN_USER_PROPERTY_GET:
        case MESHX_MODEL_OP_GEN_CLIENT_PROPERTIES_GET:
            return MESHX_SUCCESS;
        default:
            return MESHX_FAIL;
    }
}
/**
 * @brief Checks if the given model ID corresponds to a Generic Client model.
 *
 * This function determines whether the specified model ID is associated with a
 * Generic Client model in the MeshX framework.
 *
 * @param[in] model_id The model ID to be checked.
 * @return meshx_err_t Returns an error code indicating whether the model ID is a Generic Client model.
 */
static meshx_err_t meshx_is_gen_cli_model(uint32_t model_id)
{
    switch (model_id)
    {
        case MESHX_MODEL_ID_GEN_ONOFF_CLI:
        case MESHX_MODEL_ID_GEN_LEVEL_CLI:
        case MESHX_MODEL_ID_GEN_POWER_ONOFF_CLI:
        case MESHX_MODEL_ID_GEN_POWER_LEVEL_CLI:
        case MESHX_MODEL_ID_GEN_BATTERY_CLI:
        case MESHX_MODEL_ID_GEN_LOCATION_CLI:
            return MESHX_SUCCESS;
        default:
            return MESHX_FAIL;
    }
}

/**
 * @brief Checks if the given opcode corresponds to an unacknowledged (unack) message.
 *
 * This function determines whether the provided opcode represents a SET_UNACK operation
 * for various Generic models in the BLE Mesh specification. It returns success if the
 * opcode matches one of the defined unacknowledged set opcodes.
 *
 * @param[in] opcode The mesh model opcode to check (uint32_t).
 *
 * @return MESHX_SUCCESS if the opcode is a recognized unacknowledged set opcode
 *         (e.g., Generic OnOff Set Unack, Generic Level Set Unack, etc.).
 * @return MESHX_FAIL if the opcode does not match any unacknowledged set opcode.
 *
 * @note This utility function is used internally for handling reliable vs. non-reliable
 *       messaging in Generic Client models. Unacknowledged messages do not expect
 *       a response from the server.
 * @note Aligns with Bluetooth SIG Mesh Generic models opcodes (e.g., 0x8201 for
 *       Generic OnOff Set Unacknowledged).
 */
static meshx_err_t meshx_is_unack_opcode(uint32_t opcode)
{
    switch(opcode)
    {
        case MESHX_MODEL_OP_GEN_ONOFF_SET_UNACK:
        case MESHX_MODEL_OP_GEN_LEVEL_SET_UNACK:
        case MESHX_MODEL_OP_GEN_ONPOWERUP_SET_UNACK:
        case MESHX_MODEL_OP_GEN_POWER_LEVEL_SET_UNACK:
        case MESHX_MODEL_OP_GEN_LOC_GLOBAL_SET_UNACK:
        case MESHX_MODEL_OP_GEN_LOC_LOCAL_SET_UNACK:
        case MESHX_MODEL_OP_GEN_MANUFACTURER_PROPERTY_SET_UNACK:
        case MESHX_MODEL_OP_GEN_ADMIN_PROPERTY_SET_UNACK:
        case MESHX_MODEL_OP_GEN_USER_PROPERTY_SET_UNACK:
            return MESHX_SUCCESS;
        default:
            return MESHX_FAIL;
    }
}

/**
 * @brief Transmit callback handler for sending Generic Client model messages.
 *
 * This function processes the generic client message context and forwards it to the
 * platform-specific BLE Mesh generic client send function for transmission. It validates
 * the input parameters before invoking the core send function.
 *
 * @param[in] msg_param Pointer to the message context structure
 *                      (meshx_gen_client_msg_ctx_t). Must not be NULL.
 * @param[in] msg_param_len Length of the message parameter data in bytes.
 *                          Must equal sizeof(meshx_gen_client_msg_ctx_t).
 *
 * @return MESHX_SUCCESS on successful validation and forwarding.
 * @return MESHX_INVALID_ARG if msg_param is NULL or msg_param_len is invalid.
 * @return Other error codes from meshx_plat_gen_cli_send_msg() if transmission fails.
 *
 * @note This is an internal static callback registered with the TXCM module for
 *       generic client models. Assumes the caller has initialized the
 *       meshx_gen_client_msg_ctx_t structure with valid model, state, opcode,
 *       addresses, and keys.
 * @note The last parameter to meshx_plat_gen_cli_send_msg indicates whether the
 *       opcode corresponds to a GET operation (based on meshx_is_gen_cli_get_opcode).
 * @note Aligns with Bluetooth SIG Mesh Generic models (e.g., OnOff, Level, etc.).
 */
static meshx_err_t meshx_gen_client_txcm_fn_model_send(meshx_ptr_t msg_param, size_t msg_param_len)
{
    if(msg_param == NULL || msg_param_len != sizeof(meshx_gen_client_msg_ctx_t))
    {
        return MESHX_INVALID_ARG;
    }
    meshx_gen_client_msg_ctx_t * p_msg = (meshx_gen_client_msg_ctx_t*) msg_param;

    return meshx_plat_gen_cli_send_msg(
        p_msg->model,
        &p_msg->state,
        p_msg->opcode,
        p_msg->addr,
        p_msg->net_idx,
        p_msg->app_idx,
        (meshx_is_gen_cli_get_opcode(p_msg->opcode) == MESHX_SUCCESS)
    );
}

/**
 * @brief Handles a control task message for a generic client model.
 *
 * This function processes a control task message received from the control task module
 * for a generic client model. It is responsible for handling messages such as
 * Tx Control Module signals, and is called by the control task module when a control
 * task message is received.
 *
 * @param[in] pdev    Pointer to the device structure associated with the BLE Mesh node.
 * @param[in] model_id    Control task message event type, which represents the model ID of the generic
 *                     client model associated with the message.
 * @param[in] param    Pointer to a structure containing the control task message parameters. Must not be NULL.
 *
 * @return meshx_err_t  Error code from the control task message handling. Returns MESHX_SUCCESS on
 *                     successful handling.
 */
static meshx_err_t meshx_handle_txcm_msg(
    dev_struct_t *pdev,
    control_task_msg_evt_t model_id,
    meshx_gen_client_msg_ctx_t *param
)
{
    return MESHX_SUCCESS;
}

/**
 * @brief Handles ack request for generic client messages.
 *
 * This function requests the TXCM module to clear the last message by sending a ACK signal
 * with null parameters.
 *
 * @param[in] src_addr The Source address of the message.
 *
 * @return meshx_err_t Returns the error code from meshx_txcm_request_send().
 */
static meshx_err_t meshx_gen_cli_handle_ack(uint16_t src_addr)
{
    meshx_err_t err = MESHX_SUCCESS;
    if(g_meshx_client_control.gen_client_msg_waiting_for_ack > 0)
    {
        err = meshx_txcm_request_send(MESHX_TXCM_SIG_ACK, src_addr, NULL, 0, NULL);
        if(!err)
        {
            g_meshx_client_control.gen_client_msg_waiting_for_ack--;
        }
    }
    return err;
}

/**
 * @brief Handles resend request for generic client messages.
 *
 * This function requests the TXCM module to resend the last message by sending a RESEND signal
 * with null parameters.
 *
 * @return meshx_err_t Returns the error code from meshx_txcm_request_send().
 */
static meshx_err_t meshx_gen_cli_handle_resend(void)
{
    return meshx_txcm_request_send(MESHX_TXCM_SIG_RESEND, MESHX_ADDR_UNASSIGNED, NULL, 0, NULL);
}

/**
 * @brief Handle the Generic OnOff Client messages.
 *
 * This function processes the incoming messages for the Generic OnOff Client
 * and performs the necessary actions based on the message event and parameters.
 *
 * @param[in] pdev      Pointer to the device structure containing device-specific information.
 * @param[in] model_id  The model ID of the received message.
 * @param[in] param     Pointer to the Generic Client callback parameter structure.
 *
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
static meshx_err_t meshx_handle_gen_onoff_msg(
    dev_struct_t *pdev,
    control_task_msg_evt_t model_id,
    meshx_gen_cli_cb_param_t *param
)
{
    if(!param || !pdev)
    {
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Invalid parameters");
        return MESHX_INVALID_ARG;
    }
    meshx_gen_cli_cb_reg_t const * reg_cb = NULL;
    meshx_err_t err = MESHX_SUCCESS;

    meshx_gen_cli_cb_reg_node_t *node = g_meshx_gen_cli_cb_reg_head;

    while (node)
    {
        if (model_id == node->reg.model_id)
        {
            MESHX_LOGD(MODULE_ID_MODEL_CLIENT, "op|src|dst:%04" PRIx32 "|%04x|%04x",
                       param->ctx.opcode, param->ctx.src_addr, param->ctx.dst_addr);
            reg_cb = &node->reg;
            if (param->evt == MESHX_GEN_CLI_TIMEOUT || param->err_code != MESHX_SUCCESS)
            {
                MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Retrying to send the message");
                err = meshx_gen_cli_handle_resend();
                if(err != MESHX_SUCCESS)
                {
                    MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Resend failed: %d", err);
                }
                else
                {
                    /* As the retry would callback shall be triggered by the TXCM */
                    MESHX_DO_NOTHING;
                }
            }
            else
            {
                /* We need to notify if the ack is from the same source as that of dest */
                err = meshx_gen_cli_handle_ack(param->ctx.src_addr);
                if(err != MESHX_SUCCESS)
                {
                    MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Ack failed: %d", err);
                }
                err = reg_cb->cb(pdev, model_id, param);
            }
        }
        node = node->next;
    }

    if(reg_cb == NULL)
    {
        return MESHX_SUCCESS;
    }

    return err;
}

/**
 * @brief Initialize the mesh fuction generic client.
 *
 * This function sets up the necessary configurations and initializes the
 * meshxuction generic client for the BLE mesh node.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_FAIL: Failed to initialize the client
 */
meshx_err_t meshx_gen_client_init(void)
{
    if (g_meshx_client_control.meshx_client_init == MESHX_CLIENT_INIT_MAGIC_NO)
        return MESHX_SUCCESS;
    g_meshx_client_control.meshx_client_init = MESHX_CLIENT_INIT_MAGIC_NO;

    meshx_err_t err = meshx_txcm_event_cb_reg(&meshx_handle_txcm_msg);
    if(err != MESHX_SUCCESS)
        return err;
    return meshx_plat_gen_cli_init();
}

/**
 * @brief Sends a Generic Client message over BLE Mesh.
 *
 * This function sends a message from a Generic Client model to a specified address
 * within the BLE Mesh network, using the provided opcode and parameters.
 *
 * @param[in] params   Pointer to the structure containing the message parameters to set.
 *
 * @return meshx_err_t  Result of the operation. Returns MESHX_OK on success or an error code on failure.
 */
meshx_err_t meshx_gen_cli_send_msg(meshx_gen_client_send_params_t *params)
{
    if (!params || !params->model || !params->state)
    {
        return MESHX_INVALID_ARG;
    }

    meshx_err_t err = MESHX_SUCCESS;
    bool is_unack = meshx_is_unack_opcode(params->opcode) == MESHX_SUCCESS;
    /* Broadcast / Multicast will not be sending an ACK. Hence, it is not required to queue */
    meshx_txcm_sig_t req_type = (is_unack || (MESHX_ADDR_IS_UNICAST(params->addr) == false)) ? MESHX_TXCM_SIG_DIRECT_SEND : MESHX_TXCM_SIG_ENQ_SEND;

    meshx_gen_client_msg_ctx_t send_msg =
    {
        .model = params->model,
        .addr  = params->addr,
        .app_idx = params->app_idx,
        .net_idx = params->net_idx,
        .opcode  = params->opcode,
    };
    memcpy(&send_msg.state, params->state, sizeof(send_msg.state));

    g_meshx_client_control.gen_client_msg_waiting_for_ack++;
    err = meshx_txcm_request_send(
        req_type,
        send_msg.addr,
        &send_msg,
        sizeof(send_msg),
        (meshx_txcm_fn_model_send_t) &meshx_gen_client_txcm_fn_model_send
    );
    if(err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_CLIENT, "Failed to send message: %p", (meshx_ptr_t) err);
        g_meshx_client_control.gen_client_msg_waiting_for_ack--;
    }
    return err;
}

/**
 * @brief Registers a callback function for a specific generic server model.
 *
 * This function associates a callback with the given model ID, allowing the server
 * to handle events or messages related to that model.
 *
 * @param[in] model_id The unique identifier of voidthe generic server model.
 * @param[in] cb       The callback function to be registered for the model.
 *
 * @return meshx_err_t Returns an error code indicating the result of the registration.
 *                     Possible values include success or specific error codes.
 */
meshx_err_t meshx_gen_client_from_ble_reg_cb(uint32_t model_id, meshx_gen_client_cb_t cb)
{
    if (!cb || meshx_is_gen_cli_model(model_id) != MESHX_SUCCESS)
    {
        return MESHX_INVALID_ARG;
    }

    meshx_err_t err = MESHX_SUCCESS;
    meshx_gen_cli_cb_reg_t reg = { .model_id = (uint16_t)model_id, .cb = cb };

    err = meshx_gen_cli_cb_reg_add(reg);
    if (err != MESHX_SUCCESS)
    {
        return err;
    }

    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_FRM_BLE,
        model_id,
        (control_task_msg_handle_t) &meshx_handle_gen_onoff_msg
    );
}

#endif /* CONFIG_ENABLE_GEN_CLIENT */
