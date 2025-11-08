/**
 * @file meshx_model_ctl.hpp
 *
 * @brief Header file for MeshX Light CTL Models
 * This file contains the declarations and definitions for the MeshX Light CTL Models,
 * including the Light CTL Server and Client models for color temperature and lightness control.
 *
 * Key Features:
 * - Implements Bluetooth SIG-defined Light CTL model
 * - Inherits from meshXServerModel and meshXClientModel templates
 * - Provides standard Light CTL control operations (lightness, temperature, delta UV)
 * - Integrates with MeshX transmission control
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#ifndef _MESHX_MODEL_CTL_HPP_
#define _MESHX_MODEL_CTL_HPP_

#include <meshx_model_class.hpp>
#include <meshx_base_model_light.hpp>

#define MESHX_LIGHT_CTL_CLIENT_MODEL_TEMPLATE_PROTO
#define MESHX_LIGHT_CTL_CLIENT_MODEL_TEMPLATE_PARAMS

#define MESHX_LIGHT_CTL_SERVER_MODEL_TEMPLATE_PROTO
#define MESHX_LIGHT_CTL_SERVER_MODEL_TEMPLATE_PARAMS

/**
 * @brief Structure to hold the parameters for sending a Light CTL message.
 */
struct meshx_light_ctl_send_params
{
    meshx_model_t *model;      /**< Pointer to the Light CTL client model. */
    meshx_ctx_t *ctx;          /**< The context of the message. */
    uint16_t lightness;        /**< Lightness value (0-65535) */
    uint16_t temperature;      /**< Color temperature value (800-20000) */
    int16_t delta_uv;          /**< Delta UV value (-32768-32767) */
    uint8_t tid;               /**< Transaction ID of the message. Only used by Client */
};

using meshx_light_ctl_send_params_t = struct meshx_light_ctl_send_params;

#if CONFIG_ENABLE_LIGHT_CTL_CLIENT

/**
 * @brief Structure to hold the Light CTL Client to element message.
 */
struct meshx_light_ctl_cli_el_msg
{
    int err_code;              /**< Error code */
    meshx_model_t model;       /**< Light CTL Client model */
    meshx_ctx_t ctx;           /**< Context */
    uint16_t lightness;        /**< Present lightness value */
    uint16_t temperature;      /**< Present color temperature value */
    int16_t delta_uv;          /**< Present delta UV value */
    uint16_t temp_range_min;   /**< Minimum temperature range */
    uint16_t temp_range_max;   /**< Maximum temperature range */
};

using meshx_light_ctl_cli_el_msg_t = struct meshx_light_ctl_cli_el_msg;

/**
 * @class meshXLightCTLClientModel
 * @brief A template class for creating Light CTL Client models.
 *
 * This class is derived from meshXClientModel and provides a convenient interface for
 * creating Light CTL Client models. It handles the Light CTL state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 */
MESHX_LIGHT_CTL_CLIENT_MODEL_TEMPLATE_PROTO
class meshXLightCTLClientModel MESHX_LIGHT_CTL_CLIENT_MODEL_TEMPLATE_PARAMS
    : public meshXClientModel<meshXBaseLightClientModel, meshx_light_ctl_send_params_t>
{
private:
    meshx_err_t meshx_state_change_notify(const meshx_gen_light_cli_cb_param_t *param, uint8_t status) const;

public:
    meshx_err_t model_send(meshx_light_ctl_send_params_t *params) override;
    meshx_err_t model_from_ble_cb(dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;

    meshXLightCTLClientModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element = nullptr);
    ~meshXLightCTLClientModel() = default;
};

#endif /* CONFIG_ENABLE_LIGHT_CTL_CLIENT */

/********************************************************************************************************************************** */
#if CONFIG_ENABLE_LIGHT_CTL_SERVER

/**
 * @brief Structure to hold the Light CTL Server to element message.
 */
struct meshx_light_ctl_srv_el_msg
{
    meshx_model_t model;       /**< Light CTL Server model */
    uint16_t lightness;        /**< Present lightness value */
    uint16_t temperature;      /**< Present color temperature value */
    int16_t delta_uv;          /**< Present delta UV value */
};

using meshx_light_ctl_srv_el_msg_t = struct meshx_light_ctl_srv_el_msg;

/**
 * @class meshXLightCTLServerModel
 * @brief A template class for creating Light CTL Server models.
 *
 * This class is derived from meshXServerModel and provides a convenient interface for
 * creating Light CTL Server models. It handles the Light CTL state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 */
MESHX_LIGHT_CTL_SERVER_MODEL_TEMPLATE_PROTO
class meshXLightCTLServerModel MESHX_LIGHT_CTL_SERVER_MODEL_TEMPLATE_PARAMS
    : public meshXServerModel<meshXBaseLightServerModel, meshx_light_ctl_send_params_t>
{
private:
    meshx_err_t plat_model_create(void) override;
    meshx_err_t plat_model_delete(void) override;

public:
    meshx_err_t model_send(meshx_light_ctl_send_params_t *params) override;
    meshx_err_t model_from_ble_cb(dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;

    meshXLightCTLServerModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element = nullptr);
    ~meshXLightCTLServerModel() = default;
};

#endif /* CONFIG_ENABLE_LIGHT_CTL_SERVER */

#endif /* _MESHX_MODEL_CTL_HPP_ */
