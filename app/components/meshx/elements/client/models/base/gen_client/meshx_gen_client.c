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
#include "meshx_gen_client.h"

#if CONFIG_ENABLE_GEN_CLIENT
#define MESHX_CLIENT_INIT_MAGIC_NO 0x1121
static uint16_t meshx_client_init = 0;

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
 * @brief Initialize the meshxuction generic client.
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
    if (meshx_client_init == MESHX_CLIENT_INIT_MAGIC_NO)
        return MESHX_SUCCESS;
    meshx_client_init = MESHX_CLIENT_INIT_MAGIC_NO;

    return meshx_plat_gen_cli_init();
}

/**
 * @brief Sends a generic client message in the MeshX framework.
 *
 * This function constructs and sends a message from a generic client model to a specified address
 * within the mesh network. It uses the provided model context, state parameters, opcode, and addressing
 * information to format the message appropriately.
 *
 * @param[in] model     The model context or memory handle associated with the client.
 * @param[in] state     Pointer to the structure containing the state to be set or queried.
 * @param[in] opcode    The opcode representing the type of generic client message to send.
 * @param[in] addr      The destination address within the mesh network.
 * @param[in] net_idx   The network index identifying the subnet to use for sending the message.
 * @param[in] app_idx   The application key index used for encrypting the message.
 *
 * @return meshx_err_t Returns an error code indicating the result of the operation.
 */
meshx_err_t meshx_gen_cli_send_msg(
    meshx_ptr_t model,
    meshx_gen_cli_set_t *state,
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

    return meshx_plat_gen_cli_send_msg(
        model, state, opcode, addr, net_idx, app_idx, (meshx_is_gen_cli_get_opcode(opcode) == MESHX_SUCCESS)
    );
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
meshx_err_t meshx_gen_client_from_ble_reg_cb(uint32_t model_id, meshx_gen_client_cb_t cb)
{
    if (!cb || meshx_is_gen_cli_model(model_id) != MESHX_SUCCESS)
    {
        return MESHX_INVALID_ARG;
    }
    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_FRM_BLE,
        model_id,
        cb);
}

#endif /* CONFIG_ENABLE_GEN_CLIENT */
