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

#define MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PROTO
#define MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PARAMS

#define MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PROTO
#define MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PARAMS

/**
 * @brief CWWW server element context structure
 * @details This structure contains the state context for the CWWW server element,
 *          including OnOff state, CTL state, and publication/app binding information.
 *          This matches the C implementation pattern where state is maintained
 *          in the element layer (el_ctx) for NVS persistence.
 */
struct meshx_cwww_srv_el_ctx_t
{
    // OnOff state
    uint8_t on_off_state;  /**< Current OnOff state (0=OFF, 1=ON) */

    // CTL state
    int16_t delta_uv;          /**< Current delta UV value (-32768-32767) */
    uint16_t lightness;        /**< Current lightness value (0-65535) */
    uint16_t temperature;      /**< Current color temperature value (800-20000) */
    uint16_t temp_range_min;   /**< Minimum temperature range */
    uint16_t temp_range_max;   /**< Maximum temperature range */

    // Publication and app binding
    uint8_t app_id;        /**< Application key ID for publication */
    uint16_t pub_addr;     /**< Publication address */
};

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
