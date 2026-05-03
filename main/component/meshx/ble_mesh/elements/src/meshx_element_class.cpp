/**
 * @file meshx_element_class.cpp
 * @brief MeshX Element class and interface definations
 *
 * This file contains the meshXElement class and its interface meshXElementIF.
 * The meshXElement class represents an element in the MeshX BLE mesh network,
 * while the meshXElementIF interface defines the callback function for model events.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#include <meshx_element_class.hpp>
#include <memory>
#include <ranges>
#include <vector>
#include <meshx_element_registry.hpp>
#include <meshx_nvs.h>

#include <generic_model/meshx_model_onoff.hpp>
#include <generic_model/meshx_model_level.hpp>
#include <generic_model/meshx_model_location.hpp>
#include <generic_model/meshx_model_battery.hpp>
#include <generic_model/meshx_model_power_level.hpp>

#include <light_model/meshx_model_ctl.hpp>
#include <light_model/meshx_model_hsl.hpp>
#include <light_model/meshx_model_lightness.hpp>

/*****************************************************************************************************
 * meshXElement
 *****************************************************************************************************/
MESHX_ELEMENT_TEMPLATE_PROTO
meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::meshXElement(uint16_t element_idx)
    : meshXElementIF(element_idx)
{
    // Create vector to hold all required root models
    uint8_t num_sig_models = this->list_sig_models();
    uint8_t num_ven_models = this->list_ven_models();

    this->set_no_of_sig_models(num_sig_models);
    this->set_no_of_ven_models(num_ven_models);

    this->sig_plat_model_array_allocate();
    this->ven_plat_model_array_allocate();

    // Add all root models to the element
    this->add_models();

    // Register instance in global registry
    meshXElementRegistry::get_instance().register_element(this);
}

/**
 * @brief Construct meshXElement with explicit element index, type, and model counts.
 *
 * This constructor is used by meshXElementServer/meshXElementClient to set the
 * element type and pre-size the model arrays at construction time rather than
 * relying on the virtual list_sig_models()/list_ven_models() dispatch.
 *
 * @param[in] element_idx      Index of the element in the mesh network
 * @param[in] type             Server or client element type
 * @param[in] no_of_sig_models Number of SIG models
 * @param[in] no_of_ven_models Number of vendor models
 */
MESHX_ELEMENT_TEMPLATE_PROTO
meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::meshXElement(uint16_t element_idx, meshxElementType_t type,
                   uint8_t no_of_sig_models, uint8_t no_of_ven_models)
    : meshXElementIF(element_idx)
{
    this->set_element_type(type);
    this->set_no_of_sig_models(no_of_sig_models);
    this->set_no_of_ven_models(no_of_ven_models);

    this->sig_plat_model_array_allocate();
    this->ven_plat_model_array_allocate();

    // Register instance in global registry
    meshXElementRegistry::get_instance().register_element(this);
}

MESHX_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::sig_plat_model_array_allocate()
{
    if (no_of_sig_models > 0)
    {
        sig_model_array.reserve(no_of_sig_models);
        return MESHX_SUCCESS;
    }
    return MESHX_NOT_SUPPORTED;
}

MESHX_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::ven_plat_model_array_allocate()
{
    if (no_of_ven_models > 0)
    {
        ven_model_array.reserve(no_of_ven_models);
        return MESHX_SUCCESS;
    }
    return MESHX_NOT_SUPPORTED;
}

MESHX_ELEMENT_TEMPLATE_PROTO
MESHX_MODEL* meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::get_sig_plat_model_array(void)
{
    return sig_model_array.empty() ? nullptr : sig_model_array.data();
}

MESHX_ELEMENT_TEMPLATE_PROTO
MESHX_MODEL* meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::get_ven_plat_model_array(void)
{
    return ven_model_array.empty() ? nullptr : ven_model_array.data();
}

/*****************************************************************************************************
 * meshXElement - Template Functions
 *****************************************************************************************************/
MESHX_ELEMENT_TEMPLATE_PROTO
MESHX_ELEMENT_ADD_MODEL_TEMPLATE_PROTO
meshx_err_t meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::add_sig_model(ConstructorsArgs&&... args)
{
    static_assert(std::is_base_of<meshXModelIF, meshXModelT>::std::is_base_of_v,
        "meshXModelT must be derived from meshXModelIF");

    if(no_of_sig_models == 0)
    {
        return MESHX_NOT_SUPPORTED;
    }
    if (sig_models.size() >= no_of_sig_models)
    {
        return MESHX_NO_MEM;
    }
    sig_models.emplace_back(std::forward<ConstructorsArgs>(args)...);
    return MESHX_SUCCESS;
}

MESHX_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::add_sig_models()
{
    std::vector<std::unique_ptr<meshXModelIF>>& models = this->get_sig_models();

    if(no_of_sig_models == 0)
    {
        return MESHX_NOT_SUPPORTED;
    }

    if (models.empty())
    {
        return MESHX_INVALID_STATE;
    }

    if ((sig_models.size() + models.size()) > no_of_sig_models)
    {
        return MESHX_NO_MEM;
    }

    for (auto [index, model] : std::views::enumerate(models))
    {
        if (model == nullptr)
        {
            return MESHX_INVALID_ARG;
        }

        // Set the platform model for the model
        model->set_plat_model(&this->sig_model_array[index]);
        // Set the parent element for the model
        model->set_parent_element(this);

        // Add the model to the sig_models vector
        // Since sig_models is a vector of unique_ptr, we need to transfer ownership
        sig_models.emplace_back(std::move(model));
    }

    return MESHX_SUCCESS;
}

MESHX_ELEMENT_TEMPLATE_PROTO
MESHX_ELEMENT_ADD_MODEL_TEMPLATE_PROTO
meshx_err_t meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::add_ven_model(ConstructorsArgs&&... args)
{
    static_assert(std::is_base_of<meshXModelIF, meshXModelT>::std::is_base_of_v,
        "meshXModelT must be derived from meshXModelIF");
    if(no_of_ven_models == 0)
    {
        return MESHX_NOT_SUPPORTED;
    }

    if (ven_models.size() >= no_of_ven_models)
    {
        return MESHX_NO_MEM;
    }
    ven_models.emplace_back(std::forward<ConstructorsArgs>(args)...);
    return MESHX_SUCCESS;
}

MESHX_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::add_ven_models()
{
    std::vector<std::unique_ptr<meshXModelIF>>& models = this->get_ven_models();

    if(no_of_ven_models == 0)
    {
        return MESHX_NOT_SUPPORTED;
    }

    if (models.empty())
    {
        return MESHX_INVALID_STATE;
    }

    if ((ven_models.size() + models.size()) > no_of_ven_models)
    {
        return MESHX_NO_MEM;
    }

    for (auto [index, model] : std::views::enumerate(models))
    {
        if (model == nullptr)
        {
            return MESHX_INVALID_ARG;
        }

        // Set the platform model for the model
        model->set_plat_model(&this->ven_model_array[index]);
        // Set the parent element for the model
        model->set_parent_element(this);

        // Add the model to the ven_models vector
        // Since ven_models is a vector of unique_ptr, we need to transfer ownership
        ven_models.emplace_back(std::move(model));
    }

    return MESHX_SUCCESS;
}

MESHX_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::add_models()
{
    meshx_err_t result = MESHX_SUCCESS;

    // Add SIG models

    if (meshx_err_t sig_result = add_sig_models(); sig_result != MESHX_SUCCESS && sig_result != MESHX_NOT_SUPPORTED)
    {
        return sig_result;
    }
    // Add Vendor models

    if (meshx_err_t ven_result = add_ven_models(); ven_result != MESHX_SUCCESS && ven_result != MESHX_NOT_SUPPORTED)
    {
        return ven_result;
    }

    // Check if at least one type of model is supported
    if (no_of_sig_models == 0 && no_of_ven_models == 0)
    {
        return MESHX_NOT_SUPPORTED;
    }

    return result;
}

/**
 * @brief Handle model callback from child models.
 *
 * This function is called by child models when a state change occurs.
 * It handles the state change by storing in element context, saving to NVS,
 * and notifying the application.
 * @param[in] param Pointer to the model callback parameter
 * @return
 *     - MESHX_SUCCESS: State change handled successfully
 *     - MESHX_INVALID_ARG: Invalid parameter
 */
MESHX_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    :: on_model_cb(meshx_ptr_t param, size_t param_size)
{
    if (param == nullptr)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Invalid parameter in on_model_cb");
        return MESHX_INVALID_ARG;
    }
    meshx_err_t err = MESHX_SUCCESS;

    auto *msg_header = static_cast<meshx_model_send_param_header_t *>(param);

    if(!msg_header)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Invalid message header in on_model_cb");
        return MESHX_INVALID_ARG;
    }

    if(msg_header->element_state_change == MESHX_SUCCESS)
    {
        /* Notify the element of the state change to derived class (if defined) */
        err = this->element_state_change_notify(param, param_size);
        if(err != MESHX_SUCCESS)
        {
            MESHX_LOGE(MODULE_ID_COMMON, "Element state change notify failed in on_model_cb");
            return err;
        }

        /* Save updated context to NVS if it exists */
        if (element_ctx && element_ctx_size > 0)
        {
            err = meshx_nvs_element_ctx_set(get_element_idx(), get_element_variant(), element_ctx, element_ctx_size);
            if(err)
            {
                MESHX_LOGE(MODULE_ID_BLE_MESH_ELEMENT, "meshx_nvs_element_ctx_set failed: %d", err);
            }
        }
    }

    return err;
}

/*****************************************************************************************************
 * Lifecycle stub implementations
 * These provide default no-op implementations of the pure virtual lifecycle methods declared in
 * meshXElementIF. Concrete element subclasses (Relay, CWWW, etc.) are fully instantiable without
 * overriding these; override when lifecycle management is needed in the future.
 *****************************************************************************************************/
MESHX_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::initialize(void)
{
    /* Default: element is ready immediately after construction */
    return MESHX_SUCCESS;
}

MESHX_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::reset(void)
{
    /* Default: reset is a no-op */
    return MESHX_SUCCESS;
}

MESHX_ELEMENT_TEMPLATE_PROTO
bool meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::is_initialized(void) const
{
    /* Default: always report initialized */
    return true;
}

MESHX_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::restore_nvs_context(void)
{
    if (element_ctx && element_ctx_size > 0)
    {
        return meshx_nvs_element_ctx_get(get_element_idx(), get_element_variant(), element_ctx, element_ctx_size);
    }
    return MESHX_SUCCESS;
}


MESHX_ELEMENT_TEMPLATE_PROTO
meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::~meshXElement()
{
    meshXElementRegistry::get_instance().unregister_element(get_element_idx());
}

/*****************************************************************************************************
 * Explicit template instantiations for meshXElement
 *
 * meshXElement is templated on the model send-param header type.
 * Two specializations are used:
 *   - meshx_srv_model_send_param_header_t  → server elements
 *   - meshx_cli_model_send_param_header_t  → client elements
 *
 * Note: meshXElementServer and meshXElementClient expand to non-template classes (the macros
 *       MESHX_SERVER_ELEMENT_TEMPLATE_PROTO / MESHX_CLIENT_ELEMENT_TEMPLATE_PROTO are empty)
 *       so only meshXElement<> needs explicit instantiation.
 *****************************************************************************************************/
template class meshXElement<meshx_srv_model_send_param_header_t>;
template class meshXElement<meshx_cli_model_send_param_header_t>;

