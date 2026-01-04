/**
 * @file meshx_relay_element.cpp
 * @brief Implementation of MeshX Relay Element.
 *        This file contains the implementation of the MeshX Relay Element class,
 *        which represents a relay element in the MeshX BLE mesh network.
 * Key Features:
 *  - Implements relay element functionality
 *  - Inherits from meshXElementServer
 *  - Automatically initializes required SIG models for relay elements
 *  - Maintains state context for NVS persistence (similar to C el_ctx)
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#include <variants/meshx_relay_element.hpp>

#if CONFIG_RELAY_SERVER_COUNT > 0
/*********************************************************************************
 * meshXRelayServerElement
 *********************************************************************************/

/**
 * @brief Constructs a new meshXRelayServerElement instance.
 * The meshXRelayServerElement represents a relay element in the MeshX BLE mesh network.
 * It automatically initializes and configures all required SIG models for the relay element.
 *
 * @param element_idx The index of the element within the node.
 */
MESHX_RELAY_SERVER_ELEMENT_TEMPLATE_PROTO
meshXRelayServerElement MESHX_RELAY_SERVER_ELEMENT_TEMPLATE_PARAMS
    :: meshXRelayServerElement(uint16_t element_idx)
    : meshXElementServer(element_idx)
{
    this->register_element_ctx(
        &element_ctx,
        sizeof(meshx_relay_srv_el_ctx_t)
    );
}
/**
 * @brief Lists and initializes SIG models for Relay Server Element
 *
 * This function creates and adds the Generic OnOff Server model
 * to the element's SIG models list. The Relay Server element requires
 * the Generic OnOff model for proper operation in the BLE mesh network.
 *
 * @return uint8_t Number of SIG models added to the element
 */
MESHX_RELAY_SERVER_ELEMENT_TEMPLATE_PROTO
uint8_t meshXRelayServerElement MESHX_RELAY_SERVER_ELEMENT_TEMPLATE_PARAMS
    :: list_sig_models()
{
    // Create Relay Server model
    auto relay_model = std::make_unique<meshXGenericOnOffServerModel>(this, &element_ctx.gen_on_off_state);
    this->get_sig_models().push_back(std::move(relay_model));

    return (uint8_t)this->get_sig_models().size();
}

/**
 * @brief Lists vendor-specific models for Relay Server Element
 *
 * This function returns the count of vendor-specific models for the Relay Server element.
 * Currently, no vendor-specific models are supported for relay elements.
 *
 * @return uint8_t Number of vendor models (always 0)
 */
MESHX_RELAY_SERVER_ELEMENT_TEMPLATE_PROTO
uint8_t meshXRelayServerElement MESHX_RELAY_SERVER_ELEMENT_TEMPLATE_PARAMS
    :: list_ven_models()
{
    return 0;
}

#endif /* CONFIG_RELAY_SERVER_COUNT */

#if CONFIG_RELAY_CLIENT_COUNT > 0
/*********************************************************************************
 * meshXRelayClientElement
 *********************************************************************************/

/**
 * @brief Constructs a new meshXRelayClientElement instance.
 * The meshXRelayClientElement represents a relay client element in the MeshX BLE mesh network.
 * It automatically initializes and configures all required SIG models for the relay client element.
 *
 * @param element_idx The index of the element within the node.
 */
MESHX_RELAY_CLIENT_ELEMENT_TEMPLATE_PROTO
meshXRelayClientElement MESHX_RELAY_CLIENT_ELEMENT_TEMPLATE_PARAMS
    :: meshXRelayClientElement(uint16_t element_idx)
    : meshXElementClient(element_idx)
{
    this->register_element_ctx(
        &element_ctx,
        sizeof(meshx_relay_cli_el_ctx_t)
    );
}
/**
 * @brief Lists and initializes SIG models for Relay Client Element
 *
 * This function creates and adds the Generic OnOff Client model
 * to the element's SIG models list. The Relay Client element requires
 * the Generic OnOff model for proper operation in the BLE mesh network.
 *
 * @return uint8_t Number of SIG models added to the element
 */
MESHX_RELAY_CLIENT_ELEMENT_TEMPLATE_PROTO
uint8_t meshXRelayClientElement MESHX_RELAY_CLIENT_ELEMENT_TEMPLATE_PARAMS
    :: list_sig_models()
{
    // Create Relay Client model
    auto relay_model = std::make_unique<meshXGenericOnOffClientModel>(this, &element_ctx.gen_on_off_state);
    this->get_sig_models().push_back(std::move(relay_model));
    return (uint8_t)this->get_sig_models().size();
}

/**
 * @brief Lists vendor-specific models for Relay Client Element
 *
 * This function returns the count of vendor-specific models for the Relay Client element.
 * Currently, no vendor-specific models are supported for relay elements.
 *
 * @return uint8_t Number of vendor models (always 0)
 */
MESHX_RELAY_CLIENT_ELEMENT_TEMPLATE_PROTO
uint8_t meshXRelayClientElement MESHX_RELAY_CLIENT_ELEMENT_TEMPLATE_PARAMS
    :: list_ven_models()
{
    return 0;
}

#endif /* CONFIG_RELAY_CLIENT_COUNT */
