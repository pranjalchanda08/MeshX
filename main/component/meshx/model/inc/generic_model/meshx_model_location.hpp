/**
 * @file meshx_model_location.hpp
 * @brief Implementation of Generic Location Model for MeshX
 *
 * This file contains the implementation of the Generic Location model,
 * which provides standard Location model functionality in the MeshX BLE mesh framework.
 *
 * Key Features:
 * - Implements Bluetooth SIG-defined Generic Location model
 * - Inherits from meshXClientModel, meshXServerModel templates
 * - Provides standard Location control operations (Global/Local coordinates)
 * - Integrates with MeshX transmission control
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */
#include <meshx_model_class.hpp>
#include <meshx_base_model_generic.hpp>

#define MESHX_GEN_LOCATION_CLIENT_MODEL_TEMPLATE_PROTO
#define MESHX_GEN_LOCATION_CLIENT_MODEL_TEMPLATE_PARAMS

#define MESHX_GEN_LOCATION_SERVER_MODEL_TEMPLATE_PROTO
#define MESHX_GEN_LOCATION_SERVER_MODEL_TEMPLATE_PARAMS

#define MESHX_GEN_LOCATION_SETUP_SERVER_MODEL_TEMPLATE_PROTO
#define MESHX_GEN_LOCATION_SETUP_SERVER_MODEL_TEMPLATE_PARAMS

/**
 * @brief Structure to hold the parameters for sending a Generic Location message.
 */
struct meshx_gen_location_send_params
{
    meshx_model_t *model; /**< Pointer to the Location client model. */
    meshx_ctx_t *ctx;     /**< The context of the message. */
    // Global location parameters
    int32_t global_latitude;  /**< Global latitude (-90 to 90 degrees). */
    int32_t global_longitude; /**< Global longitude (-180 to 180 degrees). */
    int16_t global_altitude;  /**< Global altitude (in meters). */
    // Local location parameters
    int16_t local_north;    /**< Local North coordinate. */
    int16_t local_east;     /**< Local East coordinate. */
    int16_t local_altitude; /**< Local altitude. */
    uint8_t floor_number;   /**< Floor number. */
    uint16_t uncertainty;   /**< Uncertainty of the location. */
    uint8_t tid;            /**< The transaction ID of the message. Only used by Client*/
};

using meshx_gen_location_send_params_t = struct meshx_gen_location_send_params;

#if CONFIG_ENABLE_GEN_LOCATION_CLIENT
/**
 * @brief Structure to hold the Location Server to parent element message.
 *        The structure is used by the on_model_cb function to send the Location state
 *        change notification to the parent element.
 */
struct meshx_location_cli_el_msg
{
    uint8_t err_code;    /**< Error code */
    meshx_model_t model; /**< Generic Location Server model */
    meshx_ctx_t ctx;     /**< Context of the message */
    // Global location data
    int32_t global_latitude;  /**< Global latitude (-90 to 90 degrees) */
    int32_t global_longitude; /**< Global longitude (-180 to 180 degrees) */
    int16_t global_altitude;  /**< Global altitude (in meters) */
    // Local location data
    int16_t local_north;    /**< Local North coordinate */
    int16_t local_east;     /**< Local East coordinate */
    int16_t local_altitude; /**< Local altitude */
    uint8_t floor_number;   /**< Floor number */
    uint16_t uncertainty;   /**< Uncertainty of the location */
};

using meshx_location_cli_el_msg_t = struct meshx_location_cli_el_msg;
/**
 * @class meshXGenericLocationClientModel
 * @brief A template class for creating Generic Location Client models.
 *
 * This class is derived from meshXClientModel and provides a convenient interface for
 * creating Generic Location Client models. It handles the Generic Location state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 */
MESHX_GEN_LOCATION_CLIENT_MODEL_TEMPLATE_PROTO
class meshXGenericLocationClientModel : public meshXClientModel<meshXBaseGenericClientModel, meshx_gen_location_send_params_t>
{
private:
    meshx_err_t meshx_state_change_notify(const meshx_gen_cli_cb_param_t *param, uint8_t status) const;

public:
    meshx_err_t model_send(meshx_gen_location_send_params_t *params) override;
    meshx_err_t model_from_ble_cb(dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;

    meshXGenericLocationClientModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element = nullptr);
    ~meshXGenericLocationClientModel() = default;
};

#endif /* CONFIG_ENABLE_GEN_LOCATION_CLIENT */

#if CONFIG_ENABLE_GEN_LOCATION_SERVER
/**
 * @brief Structure to hold the Location Server to parent element message.
 *        The structure is used by the on_model_cb function to send the Location state
 *        change notification to the parent element.
 */
struct meshx_location_srv_el_msg
{
    meshx_model_t *model; /**< Generic Location Server model */
    struct
    {
        int32_t latitude;  /**< Global latitude (-90 to 90 degrees) */
        int32_t longitude; /**< Global longitude (-180 to 180 degrees) */
        int16_t altitude;  /**< Global altitude (in meters) */
    } global;              /**< Global location parameters */
    struct
    {
        int16_t north;        /**< Local North coordinate */
        int16_t east;         /**< Local East coordinate */
        int16_t altitude;     /**< Local altitude */
        uint8_t floor_number; /**< Floor number */
        uint16_t uncertainty; /**< Uncertainty of the location */
    } local;                  /**< Local location parameters */
};

using meshx_location_srv_el_msg_t = struct meshx_location_srv_el_msg;

/**
 * @class meshXGenericLocationServerModel
 * @brief A template class for creating Generic Location Server models.
 *
 * This class is derived from meshXServerModel and provides a convenient interface for
 * creating Generic Location Server models. It handles the Generic Location state change
 * notifications from the MeshX stack and publishes the state change event to the
 * element layer.
 */
MESHX_GEN_LOCATION_SERVER_MODEL_TEMPLATE_PROTO
class meshXGenericLocationServerModel : public meshXServerModel<meshXBaseGenericServerModel, meshx_gen_location_send_params_t>
{
public:
    meshx_err_t model_send(meshx_gen_location_send_params_t *params) override;
    meshx_err_t model_from_ble_cb(dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;

    meshXGenericLocationServerModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element = nullptr);
    ~meshXGenericLocationServerModel() = default;
};
#endif /* CONFIG_ENABLE_GEN_LOCATION_SERVER */

#if CONFIG_ENABLE_GEN_LOCATION_SETUP_SERVER
/**
 * @class meshXGenericLocationSetupServerModel
 * @brief A template class for creating Generic Location Setup Server models.
 *
 * This class is derived from meshXServerModel and provides a convenient interface for
 * creating Generic Location Setup Server models. It handles the Generic Location setup
 * operations from the MeshX stack.
 */
MESHX_GEN_LOCATION_SETUP_SERVER_MODEL_TEMPLATE_PROTO
class meshXGenericLocationSetupServerModel : public meshXServerModel<meshXBaseGenericServerModel, meshx_gen_location_send_params_t>
{
public:
    meshx_err_t model_send(meshx_gen_location_send_params_t *params) override;
    meshx_err_t model_from_ble_cb(dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;

    meshXGenericLocationSetupServerModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element = nullptr);
    ~meshXGenericLocationSetupServerModel() = default;
};
#endif /* CONFIG_ENABLE_GEN_LOCATION_SETUP_SERVER */
