/**
 * @file meshx_model_config.cpp
 * @brief Implementation of Config Server Model.
 */

#include <common_model/meshx_model_config.hpp>

#if CONFIG_ENABLE_CONFIG_SERVER

/**
 * @brief Creates and initializes a config server model instance.
 *
 * This function handles the platform-specific model creation process for config server models.
 * It initializes config server-specific features and cannot be overridden by derived classes.
 *
 * @return meshx_err_t Returns an error code indicating the result of the operation.
 *         - MESHX_SUCCESS on successful model creation and initialization
 *         - MESHX_ERR_NO_MEM if memory allocation fails
 *         - Other error codes for platform-specific failures
 */
MESHX_CONFIG_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXConfigModel MESHX_CONFIG_SERVER_MODEL_TEMPLATE_PARAMS::plat_model_create(void)
{
    meshx_err_t err = MESHX_SUCCESS;

    // For config server, we use the platform's get_config_srv_model function
    // to get the model instance since there's no specific create function
    err = meshx_plat_get_config_srv_model(this->get_plat_model());
    if (err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to get Config Server Model");
        return err;
    }

    // Config server doesn't use pub/gen structures like other models
    // Set them to nullptr to indicate they're not used
    this->set_pub_struct(nullptr);
    this->set_gen_struct(nullptr);

    return MESHX_SUCCESS;
}

/**
 * @brief Deletes the Config Server model and its associated resources.
 *
 * This function frees the memory allocated for the Config Server
 * and sets the pointer to NULL. For config server, this is mainly
 * a cleanup operation since the model is managed by the platform.
 *
 * @return
 *     - MESHX_SUCCESS: Model deleted successfully.
 *     - MESHX_FAIL: Failed to delete the model.
 */
MESHX_CONFIG_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXConfigModel MESHX_CONFIG_SERVER_MODEL_TEMPLATE_PARAMS::plat_model_delete(void)
{
    // Config server is managed by the platform, so we don't need to
    // explicitly delete it. Just clean up our references.
    this->set_pub_struct(nullptr);
    this->set_gen_struct(nullptr);

    return MESHX_SUCCESS;
}

/**
 * @brief Send a packet to the MeshX stack based on the given parameters
 *
 * This function takes a pointer to a meshx_config_send_params_t structure as input
 * and sends the corresponding packet to the MeshX stack for processing.
 * For config server, this is typically used for sending status responses.
 *
 * @param[in] params Pointer to the meshx_config_send_params_t structure containing the parameters
 * @return MESHX_SUCCESS on success, or an error code on failure
 */
MESHX_CONFIG_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXConfigModel MESHX_CONFIG_SERVER_MODEL_TEMPLATE_PARAMS::model_send(meshx_config_send_params_t *params)
{
    if (!params || !params->model || !params->ctx)
    {
        return MESHX_INVALID_ARG;
    }

    // For config server, we use the base model's send functionality
    // The config server send params structure is minimal (just a stub)
    meshx_config_server_send_params_t send_params = {
        .p_model   = params->model,
        .p_ctx     = params->ctx,
        .p_data    = &params->stub,  // Use stub field as data
        .data_len  = sizeof(params->stub)
    };

    return this->get_base_model()->plat_send_msg(&send_params);
}

/**
 * @brief This function is the callback for the Config Server model.
 *
 * This function is triggered whenever a Config Server message is received from the MeshX stack.
 * It handles the Config Server state change notifications from the MeshX stack and publishes
 * the state change event to the element layer.
 *
 * @param[in] p_dev       A pointer to the device structure.
 * @param[in] model_id    The unique identifier of the BLE mesh model.
 * @param[in] params      A pointer to the callback parameter structure containing the details of the
 *                        received message.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_INVALID_ARG: Invalid argument
 *    - MESHX_FAIL: Other failures
 */
MESHX_CONFIG_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXConfigModel MESHX_CONFIG_SERVER_MODEL_TEMPLATE_PARAMS::model_from_ble_cb(
    dev_struct_t *p_dev,
    evt_model_id_t model_id,
    meshx_ptr_t params)
{
    if(!params || !p_dev)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameters");
        return MESHX_INVALID_ARG;
    }
    if(model_id != MESHX_MODEL_ID_CONFIG_SRV)
    {
        /* Callback triggered not for this model */
        return MESHX_SUCCESS;
    }

    auto const *param = static_cast<meshx_config_srv_cb_param_t *>(params);

    MESHX_LOGD(MODULE_ID_MODEL_SERVER, "op|src|dst:%04" PRIx32 "|%04x|%04x",
               param->ctx.opcode, param->ctx.src_addr, param->ctx.dst_addr);

    // Store the message in member variable for later use by prepare_element_msg
    element_msg = {
        .header = {
            .model                  = param->model,
            .element_state_change   = MESHX_SUCCESS,
        },
        .state = { .state_change = param->state_change }
    };

    return MESHX_SUCCESS;
}

/**
 * @brief Prepare message for element notification
 * @details Returns pointer to the stored element message
 *
 * @param[out] msg_ptr   Pointer to message structure (output parameter)
 * @param[out] msg_size  Size of the message structure (output parameter)
 * @return MESHX_SUCCESS if message prepared successfully, error code otherwise
 */
MESHX_CONFIG_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXConfigModel MESHX_CONFIG_SERVER_MODEL_TEMPLATE_PARAMS::prepare_element_msg(
    meshx_ptr_t *msg_ptr,
    size_t *msg_size)
{
    if (!msg_ptr || !msg_size)
    {
        return MESHX_INVALID_ARG;
    }

    *msg_ptr  = &element_msg;
    *msg_size = sizeof(element_msg);

    return MESHX_SUCCESS;
}

/**
 * @brief Constructor for meshXConfigModel class
 *
 * @param[in]   parent_element  Pointer to parent element
 */
meshXConfigModel::meshXConfigModel(
        meshXElementIF *parent_element,
        meshx_ptr_t     parent_element_state,
        uint16_t        model_func_id
    )
    : meshXServerModel(nullptr, MESHX_MODEL_ID_CONFIG_SRV, parent_element, parent_element_state, model_func_id)
{
    // Constructor body can be empty; initialization done in base class.
}

#endif /* CONFIG_ENABLE_CONFIG_SERVER */
