/**
 * @file meshx_model_lightness.hpp
 *
 * @brief Header file for MeshX Light Lightness Models
 * This file contains the declarations and definitions for the MeshX Light Lightness Models,
 * including the Light Lightness Server and Client models for basic brightness control.
 *
 * Key Features:
 * - Implements Bluetooth SIG-defined Light Lightness model
 * - Inherits from meshXServerModel and meshXClientModel templates
 * - Provides standard Light Lightness control operations
 * - Integrates with MeshX transmission control
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#ifndef _MESHX_MODEL_LIGHTNESS_HPP_
#define _MESHX_MODEL_LIGHTNESS_HPP_

#include <meshx_model_class.hpp>
#include <meshx_base_model_light.hpp>

#define MESHX_LIGHT_LIGHTNESS_CLIENT_MODEL_TEMPLATE_PROTO
#define MESHX_LIGHT_LIGHTNESS_CLIENT_MODEL_TEMPLATE_PARAMS

#define MESHX_LIGHT_LIGHTNESS_SERVER_MODEL_TEMPLATE_PROTO
#define MESHX_LIGHT_LIGHTNESS_SERVER_MODEL_TEMPLATE_PARAMS

/**
 * @brief Structure to hold the Light Lightness model state.
 */
typedef struct meshx_light_lightness_model_state
{
    uint16_t present_lightness;    /**< Present lightness value */
    uint16_t target_lightness;     /**< Target lightness value */
    uint16_t lightness_last;       /**< Last lightness value */
    uint16_t lightness_default;    /**< Default lightness value */
    uint16_t range_min;            /**< Minimum range value */
    uint16_t range_max;            /**< Maximum range value */
}meshx_light_lightness_model_state_t;

/**
 * @brief Structure to hold the parameters for sending a Light Lightness message.
 */
struct meshx_light_lightness_send_params
{
    meshx_model_t                      *model;  /**< Pointer to the Light Lightness client model. */
    meshx_ctx_t                        *ctx;    /**< The context of the message. */
    uint8_t                             tid;    /**< Transaction ID of the message. Only used by Client */
    meshx_light_lightness_model_state_t   state;  /**< The state of the message. */
};

using meshx_light_lightness_send_params_t = struct meshx_light_lightness_send_params;

#if CONFIG_ENABLE_LIGHT_LIGHTNESS_CLIENT

/**
 * @brief Structure to hold the Light Lightness Client to element message.
 */
struct meshx_light_lightness_cli_el_msg
{
    meshx_cli_model_send_param_header_t header; /**< Client model send param header */
    meshx_light_lightness_model_state_t       state;  /**< The state of the message. */
};

using meshx_light_lightness_cli_el_msg_t = struct meshx_light_lightness_cli_el_msg;

/**
 * @class meshXLightLightnessClientModel
 * @brief A template class for creating Light Lightness Client models.
 *
 * This class is derived from meshXClientModel and provides a convenient interface for
 * creating Light Lightness Client models. It handles the Light Lightness state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 */
MESHX_LIGHT_LIGHTNESS_CLIENT_MODEL_TEMPLATE_PROTO
class meshXLightLightnessClientModel MESHX_LIGHT_LIGHTNESS_CLIENT_MODEL_TEMPLATE_PARAMS
    : public meshXClientModel<meshXBaseLightClientModel, meshx_light_lightness_send_params_t>
{
private:
    /* New or updated model state from BLE layer */
    meshx_light_lightness_model_state_t model_state;

    meshx_err_t meshx_state_change_notify   (
        const meshx_gen_light_cli_cb_param_t *param,
        uint8_t status
    );

    meshx_err_t element_state_change_handle (void) override;

public:
    meshx_err_t model_send          (meshx_light_lightness_send_params_t *params) override;
    meshx_err_t model_from_ble_cb   (dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;

    meshXLightLightnessClientModel(
        meshXElementIF *parent_element = nullptr,
        meshx_ptr_t     parent_element_state = nullptr
    );
    ~meshXLightLightnessClientModel() = default;
};

#endif /* CONFIG_ENABLE_LIGHT_LIGHTNESS_CLIENT */

/********************************************************************************************************************************** */
#if CONFIG_ENABLE_LIGHT_LIGHTNESS_SERVER

/**
 * @brief Structure to hold the Light Lightness Server to element message.
 */
struct meshx_light_lightness_srv_el_msg
{
    meshx_srv_model_send_param_header_t header; /**< Server model send param header */
    meshx_light_lightness_model_state_t       state;  /**< The state of the message. */
};

using meshx_light_lightness_srv_el_msg_t = struct meshx_light_lightness_srv_el_msg;

/**
 * @class meshXLightLightnessServerModel
 * @brief A template class for creating Light Lightness Server models.
 *
 * This class is derived from meshXServerModel and provides a convenient interface for
 * creating Light Lightness Server models. It handles the Light Lightness state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 */
MESHX_LIGHT_LIGHTNESS_SERVER_MODEL_TEMPLATE_PROTO
class meshXLightLightnessServerModel MESHX_LIGHT_LIGHTNESS_SERVER_MODEL_TEMPLATE_PARAMS
    : public meshXServerModel<meshXBaseLightServerModel, meshx_light_lightness_send_params_t>
{
private:
    /* New or updated model state from BLE layer */
    meshx_light_lightness_model_state_t model_state;

    meshx_err_t plat_model_create   (void) override;
    meshx_err_t plat_model_delete   (void) override;
    meshx_err_t element_state_change_handle (void) override;

public:
    meshx_err_t model_send          (meshx_light_lightness_send_params_t *params) override;
    meshx_err_t model_from_ble_cb   (dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;

    meshXLightLightnessServerModel(
        meshXElementIF *parent_element = nullptr,
        meshx_ptr_t     parent_element_state = nullptr
    );
    ~meshXLightLightnessServerModel() = default;
};

#endif /* CONFIG_ENABLE_LIGHT_LIGHTNESS_SERVER */

#endif /* _MESHX_MODEL_LIGHTNESS_HPP_ */
