/**
 * @file meshx_cwww_element.cpp
 * @brief Implementation of MeshX CWWW Element.
 *        This file contains the implementation of the MeshX CWWW Element class,
 *        which represents a CWWW (Cool White - Warm White) element in the MeshX BLE mesh network.
 * Key Features:
 * - Implements CWWW element functionality
 * - Inherits from meshXElementServer and meshXElementClient
 * - Automatically initializes required SIG models for CWWW elements
 * - The CWWW element is a combination of Generic OnOff and Light CTL server/clients
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#include <variants/meshx_cwww_element.hpp>
#include <generic_model/meshx_model_onoff.hpp>
#include <light_model/meshx_model_ctl.hpp>

#if CONFIG_LIGHT_CWWW_SRV_COUNT > 0
/*********************************************************************************
 * meshXCWWWServerElement
 *********************************************************************************/

/**
 * @brief Constructs a new meshXCWWWServerElement instance.
 * The meshXCWWWServerElement represents a CWWW server element in the MeshX BLE mesh network.
 * It automatically initializes and configures all required SIG models for the CWWW element.
 * The CWWW element combines Generic OnOff and Light CTL server models.
 *
 * @param element_idx The index of the element within the node.
 */
MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PROTO
meshXCWWWServerElement MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PARAMS
    :: meshXCWWWServerElement(uint16_t element_idx)
    : meshXElementServer(element_idx)
{
    this->register_element_ctx(
        &element_ctx,
        sizeof(meshx_cwww_srv_el_ctx_t)
    );
}

/**
 * @brief Lists and initializes SIG models for CWWW Server Element
 *
 * This function creates and adds the Generic OnOff Server and Light CTL Server models
 * to the element's SIG models list. The CWWW Server element requires both models
 * for proper operation in the BLE mesh network.
 *
 * @return uint8_t Number of SIG models added to the element
 */
MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PROTO
uint8_t meshXCWWWServerElement MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PARAMS
    :: list_sig_models()
{
    // Create Generic OnOff Server model
    auto onoff_model = std::make_unique<meshXGenericOnOffServerModel>(this, &this->element_ctx.gen_on_off_state);
    this->get_sig_models().push_back(std::move(onoff_model));

    // Create Light CTL Server model
    auto ctl_model = std::make_unique<meshXLightCTLServerModel>(this, &this->element_ctx.light_ctl_state);
    this->get_sig_models().push_back(std::move(ctl_model));

    return this->get_sig_models().size();
}

/**
 * @brief Lists vendor-specific models for CWWW Server Element
 *
 * This function returns the count of vendor-specific models for the CWWW Server element.
 * Currently, no vendor-specific models are supported for CWWW elements.
 *
 * @return uint8_t Number of vendor models (always 0)
 */
MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PROTO
uint8_t meshXCWWWServerElement MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PARAMS
    :: list_ven_models()
{
    return 0;
}

#endif /* CONFIG_LIGHT_CWWW_SRV_COUNT */

#if CONFIG_LIGHT_CWWW_CLIENT_COUNT > 0
/*********************************************************************************
 * meshXCWWWClientElement
 *********************************************************************************/

/**
 * @brief Constructs a new meshXCWWWClientElement instance.
 * The meshXCWWWClientElement represents a CWWW client element in the MeshX BLE mesh network.
 * It automatically initializes and configures all required SIG models for the CWWW element.
 * The CWWW element combines Generic OnOff and Light CTL client models.
 *
 * @param element_idx The index of the element within the node.
 */
MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PROTO
meshXCWWWClientElement MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PARAMS
    :: meshXCWWWClientElement(uint16_t element_idx)
    : meshXElementClient(element_idx)
{
    this->register_element_ctx(
        &element_ctx,
        sizeof(meshx_cwww_cli_el_ctx_t)
    );
}

/**
 * @brief Lists and initializes SIG models for CWWW Client Element
 *
 * This function creates and adds the Generic OnOff Client and Light CTL Client models
 * to the element's SIG models list. The CWWW Client element requires both models
 * for proper operation in the BLE mesh network.
 *
 * @return uint8_t Number of SIG models added to the element
 */
MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PROTO
uint8_t meshXCWWWClientElement MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PARAMS
    :: list_sig_models()
{
    // Create Generic OnOff Client model
    auto onoff_model = std::make_unique<meshXGenericOnOffClientModel>(this, &this->element_ctx.gen_on_off_state);
    this->get_sig_models().push_back(std::move(onoff_model));

    // Create Light CTL Client model
    auto ctl_model = std::make_unique<meshXLightCTLClientModel>(this, &this->element_ctx.light_ctl_state);
    this->get_sig_models().push_back(std::move(ctl_model));

    return this->get_sig_models().size();
}

/**
 * @brief Lists vendor-specific models for CWWW Client Element
 *
 * This function returns the count of vendor-specific models for the CWWW Client element.
 * Currently, no vendor-specific models are supported for CWWW elements.
 *
 * @return uint8_t Number of vendor models (always 0)
 */
MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PROTO
uint8_t meshXCWWWClientElement MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PARAMS
    :: list_ven_models()
{
    return 0;
}

#endif /* CONFIG_LIGHT_CWWW_CLIENT_COUNT */
