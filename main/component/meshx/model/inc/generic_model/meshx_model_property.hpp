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
 * @brief Structure to hold the parameters for sending a Generic Property message.
 */
struct meshx_gen_property_send_params
{
    meshx_model_t *model;    /**< Pointer to the Property client model. */
    meshx_ctx_t *ctx;        /**< The context of the message. */
    uint16_t property_id;    /**< The property ID to access. */
    uint8_t *property_value; /**< Pointer to the property value data. */
    uint8_t access;          /**< Access level performed. */
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
    uint8_t err_code;        /**< Error code */
    meshx_model_t model;     /**< Generic Property Server model */
    meshx_ctx_t ctx;         /**< Context of the message */
    uint16_t property_id;    /**< The property ID that was accessed */
    uint8_t *property_value; /**< Pointer to the property value data */
    uint8_t access;          /**< Access level performed */
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
    meshx_err_t meshx_state_change_notify(const meshx_gen_cli_cb_param_t *param, uint8_t status) const;

public:
    meshx_err_t model_send(meshx_gen_property_send_params_t *params) override;
    meshx_err_t model_from_ble_cb(dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;

    meshXGenericPropertyClientModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element = nullptr);
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
    meshx_model_t *model;       /**< Generic Property Server model */
    uint16_t property_id;       /**< Property ID */
    meshx_ptr_t property_value; /**< Pointer to the property value data */
    uint8_t access;             /**< Access level performed */
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
    meshx_err_t plat_model_create(void) override;
    meshx_err_t plat_model_delete(void) override;
public:
    meshx_err_t model_send(meshx_gen_property_send_params_t *params) override;
    meshx_err_t model_from_ble_cb(dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;

    meshXGenericAdminPropertyServerModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element = nullptr);
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
    meshx_err_t plat_model_create(void) override;
    meshx_err_t plat_model_delete(void) override;
public:
    meshx_err_t model_send(meshx_gen_property_send_params_t *params) override;
    meshx_err_t model_from_ble_cb(dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;

    meshXGenericManufacturerPropertyServerModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element = nullptr);
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
    meshx_err_t plat_model_create(void) override;
    meshx_err_t plat_model_delete(void) override;
public:
    meshx_err_t model_send(meshx_gen_property_send_params_t *params) override;
    meshx_err_t model_from_ble_cb(dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;

    meshXGenericUserPropertyServerModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element = nullptr);
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
public:
    meshx_err_t model_send(meshx_gen_property_send_params_t *params) override;
    meshx_err_t model_from_ble_cb(dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;

    meshXGenericClientPropertyServerModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element = nullptr);
    ~meshXGenericClientPropertyServerModel() = default;
};
#endif /* CONFIG_ENABLE_GEN_CLIENT_PROPERTY_SERVER */
