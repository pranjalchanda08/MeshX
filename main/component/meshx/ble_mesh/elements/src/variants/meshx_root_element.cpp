/**
 * @file meshx_element_class.hpp
 * @brief MeshX Element class and interface definations
 * The meshXElement class represents an element in the MeshX BLE mesh network,
 * while the meshXElementIF interface defines the callback function for model events.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#include <variants/meshx_root_element.hpp>
#include <common_model/meshx_model_config.hpp>

#include <meshx_composition.hpp>
 
extern "C" meshx_err_t meshx_init_config_server(void);

constexpr uint16_t MESHX_ROOT_ELEMENT_INDEX = 0;
/*********************************************************************************
 * meshXRootElement
 *********************************************************************************/

/**
 * @brief Constructor for Root Element
 *
 * This constructor initializes a Root Element with the predefined element index.
 * All required SIG models are added in list_sig_models() method.
 * Auto initialization is taken care by meshXElement constructor.
 */
meshXRootElement
    :: meshXRootElement()
    : meshXElementServer( MESHX_ROOT_ELEMENT_INDEX )
{
    // All required SIG models are added in list_sig_models()
    // Auto initialisation taken care by meshXElement constructor
}

/**
 * @brief Lists and initializes SIG models for Root Element
 *
 * This function creates and adds the Configuration Server model to the element's
 * SIG models list. The Configuration Server model is always required for
 * root elements in the BLE mesh network.
 *
 * @return uint8_t Number of SIG models added to the element
 */
uint8_t meshXRootElement :: list_sig_models()
{
    // 1. Initialize platform Config Server
    meshx_init_config_server();
 
    // 2. Create Configuration Server model (always required)
    auto config_model = std::make_unique<meshXConfigModel>(
        this,
        nullptr,
        (uint16_t) static_cast<int>(meshxRootElementComposition::MESHX_ROOT_ELEMENT_COMP_CONFIG_SERVER)
    );
    this->get_sig_models().push_back(std::move(config_model));
 

 
    return (uint8_t)this->get_sig_models().size();
}
 
uint8_t meshXRootElement::list_ven_models() {
    return 0; // Root element usually has no vendor models
}

const char* meshXRootElement::get_element_name(void) const
{
    return "Root Element";
}
 

/**
 * @brief Handle model callback from child models.
 *
 * This function is called by child models when a state change occurs.
 * It handles the state change by logging the event.
 * The root element only has a Configuration Server model.
 *
 * @param[in] param Pointer to the model callback parameter
 * @param[in] param_size Size of the parameter structure
 * @return
 *     - MESHX_SUCCESS: State change handled successfully
 *     - MESHX_INVALID_ARG: Invalid parameter
 */
meshx_err_t meshXRootElement :: element_state_change_notify(meshx_ptr_t param, size_t param_size)
{
    if (!param) {
        MESHX_LOGE(MODULE_ID_ELEMENT_ROOT, "Invalid parameter in element_state_change_notify");
        return MESHX_INVALID_ARG;
    }

    // Root element currently only has Config Server model
    // Config model events are logged but not processed further
    MESHX_LOGI(MODULE_ID_ELEMENT_ROOT, "Root element received model callback");

    // For now, just log the event and return success

    return MESHX_SUCCESS;
}

void meshXRootElement::sync(control_task_msg_evt_t evt)
{
    MESHX_LOGD(MODULE_ID_ELEMENT_ROOT, "Root Element sync event: %d", evt);
}

void meshXRootElement::handle_config(control_task_msg_evt_t evt, const meshx_config_srv_cb_param_t *params)
{
    MESHX_LOGD(MODULE_ID_ELEMENT_ROOT, "Root Element config event: %d", evt);
}
