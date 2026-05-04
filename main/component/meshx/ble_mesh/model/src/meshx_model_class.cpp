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
#include <meshx_base_model_generic.hpp>
#include <meshx_base_model_light.hpp>
#include <meshx_base_model_common.hpp>

#include <generic_model/meshx_model_onoff.hpp>
#include <generic_model/meshx_model_level.hpp>
#include <generic_model/meshx_model_battery.hpp>
#include <generic_model/meshx_model_power_onoff.hpp>
#include <generic_model/meshx_model_power_level.hpp>
#include <generic_model/meshx_model_property.hpp>
#include <generic_model/meshx_model_def_trans_time.hpp>
#include <generic_model/meshx_model_location.hpp>
#include <light_model/meshx_model_ctl.hpp>
#include <light_model/meshx_model_hsl.hpp>
#include <light_model/meshx_model_lightness.hpp>
#include <sensor_model/meshx_model_sensor.hpp>
#include <meshx_base_model_sensor.hpp>
#include <common_model/meshx_model_config.hpp>

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
 * @param[in] p_plat_model   Pointer to the platform model instance
 * @param[in] model_id       Unique identifier for this model
 * @param[in] parent_element Optional pointer to the parent element
 * @param[in] model_func_id  Optional model function ID within the element
 *
 * @note The constructor allocates memory for the base model and model interface.
 *       If memory allocation fails, the status will be set to MESHX_NO_MEM.
 */
MESHX_MODEL_TEMPLATE_PROTO
meshXModel MESHX_MODEL_TEMPLATE_PARAMS
    ::meshXModel(
        MESHX_MODEL     *p_plat_model,
        uint32_t         model_id,
        meshXElementIF  *parent_element,
        uint16_t         model_func_id
)
    : meshXModelIF(p_plat_model)
{
    this->set_plat_model(p_plat_model);
    this->set_model_func_id(model_func_id);
    this->set_parent_element(parent_element);
    /* base_model needs to be used logically by the element composition */
    base_model = new meshxBaseModel_t(model_id,
        [this](dev_struct_t *dev, control_task_msg_evt_t evt, meshx_ptr_t param) -> meshx_err_t {
            return this->model_handle_from_ble_cb(dev, evt, param);
        });

    /* Create logical model instance is now handled by derived classes or post-construction */
    status = MESHX_SUCCESS;
}

MESHX_MODEL_TEMPLATE_PROTO
meshXModel MESHX_MODEL_TEMPLATE_PARAMS
    ::~meshXModel()
{
    /* Delete logical model instance is now handled by derived classes */
    if (base_model)
    {
        delete base_model;
        base_model = nullptr;
    }
}

/**
 * @brief Send message to parent element
 * @details Common implementation to send a message to the parent element.
 *          This method checks for valid parameters, invokes the element's
 *          on_model_cb method, and handles state change updates.
 *
 * @param[in] msg_ptr  Pointer to the message structure
 * @param[in] msg_size Size of the message structure
 * @return MESHX_SUCCESS if message sent successfully, error code otherwise
 */
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

/**
 * @brief Callback function invoked when a BLE event is received for the model.
 *
 * This function is called when a BLE event is received for the model. It
 * delegates the processing of the event to the derived class implementation
 * and then prepares a message to send to the parent element.
 *
 * @param[in] p_dev         Pointer to the device structure
 * @param[in] evt_model_id  The BLE event model_id type
 * @param[in] params        Pointer to additional parameters for the event
 *
 * @return meshx_err_t Returns an error code indicating the result of the operation.
 *         - MESHX_SUCCESS on successful processing and message sending
 *         - Other error codes for failures in derived class processing or message preparation
 */
MESHX_MODEL_TEMPLATE_PROTO
meshx_err_t meshXModel MESHX_MODEL_TEMPLATE_PARAMS
    ::model_handle_from_ble_cb(
        dev_struct_t    *p_dev,
        evt_model_id_t   evt_model_id,
        meshx_ptr_t      params
)
{
    meshx_err_t err = MESHX_SUCCESS;

    // Call derived class implementation
    err = this->model_from_ble_cb(p_dev, evt_model_id, params);
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
    ::meshXServerModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element, meshx_ptr_t parent_element_state, uint16_t model_func_id)
    : meshXModel MESHX_SERVER_MODEL_TEMPLATE_PARAMS (p_plat_model, model_id, parent_element, model_func_id)
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
    ::meshXClientModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element, meshx_ptr_t parent_element_state, uint16_t model_func_id)
    : meshXModel MESHX_CLIENT_MODEL_TEMPLATE_PARAMS (p_plat_model, model_id, parent_element, model_func_id)
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
    ::plat_model_create(MESHX_MODEL* p_plat_model_ptr)
{
    meshx_err_t err = MESHX_SUCCESS;

    meshx_ptr_t p_pub = this->get_pub_struct();
    meshx_ptr_t p_gen = this->get_gen_struct();

    if (p_plat_model_ptr) {
        this->set_plat_model(p_plat_model_ptr);
    }

    MESHX_MODEL* p_use = (MESHX_MODEL*)this->get_plat_model();
    if (!p_use)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "No platform model pointer available");
        return MESHX_INVALID_STATE;
    }

    err = meshx_plat_client_create(p_use, &p_pub, &p_gen, this->get_model_id());
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
    meshx_ptr_t p_pub = this->get_pub_struct();
    meshx_ptr_t p_gen = this->get_gen_struct();

    meshx_err_t err = meshx_plat_client_delete(
        this->get_plat_model(),
        &p_pub,
        &p_gen);

    if (err == MESHX_SUCCESS)
    {
        this->set_pub_struct(p_pub);
        this->set_gen_struct(p_gen);
    }

    return err;
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

/**
 * @brief Update element_state_change field in server message header
 * @details Overrides base class implementation to handle server-specific header structure
 *          (meshx_srv_model_send_param_header_t) which doesn't include err_code and ctx.
 *
 * @param[in] element_state_change  Result from element_state_change_handle()
 * @param[in] msg_ptr               Pointer to the message structure
 */
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

/*****************************************************************************************************
 * Explicit template instantiations
 *
 * Because template method bodies live in this .cpp file (not in the header), the linker
 * cannot find them when other TUs use the templates.  Explicit instantiation directives
 * instruct the compiler to emit all member definitions for each listed specialization in
 * this translation unit, making them available to the linker.
 *
 * Add a new line here whenever a new <meshxBaseXxxModel, meshx_xxx_send_params_t> combination
 * is introduced in the codebase.
 *****************************************************************************************************/
#include <meshx_base_model_generic.hpp>
#include <meshx_base_model_light.hpp>
#include <generic_model/meshx_model_onoff.hpp>
#include <generic_model/meshx_model_level.hpp>
#include <generic_model/meshx_model_battery.hpp>
#include <generic_model/meshx_model_power_onoff.hpp>
#include <generic_model/meshx_model_power_level.hpp>
#include <generic_model/meshx_model_property.hpp>
#include <generic_model/meshx_model_def_trans_time.hpp>
#include <generic_model/meshx_model_location.hpp>
#include <light_model/meshx_model_ctl.hpp>
#include <light_model/meshx_model_hsl.hpp>
#include <light_model/meshx_model_lightness.hpp>

/* Explicit member-function instantiations
 *
 * We only instantiate the specific template methods that the linker cannot find,
 * rather than the full class. This avoids forcing buggy/unimplemented code paths
 * (e.g., plat_model_delete, get_pub_struct by value) to be instantiated.
 *
 * The missing symbols were:
 *   meshXServerModel<T,U>::update_element_state_change_header()
 *   meshXClientModel<T,U>::update_element_state_change_header()
 *   meshXServerModel<T,U>::meshXServerModel() [constructor]
 *****************************************************************************************************/
#if CONFIG_ENABLE_GEN_SERVER
template class meshXServerModel<meshXBaseGenericServerModel, meshx_gen_onoff_send_params_t>;
template class meshXServerModel<meshXBaseGenericServerModel, meshx_gen_level_send_params_t>;
template class meshXServerModel<meshXBaseGenericServerModel, meshx_gen_battery_send_params_t>;
template class meshXServerModel<meshXBaseGenericServerModel, meshx_gen_power_onoff_send_params_t>;
template class meshXServerModel<meshXBaseGenericServerModel, meshx_gen_power_level_send_params_t>;
template class meshXServerModel<meshXBaseGenericServerModel, meshx_gen_property_send_params_t>;
template class meshXServerModel<meshXBaseGenericServerModel, meshx_gen_def_trans_time_send_params_t>;
template class meshXServerModel<meshXBaseGenericServerModel, meshx_gen_location_send_params_t>;
#endif

#if CONFIG_ENABLE_GEN_CLIENT
template class meshXClientModel<meshXBaseGenericClientModel, meshx_gen_onoff_send_params_t>;
template class meshXClientModel<meshXBaseGenericClientModel, meshx_gen_level_send_params_t>;
template class meshXClientModel<meshXBaseGenericClientModel, meshx_gen_battery_send_params_t>;
template class meshXClientModel<meshXBaseGenericClientModel, meshx_gen_power_onoff_send_params_t>;
template class meshXClientModel<meshXBaseGenericClientModel, meshx_gen_power_level_send_params_t>;
template class meshXClientModel<meshXBaseGenericClientModel, meshx_gen_property_send_params_t>;
template class meshXClientModel<meshXBaseGenericClientModel, meshx_gen_def_trans_time_send_params_t>;
template class meshXClientModel<meshXBaseGenericClientModel, meshx_gen_location_send_params_t>;
#endif

#if CONFIG_ENABLE_LIGHT_SERVER
template class meshXServerModel<meshXBaseLightServerModel, meshx_light_ctl_send_params_t>;
template class meshXServerModel<meshXBaseLightServerModel, meshx_light_hsl_send_params_t>;
template class meshXServerModel<meshXBaseLightServerModel, meshx_light_lightness_send_params_t>;
#endif

#if CONFIG_ENABLE_LIGHT_CLIENT
template class meshXClientModel<meshXBaseLightClientModel, meshx_light_ctl_send_params_t>;
template class meshXClientModel<meshXBaseLightClientModel, meshx_light_hsl_send_params_t>;
template class meshXClientModel<meshXBaseLightClientModel, meshx_light_lightness_send_params_t>;
#endif

#if CONFIG_ENABLE_CONFIG_SERVER
template class meshXServerModel<meshXBaseConfigServerModel, meshx_config_send_params_t>;
#endif

#if CONFIG_ENABLE_SENSOR_SERVER
template class meshXServerModel<meshXBaseSensorServerModel, meshx_sensor_server_send_params_t>;
#endif

/* meshXModel base class instantiations */
#if CONFIG_ENABLE_GEN_SERVER
template class meshXModel<meshXBaseGenericServerModel, meshx_gen_onoff_send_params_t>;
template class meshXModel<meshXBaseGenericServerModel, meshx_gen_level_send_params_t>;
template class meshXModel<meshXBaseGenericServerModel, meshx_gen_battery_send_params_t>;
template class meshXModel<meshXBaseGenericServerModel, meshx_gen_power_onoff_send_params_t>;
template class meshXModel<meshXBaseGenericServerModel, meshx_gen_power_level_send_params_t>;
template class meshXModel<meshXBaseGenericServerModel, meshx_gen_property_send_params_t>;
template class meshXModel<meshXBaseGenericServerModel, meshx_gen_def_trans_time_send_params_t>;
template class meshXModel<meshXBaseGenericServerModel, meshx_gen_location_send_params_t>;
#endif

#if CONFIG_ENABLE_GEN_CLIENT
template class meshXModel<meshXBaseGenericClientModel, meshx_gen_onoff_send_params_t>;
template class meshXModel<meshXBaseGenericClientModel, meshx_gen_level_send_params_t>;
template class meshXModel<meshXBaseGenericClientModel, meshx_gen_battery_send_params_t>;
template class meshXModel<meshXBaseGenericClientModel, meshx_gen_power_onoff_send_params_t>;
template class meshXModel<meshXBaseGenericClientModel, meshx_gen_power_level_send_params_t>;
template class meshXModel<meshXBaseGenericClientModel, meshx_gen_property_send_params_t>;
template class meshXModel<meshXBaseGenericClientModel, meshx_gen_def_trans_time_send_params_t>;
template class meshXModel<meshXBaseGenericClientModel, meshx_gen_location_send_params_t>;
#endif

#if CONFIG_ENABLE_LIGHT_SERVER
template class meshXModel<meshXBaseLightServerModel, meshx_light_ctl_send_params_t>;
template class meshXModel<meshXBaseLightServerModel, meshx_light_hsl_send_params_t>;
template class meshXModel<meshXBaseLightServerModel, meshx_light_lightness_send_params_t>;
#endif

#if CONFIG_ENABLE_LIGHT_CLIENT
template class meshXModel<meshXBaseLightClientModel, meshx_light_ctl_send_params_t>;
template class meshXModel<meshXBaseLightClientModel, meshx_light_hsl_send_params_t>;
template class meshXModel<meshXBaseLightClientModel, meshx_light_lightness_send_params_t>;
#endif

#if CONFIG_ENABLE_CONFIG_SERVER
template class meshXModel<meshXBaseConfigServerModel, meshx_config_send_params_t>;
#endif

#if CONFIG_ENABLE_SENSOR_SERVER
template class meshXModel<meshXBaseSensorServerModel, meshx_sensor_server_send_params_t>;
#endif
