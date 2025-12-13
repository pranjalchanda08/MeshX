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

meshXRootElement
    :: meshXRootElement()
    : meshXElementServer( MESHX_ROOT_ELEMENT_INDEX )
{
    // All required SIG models are added in list_sig_models()
    // Auto initialisation taken care by meshXElement constructor
}

uint8_t meshXRootElement :: list_sig_models()
{
    // Create Configuration Server model (always required)
    auto config_model = std::make_unique<meshXConfigModel>(
        nullptr, MESHX_MODEL_ID_CONFIG_SRV, this);
    this->get_sig_models().push_back(std::move(config_model));

    return this->get_sig_models().size();
}

uint8_t meshXRootElement :: list_ven_models()
{
    return 0;
}
