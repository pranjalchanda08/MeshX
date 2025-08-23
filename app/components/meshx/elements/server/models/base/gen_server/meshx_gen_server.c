/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file meshx_gen_server.c
 * @brief Implementation of the MeshX generic server model for BLE mesh nodes.
 *        This file contains functions for registering, deregistering, and
 *        initializing the generic server model.
 *
 * The MeshX generic server model provides an interface for handling BLE mesh
 * server operations, including callback registration and initialization.
 *
 * @author Pranjal Chanda
 *
 */
#include "stdlib.h"
#include "meshx_gen_server.h"

#define MESHX_SERVER_INIT_MAGIC_NO 0x1121
static uint16_t meshx_server_init = 0;

/**
 * @brief Checks if the given opcode corresponds to a status message in the Generic Server group.
 *
 * This function determines whether the provided opcode is part of the set of status messages
 * defined for the Generic Server group in the MeshX protocol.
 *
 * @param[in] opcode The opcode to check.
 * @return meshx_err_t Returns an error code indicating whether the opcode is a status message in the Generic Server group.
 */
meshx_err_t meshx_is_status_in_gen_srv_grp(uint16_t opcode)
{
    switch(opcode)
    {
        case MESHX_MODEL_OP_GEN_ONOFF_STATUS:
        case MESHX_MODEL_OP_GEN_LEVEL_STATUS:
        case MESHX_MODEL_OP_GEN_DEF_TRANS_TIME_STATUS:
        case MESHX_MODEL_OP_GEN_ONPOWERUP_STATUS:
        case MESHX_MODEL_OP_GEN_POWER_LEVEL_STATUS:
        case MESHX_MODEL_OP_GEN_POWER_LAST_STATUS:
        case MESHX_MODEL_OP_GEN_POWER_DEFAULT_STATUS:
        case MESHX_MODEL_OP_GEN_POWER_RANGE_STATUS:
        case MESHX_MODEL_OP_GEN_BATTERY_STATUS:
        case MESHX_MODEL_OP_GEN_LOC_GLOBAL_STATUS:
        case MESHX_MODEL_OP_GEN_LOC_LOCAL_STATUS:
        case MESHX_MODEL_OP_GEN_MANUFACTURER_PROPERTIES_STATUS:
        case MESHX_MODEL_OP_GEN_MANUFACTURER_PROPERTY_STATUS:
            return MESHX_SUCCESS;
        default:
            return MESHX_FAIL;
    }
}

/**
 * @brief Sends a status message for the Generic Server model.
 *
 * This function sends a status message for the Generic Server model to the BLE Mesh network.
 * It checks if the provided model and context are valid, and if the opcode is within the
 * range of supported Generic Server opcodes.
 *
 * @param[in] p_model       Pointer to the Generic Server model structure.
 * @param[in] p_ctx         Pointer to the context containing message information.
 * @param[in] state_change  The state change data to be sent in the status message.
 * @param[in] data_len      The length of the data to be sent in the status message.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully sent the status message.
 *     - MESHX_INVALID_ARG: Invalid argument provided.
 *     - MESHX_ERR_PLAT: Platform-specific error occurred.
 */
meshx_err_t meshx_gen_srv_status_send(
    meshx_model_t *p_model,
    meshx_ctx_t *p_ctx,
    meshx_gen_srv_state_change_t state_change,
    size_t data_len)
{
    if (!p_model || !p_ctx || p_ctx->dst_addr == MESHX_ADDR_UNASSIGNED)
        return MESHX_INVALID_ARG;
    if (meshx_is_status_in_gen_srv_grp((uint16_t)p_ctx->opcode) != MESHX_SUCCESS)
        return MESHX_INVALID_ARG;

    return meshx_plat_gen_srv_send_status(
        p_model,
        p_ctx,
        &state_change,
        data_len
    );
}
/**
 * @brief Sends a message to the BLE subsystem via the control task.
 *
 * This function wraps the call to `control_task_msg_publish` with the appropriate message code
 * for BLE communication. It allows sending an event and associated parameters to the BLE handler.
 *
 * @param evt            The event type to send to BLE, of type control_task_msg_evt_to_ble_t.
 * @param params         Pointer to the parameters associated with the event.
 *
 * @return meshx_err_t   Returns the result of the message publish operation.
 */
meshx_err_t meshx_gen_srv_send_msg_to_ble(
    control_task_msg_evt_to_ble_t evt,
    const meshx_gen_srv_cb_param_t *params)
{
    if(MESHX_SUCCESS != meshx_is_status_in_gen_srv_grp(
        (uint16_t)(params->ctx.opcode)))
    {
        return MESHX_INVALID_ARG;
    }

    return control_task_msg_publish(CONTROL_TASK_MSG_CODE_TO_BLE,
                                    evt,
                                    params,
                                    sizeof(meshx_gen_srv_cb_param_t));
}
/**
 * @brief Registers a callback function for a specific generic server model.
 *
 * This function associates a callback with the given model ID, allowing the server
 * to handle events or messages related to that model.
 *
 * @param[in] model_id The unique identifier of the generic server model.
 * @param[in] cb       The callback function to be registered for the model.
 *
 * @return meshx_err_t Returns an error code indicating the result of the registration.
 *                     Possible values include success or specific error codes.
 */
meshx_err_t meshx_gen_srv_reg_cb(uint32_t model_id, meshx_server_cb cb)
{
    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_FRM_BLE,
        model_id,
        (control_task_msg_handle_t)cb);
}

/**
 * @brief Callback function to deregister a generic server model.
 *
 * This function is called to deregister a generic server model identified by the given model ID.
 *
 * @param[in] model_id  The ID of the model to be deregistered.
 * @param[in] cb        The callback function to be deregistered.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_INVALID_ARG: Invalid argument
 *     - MESHX_FAIL: Other failures
 */
meshx_err_t meshx_gen_srv_dereg_cb(uint32_t model_id, meshx_server_cb cb)
{
    return control_task_msg_unsubscribe(
        CONTROL_TASK_MSG_CODE_FRM_BLE,
        model_id,
        (control_task_msg_handle_t)cb);
}

/**
 * @brief Initialize the meshxuction generic server.
 *
 * This function sets up the necessary configurations and initializes the
 * meshxuction generic server for the BLE mesh node.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_FAIL: Failed to initialize the server
 */
meshx_err_t meshx_gen_srv_init(void)
{
    if (meshx_server_init == MESHX_SERVER_INIT_MAGIC_NO)
        return MESHX_SUCCESS;
    meshx_server_init = MESHX_SERVER_INIT_MAGIC_NO;

    return meshx_plat_gen_srv_init();
}
