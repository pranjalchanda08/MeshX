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
#include <mutex>
#include <array>

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

enum class meshxCWWWServerElementComposition : uint8_t
{
    MESHX_CWWW_SERVER_ELEMENT_COMP_GENERIC_ONOFF_SERVER = 0,
    MESHX_CWWW_SERVER_ELEMENT_COMP_LIGHT_CTL_SERVER,
    MESHX_CWWW_SERVER_ELEMENT_COMP_MAX,
};

MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PROTO
class meshXCWWWServerElement : public meshXElementServer MESHX_CWWW_SERVER_ELEMENT_TEMPLATE_PARAMS
{
private:
    meshx_cwww_srv_el_ctx_t element_ctx;

    uint8_t list_sig_models() override;
    uint8_t list_ven_models() override;

    /**
     * @brief Notify element of state change from child model.
     * @details Discriminates ONOFF vs CTL via header->model.model_id.
     *          Updates element_ctx, saves to NVS, notifies app.
     */
    meshx_err_t element_state_change_notify(meshx_ptr_t param, size_t param_size) override;

    /*-----------------------------------------------------------------
     * Static class-level callbacks (registered once via once_flag)
     *----------------------------------------------------------------*/
#if CONFIG_ENABLE_CONFIG_SERVER
    static meshx_err_t s_config_srv_cb(
        const dev_struct_t              *pdev,
        control_task_msg_evt_t           evt,
        const meshx_config_srv_cb_param_t *params);
#endif

#if CONFIG_ENABLE_PROVISIONING
    static meshx_err_t s_prov_cb(
        const dev_struct_t      *pdev,
        control_task_msg_evt_t   evt,
        const void              *params);
#endif

    static meshx_err_t s_to_ble_cb(
        const dev_struct_t      *pdev,
        control_task_msg_evt_t   evt,
        const void              *params);

    /*-----------------------------------------------------------------
     * Instance registry
     *----------------------------------------------------------------*/
    static std::once_flag s_callbacks_registered;
    static void register_class_callbacks();

public:
    /**
     * @brief Constructs a new meshXCWWWServerElement instance.
     * The meshXCWWWServerElement represents a CWWW server element in the MeshX BLE mesh network.
     * It automatically initializes and configures all required SIG models for the CWWW element.
     * The CWWW element combines Generic OnOff and Light CTL server models.
     *
     * @param element_idx The index of the element within the node.
     */
    explicit meshXCWWWServerElement(uint16_t element_idx);

    meshXCWWWServerElement() = delete;
};

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

enum class meshxCWWWClientElementComposition : uint8_t
{
    MESHX_CWWW_CLIENT_ELEMENT_COMP_GENERIC_ONOFF_CLIENT = 0,
    MESHX_CWWW_CLIENT_ELEMENT_COMP_LIGHT_CTL_CLIENT,
    MESHX_CWWW_CLIENT_ELEMENT_COMP_MAX,
};

MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PROTO
class meshXCWWWClientElement : public meshXElementClient MESHX_CWWW_CLIENT_ELEMENT_TEMPLATE_PARAMS
{
private:
    meshx_cwww_cli_el_ctx_t element_ctx;

    uint8_t list_sig_models() override;
    uint8_t list_ven_models() override;

    /**
     * @brief Notify element of state change from child model.
     * @details Discriminates ONOFF vs CTL via header->model.model_id.
     *          Updates element_ctx and notifies app.
     */
    meshx_err_t element_state_change_notify(meshx_ptr_t param, size_t param_size) override;

    /*-----------------------------------------------------------------
     * Static class-level callbacks (registered once via once_flag)
     *----------------------------------------------------------------*/
#if CONFIG_ENABLE_CONFIG_SERVER
    static meshx_err_t s_config_srv_cb(
        const dev_struct_t              *pdev,
        control_task_msg_evt_t           evt,
        const meshx_config_srv_cb_param_t *params);
#endif

#if CONFIG_ENABLE_PROVISIONING
    static meshx_err_t s_prov_cb(
        const dev_struct_t      *pdev,
        control_task_msg_evt_t   evt,
        const void              *params);
#endif

    static meshx_err_t s_to_ble_cb(
        const dev_struct_t      *pdev,
        control_task_msg_evt_t   evt,
        const void              *params);

    /*-----------------------------------------------------------------
     * Instance registry
     *----------------------------------------------------------------*/
    static std::once_flag s_callbacks_registered;
    static void register_class_callbacks();

public:
    /**
     * @brief Constructs a new meshXCWWWClientElement instance.
     * The meshXCWWWClientElement represents a CWWW client element in the MeshX BLE mesh network.
     * It automatically initializes and configures all required SIG models for the CWWW element.
     * The CWWW element combines Generic OnOff and Light CTL client models.
     *
     * @param element_idx The index of the element within the node.
     */
    explicit meshXCWWWClientElement(uint16_t element_idx);
    meshXCWWWClientElement() = delete;
};

#endif /* __MESHX_CWWW_ELEMENT_HPP__ */
