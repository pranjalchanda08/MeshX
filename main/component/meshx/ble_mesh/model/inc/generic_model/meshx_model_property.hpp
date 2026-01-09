/**
 * @file meshx_model_property.hpp
 * @brief Implementation of Generic Property Models for MeshX
 *
 * This file contains the implementation of the Generic Property models,
 * which provide standard Property model functionality in the MeshX BLE mesh framework.
 *
 * Key Features:
 * - Implements Bluetooth SIG-defined Generic Property models
 * - Supports Manufacturer, Admin, User, and Client Property servers
 * - Inherits from meshXClientModel and meshXServerModel templates
 * - Provides standard Property control operations (GET, SET operations by property ID)
 * - Integrates with MeshX transmission control
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */
#ifndef _MESHX_MODEL_PROPERTY_HPP_
#define _MESHX_MODEL_PROPERTY_HPP_

#include <meshx_model_class.hpp>
#include <meshx_base_model_generic.hpp>

#define MESHX_GEN_PROPERTY_CLIENT_MODEL_TEMPLATE_PROTO
#define MESHX_GEN_PROPERTY_CLIENT_MODEL_TEMPLATE_PARAMS

#define MESHX_GEN_ADMIN_PROPERTY_SERVER_MODEL_TEMPLATE_PROTO
#define MESHX_GEN_ADMIN_PROPERTY_SERVER_MODEL_TEMPLATE_PARAMS

#define MESHX_GEN_MANUFACTURER_PROPERTY_SERVER_MODEL_TEMPLATE_PROTO
#define MESHX_GEN_MANUFACTURER_PROPERTY_SERVER_MODEL_TEMPLATE_PARAMS

#define MESHX_GEN_USER_PROPERTY_SERVER_MODEL_TEMPLATE_PROTO
#define MESHX_GEN_USER_PROPERTY_SERVER_MODEL_TEMPLATE_PARAMS

#define MESHX_GEN_CLIENT_PROPERTY_SERVER_MODEL_TEMPLATE_PROTO
#define MESHX_GEN_CLIENT_PROPERTY_SERVER_MODEL_TEMPLATE_PARAMS

/**
 * @brief Structure to hold the Property model state.
 */
struct meshx_gen_property_model_state
{
    uint16_t property_id;       /**< Property ID */
    meshx_ptr_t property_value; /**< Pointer to the property value data */
    uint8_t access;             /**< Access level performed */
};

using meshx_gen_property_model_state_t = struct meshx_gen_property_model_state;

/**
 * @brief Structure to hold the parameters for sending a Generic Property message.
 */
struct meshx_gen_property_send_params
{
    meshx_model_t                    *model;  /**< Pointer to the Property client model. */
    meshx_ctx_t                      *ctx;    /**< The context of the message. */
    meshx_gen_property_model_state_t   state;  /**< The state of the message. */
};

using meshx_gen_property_send_params_t = struct meshx_gen_property_send_params;

#if CONFIG_ENABLE_GEN_PROPERTY_CLIENT
/**
 * @brief Structure to hold the Property Server to parent element message.
 *        The structure is used by the on_model_cb function to send the Property state
 *        change notification to the parent element.
 */
struct meshx_property_cli_el_msg
{
    meshx_cli_model_send_param_header_t header; /**< Client model send param header */
    meshx_gen_property_model_state_t       state;  /**< The state of the message. */
};

using meshx_property_cli_el_msg_t = struct meshx_property_cli_el_msg;
/**
 * @class meshXGenericPropertyClientModel
 * @brief A template class for creating Generic Property Client models.
 *
 * This class is derived from meshXClientModel and provides a convenient interface for
 * creating Generic Property Client models. It handles the Generic Property state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 */
MESHX_GEN_PROPERTY_CLIENT_MODEL_TEMPLATE_PROTO
class meshXGenericPropertyClientModel : public meshXClientModel<meshXBaseGenericClientModel, meshx_gen_property_send_params_t>
{
private:
    /* New or updated model state from BLE layer */
    meshx_gen_property_model_state_t model_state;

    /* Message to be sent to parent element */
    meshx_property_cli_el_msg_t element_msg;

    meshx_err_t meshx_state_change_notify   (const meshx_gen_cli_cb_param_t *param, uint8_t status);
    meshx_err_t element_state_change_handle (void) override;

public:
    meshx_err_t model_send          (meshx_gen_property_send_params_t *params) override;
    meshx_err_t model_from_ble_cb   (dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;
    meshx_err_t prepare_element_msg (meshx_ptr_t *msg_ptr, size_t *msg_size) override;

    meshXGenericPropertyClientModel(
        meshXElementIF *parent_element = nullptr,
        meshx_ptr_t     parent_element_state = nullptr,
        uint16_t        model_func_id = 0
    );
    ~meshXGenericPropertyClientModel() = default;
};

#endif /* CONFIG_ENABLE_GEN_PROPERTY_CLIENT */

#if CONFIG_ENABLE_GEN_ADMIN_PROPERTY_SERVER

/**
 * @brief Structure to hold the Property Server to parent element message.
 *        The structure is used by the on_model_cb function to send the Property state
 *        change notification to the parent element.
 */
struct meshx_property_srv_el_msg
{
    meshx_srv_model_send_param_header_t header; /**< Server model send param header */
    meshx_gen_property_model_state_t       state;  /**< The state of the message. */
};

using meshx_property_srv_el_msg_t = struct meshx_property_srv_el_msg;

/**
 * @class meshXGenericAdminPropertyServerModel
 * @brief A template class for creating Generic Admin Property Server models.
 *
 * This class is derived from meshXServerModel and provides a convenient interface for
 * creating Generic Admin Property Server models. It handles the Generic Admin Property state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 */
MESHX_GEN_ADMIN_PROPERTY_SERVER_MODEL_TEMPLATE_PROTO
class meshXGenericAdminPropertyServerModel : public meshXServerModel<meshXBaseGenericServerModel, meshx_gen_property_send_params_t>
{
private:
    /* New or updated model state from BLE layer */
    meshx_gen_property_model_state_t model_state;

    /* Message to be sent to parent element */
    meshx_property_srv_el_msg_t element_msg;

    /* Flag to indicate if element message has been prepared */
    bool element_msg_prepared = false;

    meshx_err_t plat_model_create   (void) override;
    meshx_err_t plat_model_delete   (void) override;

public:
    meshx_err_t model_send          (meshx_gen_property_send_params_t *params) override;
    meshx_err_t model_from_ble_cb   (dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;
    meshx_err_t prepare_element_msg (meshx_ptr_t *msg_ptr, size_t *msg_size) override;

    // Virtual method implementation from meshXModelIF
    meshx_err_t element_state_change_handle (void) override;

    meshXGenericAdminPropertyServerModel(
        meshXElementIF *parent_element = nullptr,
        meshx_ptr_t     parent_element_state = nullptr,
        uint16_t        model_func_id = 0
    );
    ~meshXGenericAdminPropertyServerModel() = default;
};
#endif /* CONFIG_ENABLE_GEN_ADMIN_PROPERTY_SERVER */

#if CONFIG_ENABLE_GEN_MANU_PROP_SERVER
/**
 * @class meshXGenericManufacturerPropertyServerModel
 * @brief A template class for creating Generic Manufacturer Property Server models.
 *
 * This class is derived from meshXServerModel and provides a convenient interface for
 * creating Generic Manufacturer Property Server models. It handles the Generic Manufacturer Property state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 */
MESHX_GEN_MANUFACTURER_PROPERTY_SERVER_MODEL_TEMPLATE_PROTO
class meshXGenericManufacturerPropertyServerModel : public meshXServerModel<meshXBaseGenericServerModel, meshx_gen_property_send_params_t>
{
private:
    /* New or updated model state from BLE layer */
    meshx_gen_property_model_state_t model_state;

    /* Message to be sent to parent element */
    meshx_property_srv_el_msg_t element_msg;

    /* Flag to indicate if element message has been prepared */
    bool element_msg_prepared = false;

    meshx_err_t plat_model_create   (void) override;
    meshx_err_t plat_model_delete   (void) override;

public:
    meshx_err_t model_send          (meshx_gen_property_send_params_t *params) override;
    meshx_err_t model_from_ble_cb   (dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;
    meshx_err_t prepare_element_msg (meshx_ptr_t *msg_ptr, size_t *msg_size) override;

    // Virtual method implementation from meshXModelIF
    meshx_err_t element_state_change_handle (void) override;

    meshXGenericManufacturerPropertyServerModel(
        meshXElementIF *parent_element = nullptr,
        meshx_ptr_t     parent_element_state = nullptr,
        uint16_t        model_func_id = 0
    );
    ~meshXGenericManufacturerPropertyServerModel() = default;
};
#endif /* CONFIG_ENABLE_GEN_MANU_PROP_SERVER */

#if CONFIG_ENABLE_GEN_USER_PROPERTY_SERVER
/**
 * @class meshXGenericUserPropertyServerModel
 * @brief A template class for creating Generic User Property Server models.
 *
 * This class is derived from meshXServerModel and provides a convenient interface for
 * creating Generic User Property Server models. It handles the Generic User Property state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 */
MESHX_GEN_USER_PROPERTY_SERVER_MODEL_TEMPLATE_PROTO
class meshXGenericUserPropertyServerModel : public meshXServerModel<meshXBaseGenericServerModel, meshx_gen_property_send_params_t>
{
private:
    /* New or updated model state from BLE layer */
    meshx_gen_property_model_state_t model_state;

    /* Message to be sent to parent element */
    meshx_property_srv_el_msg_t element_msg;

    /* Flag to indicate if element message has been prepared */
    bool element_msg_prepared = false;

    meshx_err_t plat_model_create   (void) override;
    meshx_err_t plat_model_delete   (void) override;

public:
    meshx_err_t model_send          (meshx_gen_property_send_params_t *params) override;
    meshx_err_t model_from_ble_cb   (dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;
    meshx_err_t prepare_element_msg (meshx_ptr_t *msg_ptr, size_t *msg_size) override;

    // Virtual method implementation from meshXModelIF
    meshx_err_t element_state_change_handle (void) override;

    meshXGenericUserPropertyServerModel(
        meshXElementIF *parent_element = nullptr,
        meshx_ptr_t     parent_element_state = nullptr,
        uint16_t        model_func_id = 0
    );
    ~meshXGenericUserPropertyServerModel() = default;
};
#endif /* CONFIG_ENABLE_GEN_USER_PROPERTY_SERVER */

#if CONFIG_ENABLE_GEN_CLIENT_PROPERTY_SERVER
/**
 * @class meshXGenericClientPropertyServerModel
 * @brief A template class for creating Generic Client Property Server models.
 *
 * This class is derived from meshXServerModel and provides a convenient interface for
 * creating Generic Client Property Server models. It handles the Generic Client Property state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 */
MESHX_GEN_CLIENT_PROPERTY_SERVER_MODEL_TEMPLATE_PROTO
class meshXGenericClientPropertyServerModel : public meshXServerModel<meshXBaseGenericServerModel, meshx_gen_property_send_params_t>
{
private:
    /* Message to be sent to parent element */
    meshx_property_srv_el_msg_t element_msg;

    /* Flag to indicate if element message has been prepared */
    bool element_msg_prepared = false;

public:
    meshx_err_t model_send          (meshx_gen_property_send_params_t *params) override;
    meshx_err_t model_from_ble_cb   (dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;
    meshx_err_t prepare_element_msg (meshx_ptr_t *msg_ptr, size_t *msg_size) override;

    meshXGenericClientPropertyServerModel(
        meshXElementIF *parent_element = nullptr,
        meshx_ptr_t     parent_element_state = nullptr,
        uint16_t        model_func_id = 0
    );
    ~meshXGenericClientPropertyServerModel() = default;
};
#endif /* CONFIG_ENABLE_GEN_CLIENT_PROPERTY_SERVER */

#endif /* _MESHX_MODEL_PROPERTY_HPP_ */
