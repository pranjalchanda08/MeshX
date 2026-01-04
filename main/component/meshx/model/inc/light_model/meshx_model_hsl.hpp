/**
 * @file meshx_model_hsl.hpp
 *
 * @brief Header file for MeshX Light HSL Models
 * This file contains the declarations and definitions for the MeshX Light HSL Models,
 * including the Light HSL Server and Client models for hue, saturation, and lightness control.
 *
 * Key Features:
 * - Implements Bluetooth SIG-defined Light HSL model
 * - Inherits from meshXServerModel and meshXClientModel templates
 * - Provides standard Light HSL control operations (hue, saturation, lightness)
 * - Integrates with MeshX transmission control
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#ifndef _MESHX_MODEL_HSL_HPP_
#define _MESHX_MODEL_HSL_HPP_

#include <meshx_model_class.hpp>
#include <meshx_base_model_light.hpp>

#define MESHX_LIGHT_HSL_CLIENT_MODEL_TEMPLATE_PROTO
#define MESHX_LIGHT_HSL_CLIENT_MODEL_TEMPLATE_PARAMS

#define MESHX_LIGHT_HSL_SERVER_MODEL_TEMPLATE_PROTO
#define MESHX_LIGHT_HSL_SERVER_MODEL_TEMPLATE_PARAMS

/**
 * @brief Structure to hold the Light HSL model state.
 */
typedef struct meshx_light_hsl_model_state
{
    uint16_t lightness;        /**< Present lightness value */
    uint16_t hue;              /**< Present hue value */
    uint16_t saturation;       /**< Present saturation value */
    uint16_t target_lightness; /**< Target lightness value */
    uint16_t target_hue;       /**< Target hue value */
    uint16_t target_saturation;/**< Target saturation value */
    uint16_t hue_range_min;    /**< Hue range minimum */
    uint16_t hue_range_max;    /**< Hue range maximum */
    uint16_t sat_range_min;    /**< Saturation range minimum */
    uint16_t sat_range_max;    /**< Saturation range maximum */
}meshx_light_hsl_model_state_t;

/**
 * @brief Structure to hold the parameters for sending a Light HSL message.
 */
struct meshx_light_hsl_send_params
{
    meshx_model_t                *model;  /**< Pointer to the Light HSL client model. */
    meshx_ctx_t                  *ctx;    /**< The context of the message. */
    uint8_t                       tid;    /**< Transaction ID of the message. Only used by Client */
    meshx_light_hsl_model_state_t state;  /**< The state of the message. */
};

using meshx_light_hsl_send_params_t = struct meshx_light_hsl_send_params;

#if CONFIG_ENABLE_LIGHT_HSL_CLIENT

/**
 * @brief Structure to hold the Light HSL Client to element message.
 */
struct meshx_light_hsl_cli_el_msg
{
    meshx_cli_model_send_param_header_t header; /**< Client model send param header */
    meshx_light_hsl_model_state_t       state;  /**< The state of the message. */
};

using meshx_light_hsl_cli_el_msg_t = struct meshx_light_hsl_cli_el_msg;

/**
 * @class meshXLightHSLClientModel
 * @brief A template class for creating Light HSL Client models.
 *
 * This class is derived from meshXClientModel and provides a convenient interface for
 * creating Light HSL Client models. It handles the Light HSL state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 */
MESHX_LIGHT_HSL_CLIENT_MODEL_TEMPLATE_PROTO
class meshXLightHSLClientModel MESHX_LIGHT_HSL_CLIENT_MODEL_TEMPLATE_PARAMS
    : public meshXClientModel<meshXBaseLightClientModel, meshx_light_hsl_send_params_t>
{
private:
    /* New or updated model state from BLE layer */
    meshx_light_hsl_model_state_t model_state;

    meshx_err_t meshx_state_change_notify   (
        const meshx_gen_light_cli_cb_param_t *param,
        uint8_t status
    );

    meshx_err_t element_state_change_handle (void) override;

public:
    meshx_err_t model_send          (meshx_light_hsl_send_params_t *params) override;
    meshx_err_t model_from_ble_cb   (dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;

    meshXLightHSLClientModel(
        meshXElementIF *parent_element = nullptr,
        meshx_ptr_t     parent_element_state = nullptr
    );
    ~meshXLightHSLClientModel() = default;
};

#endif /* CONFIG_ENABLE_LIGHT_HSL_CLIENT */

/********************************************************************************************************************************** */
#if CONFIG_ENABLE_LIGHT_HSL_SERVER

/**
 * @brief Structure to hold the Light HSL Server to element message.
 */
struct meshx_light_hsl_srv_el_msg
{
    meshx_srv_model_send_param_header_t header; /**< Server model send param header */
    meshx_light_hsl_model_state_t       state;  /**< The state of the message. */
};

using meshx_light_hsl_srv_el_msg_t = struct meshx_light_hsl_srv_el_msg;

/**
 * @class meshXLightHSLServerModel
 * @brief A template class for creating Light HSL Server models.
 *
 * This class is derived from meshXServerModel and provides a convenient interface for
 * creating Light HSL Server models. It handles the Light HSL state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 */
MESHX_LIGHT_HSL_SERVER_MODEL_TEMPLATE_PROTO
class meshXLightHSLServerModel MESHX_LIGHT_HSL_SERVER_MODEL_TEMPLATE_PARAMS
    : public meshXServerModel<meshXBaseLightServerModel, meshx_light_hsl_send_params_t>
{
private:
    /* New or updated model state from BLE layer */
    meshx_light_hsl_model_state_t model_state;

    meshx_err_t plat_model_create   (void) override;
    meshx_err_t plat_model_delete   (void) override;
    meshx_err_t element_state_change_handle (void) override;

public:
    meshx_err_t model_send          (meshx_light_hsl_send_params_t *params) override;
    meshx_err_t model_from_ble_cb   (dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;

    meshXLightHSLServerModel(
        meshXElementIF *parent_element = nullptr,
        meshx_ptr_t     parent_element_state = nullptr
    );
    ~meshXLightHSLServerModel() = default;
};

#endif /* CONFIG_ENABLE_LIGHT_HSL_SERVER */

#endif /* _MESHX_MODEL_HSL_HPP_ */
