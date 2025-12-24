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
    uint8_t list_sig_models() override;
    uint8_t list_ven_models() override;
public:
    /**
     * @brief Constructs a new meshXCWWWServerElement instance.
     * The meshXCWWWServerElement represents a CWWW server element in the MeshX BLE mesh network.
     * It automatically initializes and configures all required SIG models for the CWWW element.
     * The CWWW element combines Generic OnOff and Light CTL server models.
     */
    meshXCWWWServerElement() = default;

    meshXCWWWServerElement(uint16_t element_idx)
        : meshXElementServer(element_idx) { };
};

#endif /* CONFIG_LIGHT_CWWW_SRV_COUNT */

#if CONFIG_LIGHT_CWWW_CLIENT_COUNT > 0
/*********************************************************************************
 * meshXCWWWClientElement
 *********************************************************************************/

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
    uint8_t list_sig_models() override;
    uint8_t list_ven_models() override;
public:
    /**
     * @brief Constructs a new meshXCWWWClientElement instance.
     * The meshXCWWWClientElement represents a CWWW client element in the MeshX BLE mesh network.
     * It automatically initializes and configures all required SIG models for the CWWW element.
     * The CWWW element combines Generic OnOff and Light CTL client models.
     */
    meshXCWWWClientElement() = default;

    meshXCWWWClientElement(uint16_t element_idx)
        : meshXElementClient(element_idx) { };
};
#endif /* CONFIG_LIGHT_CWWW_CLIENT_COUNT */

#endif /* __MESHX_CWWW_ELEMENT_HPP__ */
