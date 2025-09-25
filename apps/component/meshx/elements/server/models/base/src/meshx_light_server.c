/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_light_server.c
 * @brief Implementation of the BLE Mesh Lighting Server for the ESP32.
 *
 * This file contains the implementation of the BLE Mesh Lighting Server,
 * including initialization, event handling, and callback registration.
 *
 * @author Pranjal Chanda
 */

#include "meshx_platform_ble_mesh.h"
#include "meshx_light_server.h"
#include "meshx_gen_server.h"

#if CONFIG_ENABLE_LIGHT_SERVER
#define MESHX_SERVER_INIT_MAGIC_NO 0x2483

static uint16_t meshx_lighting_server_init = 0;

/**
 * @brief Checks if the given opcode belongs to the Generic Light group.
 *
 * This function determines whether the specified opcode is part of the
 * Generic Light group opcodes as defined in the MeshX Light Server model.
 *
 * @param opcode The opcode to check.
 * @return meshx_err_t Returns an error code indicating whether the opcode is in the Generic Light group.
 *         - MESHX_OK if the opcode is in the group.
 *         - MESHX_ERR_NOT_FOUND if the opcode is not in the group.
 */

meshx_err_t meshx_is_status_in_gen_light_grp(uint16_t opcode)
{
    switch (opcode)
    {
        case MESHX_MODEL_OP_LIGHT_LIGHTNESS_STATUS:
        case MESHX_MODEL_OP_LIGHT_LIGHTNESS_LINEAR_STATUS:
        case MESHX_MODEL_OP_LIGHT_CTL_STATUS:
        case MESHX_MODEL_OP_LIGHT_CTL_TEMPERATURE_STATUS:
        case MESHX_MODEL_OP_LIGHT_HSL_STATUS:
        case MESHX_MODEL_OP_LIGHT_HSL_HUE_STATUS:
        case MESHX_MODEL_OP_LIGHT_HSL_SATURATION_STATUS:
            return MESHX_SUCCESS;
        default:
            return MESHX_FAIL;
    }
}

/**
 * @brief Sends a message to the BLE subsystem via the control task.
 *
 * This function publishes a message to the BLE layer with the specified event and parameters.
 *
 * @param[in] evt          The event to be sent to the BLE layer.
 * @param[in] params       Pointer to the parameters associated with the event.
 *
 * @return
 *     - MESHX_SUCCESS: Message sent successfully.
 *     - MESHX_FAIL: Failed to send the message.
 */
meshx_err_t meshx_gen_light_srv_send_msg_to_ble(
    control_task_msg_evt_to_ble_t evt,
    const meshx_lighting_server_cb_param_t *params)
{
    if (MESHX_SUCCESS != meshx_is_status_in_gen_light_grp(
            (uint16_t)(params->ctx.opcode)))
    {
        return MESHX_SUCCESS; // No action needed for non-light group opcodes
    }
    return control_task_msg_publish(CONTROL_TASK_MSG_CODE_TO_BLE,
                                    evt,
                                    params,
                                    sizeof(meshx_lighting_server_cb_param_t));
}

/**
 * @brief Callback function to deregister a lighting server model.
 *
 * This function is called to deregister a lighting server model identified by the given model ID.
 *
 * @param[in] model_id  The ID of the model to be deregistered.
 * @param[in] cb        The callback function to be deregistered.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_INVALID_ARG: Invalid argument
 *     - MESHX_FAIL: Other failures
 */
meshx_err_t meshx_lighting_srv_dereg_cb(uint32_t model_id, meshx_lighting_server_cb cb)
{
    return control_task_msg_unsubscribe(CONTROL_TASK_MSG_CODE_FRM_BLE, model_id, (control_task_msg_handle_t)cb);
}
/**
 * @brief Register a callback function for the lighting server model.
 *
 * This function registers a callback function that will be called when
 * certain events occur in the lighting server model.
 *
 * @param[in] model_id  The ID of the lighting server model.
 * @param[in] cb        The callback function to register.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_INVALID_ARG: Invalid argument
 *    - MESHX_FAIL: Other failures
 */
meshx_err_t meshx_lighting_reg_cb(uint32_t model_id, meshx_lighting_server_cb cb)
{
    return control_task_msg_subscribe(CONTROL_TASK_MSG_CODE_FRM_BLE, model_id, (control_task_msg_handle_t)cb);
}

/**
 * @brief Initialize the meshxuction lighting server.
 *
 * This function sets up the necessary configurations and initializes the
 * meshxuction lighting server for the BLE mesh node.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_FAIL: Failed to initialize the lighting server
 */
meshx_err_t meshx_lighting_srv_init(void)
{
    if (meshx_lighting_server_init == MESHX_SERVER_INIT_MAGIC_NO)
        return MESHX_SUCCESS;
    meshx_lighting_server_init = MESHX_SERVER_INIT_MAGIC_NO;

    return meshx_plat_light_srv_init();
}

/**
 * @brief Sends a status message for the Lighting Server model.
 *
 * This function sends a status message for the Lighting Server model with the specified parameters.
 *
 * @param[in] p_model       Pointer to the Lighting Server model.
 * @param[in] ctx           Pointer to the context of the received messages.
 * @param[in] state_change  Pointer to the state change data for the Lighting Server.
 *
 * @return MESHX_SUCCESS on success, or an appropriate error code on failure.
 */
meshx_err_t meshx_gen_light_srv_status_send(
    meshx_model_t *p_model,
    meshx_ctx_t *ctx,
    meshx_lighting_server_state_change_t *state_change
)
{
    if (!p_model || !ctx || !state_change)
        return MESHX_INVALID_ARG;

    if (meshx_is_status_in_gen_light_grp((uint16_t)ctx->opcode) != MESHX_SUCCESS)
    {
        return MESHX_NOT_SUPPORTED;
    }

    return meshx_plat_gen_light_srv_send_status(
        p_model,
        ctx,
        state_change
    );
}

#endif /* CONFIG_ENABLE_LIGHT_SERVER */
