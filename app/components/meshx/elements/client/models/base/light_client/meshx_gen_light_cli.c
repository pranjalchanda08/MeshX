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
#include "meshx_gen_light_cli.h"

#define MESHX_CLIENT_INIT_MAGIC_NO 0x4309

#if CONFIG_ENABLE_LIGHT_CLIENT
static uint16_t meshx_client_init = 0;

/**
 * @brief Checks if the given model ID corresponds to a Generic Light Client GET opcode.
 *
 * This function determines whether the specified opcode is part of the set of GET
 *
 * @param opcode The opcode to check.
 * @return meshx_err_t Returns an error code indicating whether the opcode is a GET opcode
 *                     for the Generic Light Client model.
 */
static meshx_err_t meshx_is_gen_light_cli_get_opcode(uint32_t opcode)
{
    switch (opcode)
    {
        case MESHX_MODEL_OP_LIGHT_LIGHTNESS_GET:
        case MESHX_MODEL_OP_LIGHT_CTL_GET:
        case MESHX_MODEL_OP_LIGHT_HSL_GET:
        case MESHX_MODEL_OP_LIGHT_XYL_GET:
        case MESHX_MODEL_OP_LIGHT_LC_MODE_GET:
        case MESHX_MODEL_OP_LIGHT_LC_OM_GET:
        case MESHX_MODEL_OP_LIGHT_LC_LIGHT_ONOFF_GET:
        case MESHX_MODEL_OP_LIGHT_LC_PROPERTY_GET:
            return MESHX_SUCCESS;
        default:
            return MESHX_FAIL;
    }
}

/**
 * @brief Checks if the given model ID corresponds to a Generic Light Client model.
 *
 * This function determines whether the specified model ID matches the
 * Generic Light Client model supported by MeshX.
 *
 * @param model_id The model ID to check.
 * @return meshx_err_t Returns an error code indicating whether the model ID is a Generic Light Client model.
 */
static meshx_err_t meshx_is_gen_light_cli_model(uint32_t model_id)
{
    switch (model_id)
    {
        case MESHX_MODEL_ID_LIGHT_LIGHTNESS_CLI:
        case MESHX_MODEL_ID_LIGHT_CTL_CLI:
        case MESHX_MODEL_ID_LIGHT_HSL_CLI:
        case MESHX_MODEL_ID_LIGHT_XYL_CLI:
        case MESHX_MODEL_ID_LIGHT_LC_CLI:
            return MESHX_SUCCESS;
        default:
            return MESHX_FAIL;
    }
}
/**
 * @brief Initialize the meshxuction generic client.
 *
 * This function sets up the necessary configurations and initializes the
 * meshxuction generic client for the BLE mesh node.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_FAIL: Failed to initialize the client
 */
meshx_err_t meshx_gen_light_cli_init(void)
{
    if (meshx_client_init == MESHX_CLIENT_INIT_MAGIC_NO)
        return MESHX_SUCCESS;
    meshx_client_init = MESHX_CLIENT_INIT_MAGIC_NO;

    return meshx_plat_gen_light_client_init();
}

/**
 * @brief Send a message using the generic client model.
 *
 * This function sends a message using the generic client model, allowing
 * interaction with the BLE mesh network.
 *
 * @param[in] model     Pointer to the model instance.
 * @param[in] state     Pointer to the state to be set.
 * @param[in] opcode    The operation code for the message.
 * @param[in] addr      The address to which the message is sent.
 * @param[in] net_idx   The network index for routing the message.
 * @param[in] app_idx   The application index for the message.
 *
 * @return
 *     - MESHX_SUCCESS: Message sent successfully.
 *     - Appropriate error code on failure.
 */
meshx_err_t meshx_gen_light_send_msg(
    meshx_ptr_t model,
    meshx_light_client_set_state_t *state,
    uint16_t opcode,
    uint16_t addr,
    uint16_t net_idx,
    uint16_t app_idx
)
{
    if (!model || !state)
    {
        return MESHX_INVALID_ARG;
    }

    bool is_get_opcode = (meshx_is_gen_light_cli_get_opcode(opcode) == MESHX_SUCCESS);

    return meshx_plat_light_client_send_msg(
        model, state, opcode, addr, net_idx, app_idx, is_get_opcode
    );
}

/**
 * @brief Registers a callback function for getting Generic Light Client messages from BLE.
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
meshx_err_t meshx_gen_light_client_from_ble_reg_cb(uint32_t model_id, meshx_gen_light_client_cb_t cb)
{
    if (!cb || meshx_is_gen_light_cli_model(model_id) != MESHX_SUCCESS)
    {
        return MESHX_INVALID_ARG;
    }
    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_FRM_BLE,
        model_id,
        cb);
}

#endif /* CONFIG_ENABLE_LIGHT_CLIENT */
