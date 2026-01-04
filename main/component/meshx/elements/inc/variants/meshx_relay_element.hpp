/**
 * @file meshx_relay_element.hpp
 * @brief MeshX Relay Element class definition
 * This file contains the meshXRelayServerElement class which represents a relay element
 * in the MeshX BLE mesh network.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#ifndef __MESHX_RELAY_ELEMENT_HPP__
#define __MESHX_RELAY_ELEMENT_HPP__

#include <meshx_element_class.hpp>
#include <generic_model/meshx_model_onoff.hpp>

#define MESHX_RELAY_SERVER_ELEMENT_TEMPLATE_PROTO
#define MESHX_RELAY_SERVER_ELEMENT_TEMPLATE_PARAMS

#define MESHX_RELAY_CLIENT_ELEMENT_TEMPLATE_PROTO
#define MESHX_RELAY_CLIENT_ELEMENT_TEMPLATE_PARAMS

#if CONFIG_RELAY_SERVER_COUNT > 0

/**
 * @brief Relay server element context structure
 * @details This structure contains the state context for the relay server element,
 *          including the OnOff state and publication/app binding information.
 *          This matches the C implementation pattern where state is maintained
 *          in the element layer for NVS persistence.
 */
struct meshx_relay_srv_el_ctx_t
{
    uint8_t                         app_id;             /**< Application key ID for publication */
    uint16_t                        pub_addr;           /**< Publication address */
    meshx_gen_onoff_model_state_t   gen_on_off_state;   /**< Current OnOff state (0=OFF, 1=ON) */
};

using meshx_relay_srv_el_ctx_t = struct meshx_relay_srv_el_ctx_t;

/*********************************************************************************
 * meshXRelayServerElement
 *********************************************************************************/

MESHX_RELAY_SERVER_ELEMENT_TEMPLATE_PROTO
class meshXRelayServerElement : public meshXElementServer MESHX_RELAY_SERVER_ELEMENT_TEMPLATE_PARAMS
{
private:
    meshx_relay_srv_el_ctx_t element_ctx;

    uint8_t list_sig_models (void) override;
    uint8_t list_ven_models (void) override;
public:

    /**
     * @brief Constructs a new meshXRelayServerElement instance.
     * The meshXRelayServerElement represents a relay element in the MeshX BLE mesh network.
     * It automatically initializes and configures all required SIG models for the relay element.
     *
     * @param element_idx The index of the element within the node.
     */
    meshXRelayServerElement (uint16_t element_idx);

    meshXRelayServerElement (void) = delete;
};

#endif /* CONFIG_RELAY_SERVER_COUNT */

#if CONFIG_RELAY_CLIENT_COUNT > 0
/*********************************************************************************
 * meshXRelayClientElement
 *********************************************************************************/
/**
 * @brief Relay client element context structure
 * @details This structure contains state context for relay client element,
 *          including OnOff state and publication/app binding information.
 *          This matches to C implementation pattern where state is maintained
 *          in element layer for NVS persistence.
 */
struct meshx_relay_cli_el_ctx_t
{
    uint8_t                         app_id;             /**< Application key ID for publication */
    uint16_t                        pub_addr;           /**< Publication address */
    meshx_gen_onoff_model_state_t   gen_on_off_state;   /**< Current OnOff state (0=OFF, 1=ON) */
};

using meshx_relay_cli_el_ctx_t = struct meshx_relay_cli_el_ctx_t;

MESHX_RELAY_CLIENT_ELEMENT_TEMPLATE_PROTO
class meshXRelayClientElement : public meshXElementClient MESHX_RELAY_CLIENT_ELEMENT_TEMPLATE_PARAMS
{
private:

    meshx_relay_cli_el_ctx_t element_ctx;

    uint8_t list_sig_models (void) override;
    uint8_t list_ven_models (void) override;
public:
    /**
     * @brief Constructs a new meshXRelayClientElement instance.
     * The meshXRelayClientElement represents a relay client element in the MeshX BLE mesh network.
     * It automatically initializes and configures all required SIG models for the relay client element.
     *
     * @param element_idx The index of the element within the node.
     */
    meshXRelayClientElement (uint16_t element_idx);

    meshXRelayClientElement (void) = delete;
};
#endif /* CONFIG_RELAY_CLIENT_COUNT */

#endif /* __MESHX_RELAY_ELEMENT_HPP__ */
