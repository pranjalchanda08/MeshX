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
    // Create Configuration Server model (always required)
    auto config_model = std::make_unique<meshXConfigModel>(this);
    this->get_sig_models().push_back(std::move(config_model));

    return (uint8_t)this->get_sig_models().size();
}

/**
 * @brief Lists vendor-specific models for Root Element
 *
 * This function returns the count of vendor-specific models for the Root Element.
 * Currently, no vendor-specific models are supported for root elements.
 *
 * @return uint8_t Number of vendor models (always 0)
 */
uint8_t meshXRootElement :: list_ven_models()
{
    return 0;
}

/**
 * @brief Handle model callback from child models.
 *
 * This function is called by child models when a state change occurs.
 * It handles the state change by logging the event.
 * The root element only has a Configuration Server model.
 *
 * @param[in] param Pointer to the model callback parameter
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
    MESHX_LOGI(MODULE_ID_ELEMENT_ROOT,
        "Root element received model callback");

    // For now, just log the event and return success

    return MESHX_SUCCESS;
}
