/**
 * @file meshx_cwww_element.hpp
 * @brief MeshX CWWW (Cool White - Warm White) Element class definition
 * This file contains the meshXCWWWServerElement and meshXCWWWClientElement classes
 * which represent CWWW elements in the MeshX BLE mesh network.
 * The CWWW element is a combination of Generic OnOff and Light CTL server/clients.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#ifndef __MESHX_CWWW_ELEMENT_HPP__
#define __MESHX_CWWW_ELEMENT_HPP__

#include <meshx_element_class.hpp>
#include <generic_model/meshx_model_onoff.hpp>
#include <light_model/meshx_model_ctl.hpp>

#define MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PROTO
#define MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PARAMS

#define MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PROTO
#define MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PARAMS

#if CONFIG_LIGHT_CWWW_SRV_COUNT > 0

/**
 * @brief CWWW server element context structure
 * @details This structure contains the state context for the CWWW server element,
 *          including OnOff state, CTL state, and publication/app binding information.
 *          This matches the C implementation pattern where state is maintained
 *          in the element layer (el_ctx) for NVS persistence.
 */
struct meshx_cwww_srv_el_ctx_t
{
    // Publication and app binding
    uint8_t app_id;        /**< Application key ID for publication */
    uint16_t pub_addr;     /**< Publication address */
    // Generic OnOff state
    meshx_gen_onoff_model_state_t   gen_on_off_state;   /**< Current OnOff state (0=OFF, 1=ON) */
    // Light CTL state
    meshx_light_ctl_model_state_t   light_ctl_state;    /**< Current Light CTL state */
};

using meshx_cwww_srv_el_ctx_t = struct meshx_cwww_srv_el_ctx_t;

/*********************************************************************************
 * meshXCWWWServerElement
 *********************************************************************************/

/**
 * @brief Enumeration of CWWW SIG model IDs for server element.
 */
typedef enum
{
    CWWW_SRV_SIG_ONOFF_MODEL_ID, /**< On/Off model ID */
    CWWW_SRV_SIG_L_CTL_MODEL_ID, /**< Light CTL model ID */
    CWWW_SRV_SIG_ID_MAX          /**< Maximum number of model IDs */
} cwww_srv_sig_id_t;

MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PROTO
class meshXCWWWServerElement : public meshXElementServer MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PARAMS
{
private:
    meshx_cwww_srv_el_ctx_t element_ctx;

    uint8_t list_sig_models() override;
    uint8_t list_ven_models() override;
public:
    /**
     * @brief Constructs a new meshXCWWWServerElement instance.
     * The meshXCWWWServerElement represents a CWWW server element in the MeshX BLE mesh network.
     * It automatically initializes and configures all required SIG models for the CWWW element.
     * The CWWW element combines Generic OnOff and Light CTL server models.
     *
     * @param element_idx The index of the element within the node.
     */
    meshXCWWWServerElement(uint16_t element_idx);

    meshXCWWWServerElement() = delete;
};

#endif /* CONFIG_LIGHT_CWWW_SRV_COUNT */

#if CONFIG_LIGHT_CWWW_CLIENT_COUNT > 0
/*********************************************************************************
 * meshXCWWWClientElement
 *********************************************************************************/

/**
 * @brief CWWW client element context structure
 * @details This structure contains the state context for the CWWW client element,
 *          including OnOff state, CTL state, and publication/app binding information.
 *          This matches the C implementation pattern where state is maintained
 *          in the element layer (el_ctx) for NVS persistence.
 */
struct meshx_cwww_cli_el_ctx_t
{
    // Publication and app binding
    uint8_t app_id;        /**< Application key ID for publication */
    uint16_t pub_addr;     /**< Publication address */
    // Generic OnOff state
    meshx_gen_onoff_model_state_t   gen_on_off_state;   /**< Current OnOff state (0=OFF, 1=ON) */
    // Light CTL state
    meshx_light_ctl_model_state_t   light_ctl_state;    /**< Current Light CTL state */
};

using meshx_cwww_srv_el_ctx_t = struct meshx_cwww_srv_el_ctx_t;
/**
 * @brief Enumeration of CWWW SIG model IDs for client element.
 */
typedef enum
{
    CWWW_CLI_SIG_ONOFF_MODEL_ID, /**< On/Off model ID */
    CWWW_CLI_SIG_L_CTL_MODEL_ID, /**< Light CTL model ID */
    CWWW_CLI_SIG_ID_MAX          /**< Maximum number of model IDs */
} cwww_cli_sig_id_t;

MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PROTO
class meshXCWWWClientElement : public meshXElementClient MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PARAMS
{
private:
    meshx_cwww_cli_el_ctx_t element_ctx;
    uint8_t list_sig_models() override;
    uint8_t list_ven_models() override;
public:
    /**
     * @brief Constructs a new meshXCWWWClientElement instance.
     * The meshXCWWWClientElement represents a CWWW client element in the MeshX BLE mesh network.
     * It automatically initializes and configures all required SIG models for the CWWW element.
     * The CWWW element combines Generic OnOff and Light CTL client models.
     *
     * @param element_idx The index of the element within the node.
     */
    meshXCWWWClientElement(uint16_t element_idx);
    meshXCWWWClientElement() = delete;
};
#endif /* CONFIG_LIGHT_CWWW_CLIENT_COUNT */

#endif /* __MESHX_CWWW_ELEMENT_HPP__ */
