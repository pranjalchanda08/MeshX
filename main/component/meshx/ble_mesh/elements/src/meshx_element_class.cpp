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
#include <meshx_element_registry.hpp>


/*****************************************************************************************************
 * meshXElement
 *****************************************************************************************************/
MESHX_ELEMENT_TEMPLATE_PROTO
meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::meshXElement(uint16_t element_idx)
    : meshXElementIF(element_idx), no_of_sig_models(0), no_of_ven_models(0), element_type(meshxElementType_t::MESHX_ELEMENT_TYPE_SERVER),
      element_variant(MESHX_ELEMENT_TYPE_MAX), element_ctx(nullptr), element_ctx_size(0)
{
    // Register instance in global registry
    meshXElementRegistry::get_instance().register_element(this);
    // Register global callbacks
    this->register_global_callbacks();
}

/**
 * @brief Construct meshXElement with explicit element index, type, and model counts.
 */
MESHX_ELEMENT_TEMPLATE_PROTO
meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::meshXElement(uint16_t element_idx, meshxElementType_t type,
                   uint8_t no_of_sig_models, uint8_t no_of_ven_models)
    : meshXElementIF(element_idx), no_of_sig_models(no_of_sig_models), no_of_ven_models(no_of_ven_models),
      element_type(type), element_variant(MESHX_ELEMENT_TYPE_MAX), element_ctx(nullptr), element_ctx_size(0)
{
    // Register instance in global registry
    meshXElementRegistry::get_instance().register_element(this);
    // Register global callbacks
    this->register_global_callbacks();
}

MESHX_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::sig_plat_model_array_allocate()
{
    if (no_of_sig_models > 0)
    {
        sig_model_array.resize(no_of_sig_models * sizeof(MESHX_MODEL));
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
        ven_model_array.resize(no_of_ven_models * sizeof(MESHX_MODEL));
        return MESHX_SUCCESS;
    }
    return MESHX_NOT_SUPPORTED;
}

MESHX_ELEMENT_TEMPLATE_PROTO
MESHX_MODEL* meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::get_sig_plat_model_array(void)
{
    return sig_model_array.empty() ? nullptr : reinterpret_cast<MESHX_MODEL*>(sig_model_array.data());
}

MESHX_ELEMENT_TEMPLATE_PROTO
MESHX_MODEL* meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::get_ven_plat_model_array(void)
{
    return ven_model_array.empty() ? nullptr : reinterpret_cast<MESHX_MODEL*>(ven_model_array.data());
}

MESHX_ELEMENT_TEMPLATE_PROTO
const char* meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::get_element_name(void) const
{
    return "Unknown Element";
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

    uint8_t index = 0;
    for (auto& model : models)
    {
        if (model == nullptr)
        {
            return MESHX_INVALID_ARG;
        }

        // Set the platform model for the model
        model->set_plat_model(reinterpret_cast<MESHX_MODEL*>(&this->sig_model_array[index * sizeof(MESHX_MODEL)]));
        // Set the parent element for the model
        model->set_parent_element(this);

        // Add the model to the sig_models vector
        // Since sig_models is a vector of unique_ptr, we need to transfer ownership
        sig_models.emplace_back(std::move(model));
        index++;
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

    uint8_t index = 0;
    for (auto& model : models)
    {
        if (model == nullptr)
        {
            return MESHX_INVALID_ARG;
        }

        // Set the platform model for the model
        model->set_plat_model(reinterpret_cast<MESHX_MODEL*>(&this->ven_model_array[index * sizeof(MESHX_MODEL)]));
        // Set the parent element for the model
        model->set_parent_element(this);

        // Add the model to the ven_models vector
        // Since ven_models is a vector of unique_ptr, we need to transfer ownership
        ven_models.emplace_back(std::move(model));
        index++;
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
 */
MESHX_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::on_model_cb(meshx_ptr_t param, size_t param_size, const meshx_uvp_ctx_t* ctx)
{
    if (param == nullptr)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Invalid parameter in on_model_cb");
        return MESHX_INVALID_ARG;
    }
    meshx_err_t err = MESHX_SUCCESS;

    if (ctx)
    {
        /* 
         * UVP Dispatcher call: 
         * 'param' is the raw TLV payload.
         */
        err = this->element_state_change_notify(param, param_size, ctx);
    }
    else
    {
        /* 
         * Standard Model call: 
         * 'param' is meshx_model_send_param_header_t (template param).
         */
        auto *msg_header = static_cast<meshx_model_send_param_header_t *>(param);
        if(!msg_header)
        {
            MESHX_LOGE(MODULE_ID_COMMON, "Invalid message header in on_model_cb");
            return MESHX_INVALID_ARG;
        }

        if(msg_header->element_state_change == MESHX_SUCCESS)
        {
            err = this->element_state_change_notify(param, param_size, nullptr);
        }
        else
        {
            return msg_header->element_state_change;
        }
    }

    /* Common Post-Notification Logic (Persistence) */
    if(err == MESHX_SUCCESS)
    {
        /* Save updated context to NVS if it exists */
        if (element_ctx && element_ctx_size > 0)
        {
            meshx_err_t nvs_err = meshx_nvs_element_ctx_set(get_element_idx(), get_element_variant(), element_ctx, element_ctx_size);
            if(nvs_err)
            {
                MESHX_LOGE(MODULE_ID_BLE_MESH_ELEMENT, "meshx_nvs_element_ctx_set failed: %d", nvs_err);
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
    /* Restoration from NVS should happen before model initialization if needed */
    this->restore_nvs_context();

    /*
     * If platform model pointers are already set (e.g. by meshx_composition_bake),
     * we skip re-allocation and re-adding to maintain pointer stability.
     */
    bool already_initialized = false;
    if (!this->get_sig_models().empty() && this->get_sig_models()[0]->get_plat_model() != nullptr) {
        already_initialized = true;
    }

    if (!already_initialized) {
        uint8_t num_sig = this->list_sig_models();
        uint8_t num_ven = this->list_ven_models();

        this->set_no_of_sig_models(num_sig);
        this->set_no_of_ven_models(num_ven);

        this->sig_plat_model_array_allocate();
        this->ven_plat_model_array_allocate();

        /* Add all models to the element (this populates the platform model pointers) */
        this->add_models();
    } else {
        MESHX_LOGI(MODULE_ID_BLE_MESH_ELEMENT, "Element %d already has platform pointers set, skipping re-init", get_element_idx());
    }

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

MESHX_ELEMENT_TEMPLATE_PROTO
bool meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::has_model(uint16_t model_id) const
{
    // Check SIG models
    for (const auto& m : sig_models) {
        if (m && m->get_model_id() == model_id) return true;
    }

    // Check Vendor models
    for (const auto& m : ven_models) {
        if (m && m->get_model_id() == model_id) return true;
    }

    return false;
}



/*****************************************************************************************************
 * Global Static Callbacks
 *****************************************************************************************************/
MESHX_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::static_prov_srv_cb(dev_struct_t *pdev, control_task_msg_evt_t evt, void *params, uint16_t params_len)
{
    MESHX_UNUSED(pdev);
    MESHX_UNUSED(params);
    MESHX_UNUSED(params_len);

    // Iterate through all elements and call sync only for server elements
    auto elements = meshXElementRegistry::get_instance().get_all_elements();
    for (auto const& [abs_id, base_el] : elements)
    {
        if (base_el && base_el->get_element_type() == meshxElementType_t::MESHX_ELEMENT_TYPE_SERVER)
        {
            base_el->sync(evt);
        }
    }
    return MESHX_SUCCESS;
}

MESHX_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::static_prov_cli_cb(dev_struct_t *pdev, control_task_msg_evt_t evt, void *params, uint16_t params_len)
{
    MESHX_UNUSED(pdev);
    MESHX_UNUSED(params);
    MESHX_UNUSED(params_len);

    // Iterate through all elements and call sync only for client elements
    auto elements = meshXElementRegistry::get_instance().get_all_elements();
    for (auto const& [abs_id, base_el] : elements)
    {
        if (base_el && base_el->get_element_type() == meshxElementType_t::MESHX_ELEMENT_TYPE_CLIENT)
        {
            base_el->sync(evt);
        }
    }
    return MESHX_SUCCESS;
}

MESHX_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::static_config_cb(dev_struct_t *pdev, control_task_msg_evt_t evt, void *params, uint16_t params_len)
{
    const meshx_config_srv_cb_param_t *p_params = (const meshx_config_srv_cb_param_t *)params;
    MESHX_UNUSED(pdev);
    MESHX_UNUSED(params_len);
    if (!p_params) return MESHX_INVALID_ARG;

    uint16_t element_addr = 0;

    if (evt == CONTROL_TASK_MSG_EVT_APP_KEY_BIND)
    {
        element_addr = p_params->state_change.mod_app_bind.element_addr;
    }
    else if (evt == CONTROL_TASK_MSG_EVT_PUB_ADD || evt == CONTROL_TASK_MSG_EVT_PUB_DEL)
    {
        element_addr = p_params->state_change.mod_pub_set.element_addr;
    }
    else
    {
        return MESHX_SUCCESS; // Not an event we handle context for directly
    }

    // Find element by address (abs_id)
    auto *el = meshXElementRegistry::get_instance().find_element(element_addr);
    if (el)
    {
        // Update common context first (pattern matching)
        auto *ctx = static_cast<meshx_element_common_ctx_t *>(el->get_element_ctx());
        if (ctx)
        {
            bool save = false;
            if (evt == CONTROL_TASK_MSG_EVT_APP_KEY_BIND)
            {
                ctx->app_id = p_params->state_change.mod_app_bind.app_idx;
                save = true;
            }
            else if (evt == CONTROL_TASK_MSG_EVT_PUB_ADD || evt == CONTROL_TASK_MSG_EVT_PUB_DEL)
            {
                if (evt == CONTROL_TASK_MSG_EVT_PUB_ADD) {
                    ctx->pub_addr = p_params->state_change.mod_pub_set.pub_addr;
                    ctx->app_id   = p_params->state_change.mod_pub_set.app_idx;
                } else {
                    ctx->pub_addr = MESHX_ADDR_UNASSIGNED;
                }
                save = true;
            }

            if (save)
            {
                meshx_nvs_element_ctx_set(el->get_element_idx(), el->get_element_variant(),
                                         el->get_element_ctx(), el->get_element_ctx_size());
            }
        }

        // Notify element for any specialized handling
        el->handle_config(evt, p_params);
    }
    return MESHX_SUCCESS;
}

MESHX_ELEMENT_TEMPLATE_PROTO
void meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::register_global_callbacks(void)
{
    static std::once_flag registered;
    std::call_once(registered, [](){
#if CONFIG_ENABLE_CONFIG_SERVER
        meshx_config_server_cb_reg((config_srv_cb_t)static_config_cb, 0xFF); // Subscribe to all
#endif
#if CONFIG_ENABLE_PROVISIONING
        meshx_prov_srv_reg_el_server_cb((prov_srv_cb_t)static_prov_srv_cb);
        meshx_prov_srv_reg_el_client_cb((prov_srv_cb_t)static_prov_cli_cb);
#endif
    });
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

