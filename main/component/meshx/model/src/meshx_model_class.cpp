/**
 * @file meshx_model_class.cpp
 * @brief Implementation of MeshX model wrapper classes
 *
 * This file contains the implementation of the constructor functions for
 * the MeshX model wrapper classes, providing the core initialization logic
 * for both client and server model wrappers.
 *
 * Key Features:
 * - Base model initialization
 * - Template-based constructor implementations
 * - Consistent initialization pattern for all model types
 * - Error handling and status reporting
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

/**
 * @defgroup meshx_model MeshX Model
 * @{
 */

#include <meshx_model_class.hpp>

/*****************************************************************************************************
 * meshXModel
 ******************************************************************************************************/
/**
 * @brief Constructs a new meshXModel instance.
 *
 * This constructor initializes a meshXModel object with the given platform model,
 * model ID, and optional parent element. It sets up the base model and model interface
 * for BLE mesh communication.
 *
 * @param[in] p_plat_model  Pointer to the platform model instance
 * @param[in] model_id      Unique identifier for this model
 * @param[in] parent_element Optional pointer to the parent element
 *
 * @note The constructor allocates memory for the base model and model interface.
 *       If memory allocation fails, the status will be set to MESHX_NO_MEM.
 */
MESHX_MODEL_TEMPLATE_PROTO
meshXModel MESHX_MODEL_TEMPLATE_PARAMS
    ::meshXModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element)
    : meshXModelIF(p_plat_model)
{
    this->set_parent_element(parent_element);
    this->p_plat_model = p_plat_model;
    /* base_model needs to be used logically by the element composition */
    base_model = std::make_unique<meshxBaseModel_t>(model_id, model_handle_from_ble_cb);

    /* Create logical model instance */
    this->plat_model_create();

    status = MESHX_SUCCESS;
}

MESHX_MODEL_TEMPLATE_PROTO
meshx_err_t meshXModel MESHX_MODEL_TEMPLATE_PARAMS
    ::send_to_parent_element(meshx_ptr_t msg_ptr, size_t msg_size)
{
    if (!msg_ptr || msg_size == 0)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Invalid message parameters");
        return MESHX_INVALID_ARG;
    }

    /* Send the state change event to the respective Element */
    if (this->get_parent_element())
    {
        meshx_err_t state_change_result = MESHX_SUCCESS;

        if(this->get_parent_element_state())
        {
            state_change_result = this->element_state_change_handle();
        }
        else
        {
            MESHX_LOGE(MODULE_ID_COMMON, "Parent element state is null");
            state_change_result = MESHX_NOT_FOUND;
        }

        // Delegate type-specific header casting to derived class
        this->update_element_state_change_header(state_change_result, msg_ptr);

        return this->get_parent_element()->on_model_cb(msg_ptr, msg_size);
    }
    else
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Parent element is null");
    }

    return MESHX_INVALID_STATE;
}

MESHX_MODEL_TEMPLATE_PROTO
meshx_err_t meshXModel MESHX_MODEL_TEMPLATE_PARAMS
    ::model_handle_from_ble_cb(
        dev_struct_t *p_dev,
        control_task_msg_evt_t evt,
        meshx_ptr_t params)
{
    meshx_err_t err = MESHX_SUCCESS;

    // Call derived class implementation
    err = this->model_from_ble_cb(p_dev, evt, params);
    if(err)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Error in model_from_ble_cb: %d", err);
        return err;
    }

    // Get message from derived class
    meshx_ptr_t msg_ptr = nullptr;
    size_t msg_size = 0;
    err = this->prepare_element_msg(&msg_ptr, &msg_size);
    if(err)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Error in prepare_element_msg: %d", err);
        return err;
    }

    // Send to parent element using common implementation
    return this->send_to_parent_element(msg_ptr, msg_size);
}

/**
 * @brief Destructor for meshXModel
 * @details Releases any allocated resources (base model and model interface) and
 *          calls the platform-specific model deletion function.
 */
MESHX_MODEL_TEMPLATE_PROTO
meshXModel MESHX_MODEL_TEMPLATE_PARAMS
    :: ~meshXModel()
{
    this->plat_model_delete();
}

/****************************************************************************************************
 * meshXServerModel
 ****************************************************************************************************/
/**
 * @brief Constructs a new meshXServerModel instance.
 *
 * This constructor initializes a server model with the given platform model,
 * model ID, and optional parent element. It sets up the base server model
 * functionality.
 *
 * @tparam MESHX_MODEL Platform-specific model type
 * @tparam meshxBaseModel_t Base model implementation type
 * @tparam meshx_send_packet_params_t Type for send packet parameters
 *
 * @note This constructor delegates to the base meshXModel constructor.
 */
MESHX_SERVER_MODEL_TEMPLATE_PROTO
meshXServerModel MESHX_SERVER_MODEL_TEMPLATE_PARAMS
    ::meshXServerModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element, meshx_ptr_t parent_element_state)
    : meshXModel MESHX_SERVER_MODEL_TEMPLATE_PARAMS (p_plat_model, model_id, parent_element)
{
    this->set_parent_element_state(parent_element_state);
}

/**************************************************************************************************
 * meshXClientModel
 **************************************************************************************************/
/**
 * @brief Constructs a new meshXClientModel instance.
 *
 * This constructor initializes a client model with the given platform model,
 * model ID, and optional parent element. It sets up the base client model
 * functionality and creates the logical model for the client model and its derivatives.
 *
 * @tparam MESHX_MODEL Platform-specific model type
 * @tparam meshxBaseModel_t Base model implementation type
 * @tparam meshx_send_packet_params_t Type for send packet parameters
 *
 * @note This constructor delegates to the base meshXModel constructor.
 *       Derived client models should use this constructor to ensure proper initialization.
 */
MESHX_CLIENT_MODEL_TEMPLATE_PROTO
meshXClientModel MESHX_CLIENT_MODEL_TEMPLATE_PARAMS
    ::meshXClientModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element, meshx_ptr_t parent_element_state)
    : meshXModel MESHX_CLIENT_MODEL_TEMPLATE_PARAMS (p_plat_model, model_id, parent_element)
{
    this->set_parent_element_state(parent_element_state);
}

/**
 * @brief Creates and initializes a client model instance.
 *
 * This function handles the platform-specific model creation process for client models.
 * It initializes client-specific features and cannot be overridden by derived classes.
 *
 * @tparam MESHX_MODEL Platform-specific model type
 * @tparam meshxBaseModel_t Base model implementation type
 * @tparam meshx_send_packet_params_t Type for send packet parameters
 *
 * @return meshx_err_t Returns an error code indicating the result of the operation.
 *         - MESHX_SUCCESS on successful model creation and initialization
 *         - MESHX_ERR_NO_MEM if memory allocation fails
 *         - Other error codes for platform-specific failures
 *
 * @note This is a final function and cannot be overridden by derived classes.
 */
MESHX_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXClientModel MESHX_CLIENT_MODEL_TEMPLATE_PARAMS
    ::plat_model_create()
{
    meshx_err_t err = MESHX_SUCCESS;

    meshx_ptr_t p_pub = this->get_pub_struct();
    meshx_ptr_t p_gen = this->get_gen_struct();

    err = meshx_plat_client_create(this->get_plat_model(), &p_pub, &p_gen, this->get_model_id());
    if (err)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to create client model");
        err = plat_model_delete();
        if (err)
        {
            MESHX_LOGE(MODULE_ID_COMMON, "Failed to delete client model");
        }
    }
    else
    {
        /* Set the publication and generic structures */
        this->set_pub_struct(p_pub);
        this->set_gen_struct(p_gen);
    }
    return err;
}

/**
 * @brief Deletes the client model instance.
 *
 * This function is responsible for deleting the client model instance
 * and releasing any associated resources.
 *
 * @return meshx_err_t Returns an error code indicating the result of the operation.
 *         - MESHX_SUCCESS on successful deletion
 *         - Other error codes for platform-specific failures
 *
 * @note This is a final function and cannot be overridden by derived classes.
 */
MESHX_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXClientModel MESHX_CLIENT_MODEL_TEMPLATE_PARAMS
    ::plat_model_delete()
{
    return meshx_plat_client_delete(
        this->get_plat_model(),
        &this->get_pub_struct(),
        &this->get_gen_struct());
}

MESHX_CLIENT_MODEL_TEMPLATE_PROTO
void meshXClientModel MESHX_CLIENT_MODEL_TEMPLATE_PARAMS
    ::update_element_state_change_header(meshx_err_t element_state_change, meshx_ptr_t msg_ptr)
{
    // Cast to client header - safe because this is a client model
    auto *cli_header = static_cast<meshx_cli_model_send_param_header_t*>(msg_ptr);
    cli_header->element_state_change = element_state_change;
}

/**************************************************************************************************
 * meshXServerModel
 **************************************************************************************************/

MESHX_SERVER_MODEL_TEMPLATE_PROTO
void meshXServerModel MESHX_SERVER_MODEL_TEMPLATE_PARAMS
    ::update_element_state_change_header(meshx_err_t element_state_change, meshx_ptr_t msg_ptr)
{
    // Cast to server header - safe because this is a server model
    auto *srv_header = static_cast<meshx_srv_model_send_param_header_t*>(msg_ptr);
    srv_header->element_state_change = element_state_change;
}

/**
 * @}
 */
