/**
 * @file meshx_rgb_element.hpp
 * @brief MeshX RGB (HSL) Element class definition
 * This file contains the meshXRGBServerElement class which represents an RGB (HSL) element
 * in the MeshX BLE mesh network.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#ifndef __MESHX_RGB_ELEMENT_HPP__
#define __MESHX_RGB_ELEMENT_HPP__

#include <meshx_element_class.hpp>
#include <generic_model/meshx_model_onoff.hpp>
#include <light_model/meshx_model_hsl.hpp>
#include <mutex>
#include <array>

#define MESHX_RGB_SERVER_ELEMENT_TEMPLATE_PROTO
#define MESHX_RGB_SERVER_ELEMENT_TEMPLATE_PARAMS

/**
 * @brief RGB server element context structure
 * @details This structure contains the state context for the RGB server element,
 *          including the OnOff and HSL states and publication/app binding information.
 */
struct meshx_rgb_srv_el_ctx_t
{
    uint16_t                        app_id;             /**< Application key ID for publication */
    uint16_t                        pub_addr;           /**< Publication address */
    meshx_gen_onoff_model_state_t   gen_on_off_state;   /**< Current OnOff state */
    meshx_light_hsl_model_state_t   light_hsl_state;    /**< Current HSL state */
};

using meshx_rgb_srv_el_ctx_t = struct meshx_rgb_srv_el_ctx_t;

/*********************************************************************************
 * meshXRGBServerElement
 *********************************************************************************/

enum class meshxRGBServerElementComposition : uint8_t
{
    MESHX_RGB_SERVER_ELEMENT_COMP_GENERIC_ONOFF_SERVER = 0,
    MESHX_RGB_SERVER_ELEMENT_COMP_LIGHT_HSL_SERVER,
    MESHX_RGB_SERVER_ELEMENT_COMP_MAX,
};

MESHX_RGB_SERVER_ELEMENT_TEMPLATE_PROTO
class meshXRGBServerElement : public meshXElementServer MESHX_RGB_SERVER_ELEMENT_TEMPLATE_PARAMS
{
private:
    meshx_rgb_srv_el_ctx_t element_ctx;

    uint8_t list_sig_models (void) override;
    uint8_t list_ven_models (void) override;

    /**
     * @brief Notify element of state change from child model.
     */
    meshx_err_t element_state_change_notify(meshx_ptr_t param, size_t param_size) override;

    /**
     * @brief Synchronize element state (Status broadcast)
     */
    void sync(control_task_msg_evt_t evt) override;

    /**
     * @brief Handle configuration server events
     */
    void handle_config(control_task_msg_evt_t evt, const meshx_config_srv_cb_param_t *params) override;

    static meshx_err_t s_to_ble_cb(
        const dev_struct_t      *pdev,
        control_task_msg_evt_t   evt,
        const void              *params);

public:
    /**
     * @brief Constructs a new meshXRGBServerElement instance.
     * @param element_idx The index of the element within the node.
     */
    explicit meshXRGBServerElement (uint16_t element_idx);

    meshXRGBServerElement (void) = delete;
};

#endif /* __MESHX_RGB_ELEMENT_HPP__ */
