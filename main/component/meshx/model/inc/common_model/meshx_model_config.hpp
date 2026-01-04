/**
 * @file meshx_model_config.hpp
 * @brief MeshX Model Configuration Header File
 * This file contains configuration settings and macros for MeshX models.
 * It defines various model-related parameters and options used throughout the MeshX BLE mesh framework.
 *
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#ifndef _MESHX_MODEL_CONFIG_HPP__
#define _MESHX_MODEL_CONFIG_HPP_

#include <meshx_fwd_decl.hpp>
#include <meshx_model_class.hpp>
#include <meshx_base_model_common.hpp>

/*********************************************************************************
 * Model Configuration Macros
 *********************************************************************************/

#if CONFIG_ENABLE_CONFIG_SERVER

#define MESHX_CONFIG_SERVER_MODEL_TEMPLATE_PROTO
#define MESHX_CONFIG_SERVER_MODEL_TEMPLATE_PARAMS

/**
 * @brief Structure to hold the parameters for sending a Generic OnOff message.
 */
struct meshx_config_send_params
{
    meshx_model_t *model; /**< Pointer to the On/Off client model. */
    meshx_ctx_t *ctx;     /**< The context of the message. */
    uint8_t stub;         /**< No Params to send hence placing a stub */
};

using meshx_config_send_params_t = struct meshx_config_send_params;

/**
 * @brief Structure to hold config server element message parameters.
 */
struct meshx_config_srv_el_msg
{
    meshx_model_t model;           /**< Config server model information. */
    meshx_ctx_t ctx;              /**< Message context. */
    meshx_cfg_srv_state_change_t state_change; /**< State change information. */
};

using meshx_config_srv_el_msg_t = struct meshx_config_srv_el_msg;

MESHX_CONFIG_SERVER_MODEL_TEMPLATE_PROTO
class meshXConfigModel : public meshXServerModel<meshXBaseConfigServerModel, meshx_config_send_params_t>
{
private:
    meshx_err_t plat_model_create(void) override;
    meshx_err_t plat_model_delete(void) override;
    meshx_err_t element_state_change_handle(void) override
    {
        /* Nothing to do here */
        return MESHX_SUCCESS;
    };

public:
    meshx_err_t model_send(meshx_config_send_params_t *params) override;
    meshx_err_t model_from_ble_cb(dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;

    meshXConfigModel(meshXElementIF *parent_element = nullptr);
    ~meshXConfigModel() override = default;
    // Configuration model related members and methods would be defined here.
};

#endif /* CONFIG_ENABLE_CONFIG_SERVER */

#endif /* _MESHX_MODEL_CONFIG_HPP__ */
