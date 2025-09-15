/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file meshx_ble_mesh_gen_cli.h
 * @brief Header file for BLE Mesh Generic Client functionality in the MeshX framework.
 *        This file contains declarations and includes required for implementing
 *        BLE Mesh Generic Client operations.
 *
 * @author Pranjal Chanda
 */

#ifndef __MESHX_BLE_MESH_GEN_CLI_H__
#define __MESHX_BLE_MESH_GEN_CLI_H__

#include "../meshx_ble_mesh_cmn.h"
#include "meshx_control_task.h"

typedef enum
{
    MESHX_GEN_CLI_EVT_GET = MESHX_BIT(0),
    MESHX_GEN_CLI_EVT_SET = MESHX_BIT(1),
    MESHX_GEN_CLI_PUBLISH = MESHX_BIT(2),
    MESHX_GEN_CLI_TIMEOUT = MESHX_BIT(3),
    MESHX_GEN_CLI_EVT_ALL = (       \
            MESHX_GEN_CLI_EVT_GET | \
            MESHX_GEN_CLI_EVT_SET | \
            MESHX_GEN_CLI_PUBLISH | \
            MESHX_GEN_CLI_TIMEOUT)
} meshx_gen_cli_evt_t;

/** Parameters of Generic OnOff Set. */
typedef struct {
    bool    op_en;      /*!< Indicate if optional parameters are included */
    uint8_t onoff;      /*!< Target value of Generic OnOff state */
    uint8_t tid;        /*!< Transaction ID */
    uint8_t trans_time; /*!< Time to complete state transition (optional) */
    uint8_t delay;      /*!< Indicate message execution delay (C.1) */
} meshx_gen_onoff_set_t;

/** Parameters of Generic Level Set. */
typedef struct {
    bool    op_en;      /*!< Indicate if optional parameters are included */
    int16_t level;      /*!< Target value of Generic Level state */
    uint8_t tid;        /*!< Transaction ID */
    uint8_t trans_time; /*!< Time to complete state transition (optional) */
    uint8_t delay;      /*!< Indicate message execution delay (C.1) */
} meshx_gen_level_set_t;

/** Parameters of Generic Delta Set. */
typedef struct {
    bool    op_en;      /*!< Indicate if optional parameters are included */
    int32_t level;      /*!< Delta change of Generic Level state */
    uint8_t tid;        /*!< Transaction ID */
    uint8_t trans_time; /*!< Time to complete state transition (optional) */
    uint8_t delay;      /*!< Indicate message execution delay (C.1) */
} meshx_gen_delta_set_t;

/** Parameters of Generic Move Set. */
typedef struct {
    bool    op_en;          /*!< Indicate if optional parameters are included */
    int16_t delta_level;    /*!< Delta Level step to calculate Move speed for Generic Level state */
    uint8_t tid;            /*!< Transaction ID */
    uint8_t trans_time;     /*!< Time to complete state transition (optional) */
    uint8_t delay;          /*!< Indicate message execution delay (C.1) */
} meshx_gen_move_set_t;

/** Parameter of Generic Default Transition Time Set. */
typedef struct {
    uint8_t trans_time; /*!< The value of the Generic Default Transition Time state */
} meshx_gen_def_trans_time_set_t;

/** Parameter of Generic OnPowerUp Set. */
typedef struct {
    uint8_t onpowerup;  /*!< The value of the Generic OnPowerUp state */
} meshx_gen_onpowerup_set_t;

/** Parameters of Generic Power Level Set. */
typedef struct {
    bool     op_en;         /*!< Indicate if optional parameters are included */
    uint16_t power;         /*!< Target value of Generic Power Actual state */
    uint8_t  tid;           /*!< Transaction ID */
    uint8_t  trans_time;    /*!< Time to complete state transition (optional) */
    uint8_t  delay;         /*!< Indicate message execution delay (C.1) */
} meshx_gen_power_level_set_t;

/** Parameter of Generic Power Default Set. */
typedef struct {
    uint16_t power;         /*!< The value of the Generic Power Default state */
} meshx_gen_power_default_set_t;

/** Parameters of Generic Power Range Set. */
typedef struct {
    uint16_t range_min;     /*!< Value of Range Min field of Generic Power Range state */
    uint16_t range_max;     /*!< Value of Range Max field of Generic Power Range state */
} meshx_gen_power_range_set_t;

/** Parameters of Generic Location Global Set. */
typedef struct {
    int32_t global_latitude;    /*!< Global Coordinates (Latitude) */
    int32_t global_longitude;   /*!< Global Coordinates (Longitude) */
    int16_t global_altitude;    /*!< Global Altitude */
} meshx_gen_loc_global_set_t;

/** Parameters of Generic Location Local Set. */
typedef struct {
    int16_t  local_north;       /*!< Local Coordinates (North) */
    int16_t  local_east;        /*!< Local Coordinates (East) */
    int16_t  local_altitude;    /*!< Local Altitude */
    uint8_t  floor_number;      /*!< Floor Number */
    uint16_t uncertainty;       /*!< Uncertainty */
} meshx_gen_loc_local_set_t;

/**
 * @brief Generic Client Model set message union
 */
typedef union {
    meshx_gen_onoff_set_t          onoff_set;            /*!< For MESHX_MODEL_OP_GEN_ONOFF_SET & MESHX_MODEL_OP_GEN_ONOFF_SET_UNACK */
    meshx_gen_level_set_t          level_set;            /*!< For MESHX_MODEL_OP_GEN_LEVEL_SET & MESHX_MODEL_OP_GEN_LEVEL_SET_UNACK */
    meshx_gen_delta_set_t          delta_set;            /*!< For MESHX_MODEL_OP_GEN_DELTA_SET & MESHX_MODEL_OP_GEN_DELTA_SET_UNACK */
    meshx_gen_move_set_t           move_set;             /*!< For MESHX_MODEL_OP_GEN_MOVE_SET & MESHX_MODEL_OP_GEN_MOVE_SET_UNACK */
    meshx_gen_def_trans_time_set_t def_trans_time_set;   /*!< For MESHX_MODEL_OP_GEN_DEF_TRANS_TIME_SET & MESHX_MODEL_OP_GEN_DEF_TRANS_TIME_SET_UNACK */
    meshx_gen_onpowerup_set_t      power_set;            /*!< For MESHX_MODEL_OP_GEN_ONPOWERUP_SET & MESHX_MODEL_OP_GEN_ONPOWERUP_SET_UNACK */
    meshx_gen_power_level_set_t    power_level_set;      /*!< For MESHX_MODEL_OP_GEN_POWER_LEVEL_SET & MESHX_MODEL_OP_GEN_POWER_LEVEL_SET_UNACK */
    meshx_gen_power_default_set_t  power_default_set;    /*!< For MESHX_MODEL_OP_GEN_POWER_DEFAULT_SET & MESHX_MODEL_OP_GEN_POWER_DEFAULT_SET_UNACK */
    meshx_gen_power_range_set_t    power_range_set;      /*!< For MESHX_MODEL_OP_GEN_POWER_RANGE_SET & MESHX_MODEL_OP_GEN_POWER_RANGE_SET_UNACK */
    meshx_gen_loc_global_set_t     loc_global_set;       /*!< For MESHX_MODEL_OP_GEN_LOC_GLOBAL_SET & MESHX_MODEL_OP_GEN_LOC_GLOBAL_SET_UNACK */
    meshx_gen_loc_local_set_t      loc_local_set;        /*!< For MESHX_MODEL_OP_GEN_LOC_LOCAL_SET & MESHX_MODEL_OP_GEN_LOC_LOCAL_SET_UNACK */
} meshx_gen_cli_set_t;

/** Parameters of Generic OnOff Status. */
typedef struct {
    bool    op_en;          /*!< Indicate if optional parameters are included */
    uint8_t present_onoff;  /*!< Current value of Generic OnOff state */
    uint8_t target_onoff;   /*!< Target value of Generic OnOff state (optional) */
    uint8_t remain_time;    /*!< Time to complete state transition (C.1) */
} meshx_gen_onoff_status_cb_t;

/** Parameters of Generic Level Status. */
typedef struct {
    bool    op_en;          /*!< Indicate if optional parameters are included */
    int16_t present_level;  /*!< Current value of Generic Level state */
    int16_t target_level;   /*!< Target value of the Generic Level state (optional) */
    uint8_t remain_time;    /*!< Time to complete state transition (C.1) */
} meshx_gen_level_status_cb_t;

/** Parameter of Generic Default Transition Time Status. */
typedef struct {
    uint8_t trans_time;     /*!< The value of the Generic Default Transition Time state */
} meshx_gen_def_trans_time_status_cb_t;

/** Parameter of Generic OnPowerUp Status. */
typedef struct {
    uint8_t onpowerup;      /*!< The value of the Generic OnPowerUp state */
} meshx_gen_onpowerup_status_cb_t;

/** Parameters of Generic Power Level Status. */
typedef struct {
    bool     op_en;         /*!< Indicate if optional parameters are included */
    uint16_t present_power; /*!< Current value of Generic Power Actual state */
    uint16_t target_power;  /*!< Target value of Generic Power Actual state (optional) */
    uint8_t  remain_time;   /*!< Time to complete state transition (C.1) */
} meshx_gen_power_level_status_cb_t;

/** Parameter of Generic Power Last Status. */
typedef struct {
    uint16_t power;         /*!< The value of the Generic Power Last state */
} meshx_gen_power_last_status_cb_t;

/** Parameter of Generic Power Default Status. */
typedef struct {
    uint16_t power;         /*!< The value of the Generic Default Last state */
} meshx_gen_power_default_status_cb_t;

/** Parameters of Generic Power Range Status. */
typedef struct {
    uint8_t  status_code;   /*!< Status Code for the request message */
    uint16_t range_min;     /*!< Value of Range Min field of Generic Power Range state */
    uint16_t range_max;     /*!< Value of Range Max field of Generic Power Range state */
} meshx_gen_power_range_status_cb_t;

/** Parameters of Generic Battery Status. */
typedef struct {
    uint32_t battery_level     : 8;  /*!< Value of Generic Battery Level state */
    uint32_t time_to_discharge : 24; /*!< Value of Generic Battery Time to Discharge state */
    uint32_t time_to_charge    : 24; /*!< Value of Generic Battery Time to Charge state */
    uint32_t flags             : 8;  /*!< Value of Generic Battery Flags state */
} meshx_gen_battery_status_cb_t;

/** Parameters of Generic Location Global Status. */
typedef struct {
    int32_t global_latitude;  /*!< Global Coordinates (Latitude) */
    int32_t global_longitude; /*!< Global Coordinates (Longitude) */
    int16_t global_altitude;  /*!< Global Altitude */
} meshx_gen_loc_global_status_cb_t;

/** Parameters of Generic Location Local Status. */
typedef struct {
    int16_t  local_north;     /*!< Local Coordinates (North) */
    int16_t  local_east;      /*!< Local Coordinates (East) */
    int16_t  local_altitude;  /*!< Local Altitude */
    uint8_t  floor_number;    /*!< Floor Number */
    uint16_t uncertainty;     /*!< Uncertainty */
} meshx_gen_loc_local_status_cb_t;

/** Parameter of Generic User Properties Status. */
typedef struct {
    struct net_buf_simple *property_ids;    /*!< Buffer contains a sequence of N User Property IDs */
} meshx_gen_user_properties_status_cb_t;

/** Parameters of Generic User Property Status. */
typedef struct {
    bool     op_en;         /*!< Indicate if optional parameters are included */
    uint16_t property_id;   /*!< Property ID identifying a Generic User Property */
    uint8_t  user_access;   /*!< Enumeration indicating user access (optional) */
    struct net_buf_simple *property_value;  /*!< Raw value for the User Property (C.1) */
} meshx_gen_user_property_status_cb_t;

/** Parameter of Generic Admin Properties Status. */
typedef struct {
    struct net_buf_simple *property_ids; /*!< Buffer contains a sequence of N Admin Property IDs */
} meshx_gen_admin_properties_status_cb_t;

/** Parameters of Generic Admin Property Status. */
typedef struct {
    bool     op_en;         /*!< Indicate if optional parameters are included */
    uint16_t property_id;   /*!< Property ID identifying a Generic Admin Property */
    uint8_t  user_access;   /*!< Enumeration indicating user access (optional) */
    struct net_buf_simple *property_value;  /*!< Raw value for the Admin Property (C.1) */
} meshx_gen_admin_property_status_cb_t;

/** Parameter of Generic Manufacturer Properties Status. */
typedef struct {
    struct net_buf_simple *property_ids;    /*!< Buffer contains a sequence of N Manufacturer Property IDs */
} meshx_gen_manufacturer_properties_status_cb_t;

/** Parameters of Generic Manufacturer Property Status. */
typedef struct {
    bool     op_en;         /*!< Indicate if optional parameters are included */
    uint16_t property_id;   /*!< Property ID identifying a Generic Manufacturer Property */
    uint8_t  user_access;   /*!< Enumeration indicating user access (optional) */
    struct net_buf_simple *property_value;  /*!< Raw value for the Manufacturer Property (C.1) */
} meshx_gen_manufacturer_property_status_cb_t;

/** Parameter of Generic Client Properties Status. */
typedef struct {
    struct net_buf_simple *property_ids;    /*!< Buffer contains a sequence of N Client Property IDs */
} meshx_gen_client_properties_status_cb_t;

/**
 * @brief Generic Client Model received message union
 */
typedef union {
    meshx_gen_onoff_status_cb_t             onoff_status;            /*!< For MESHX_MODEL_OP_GEN_ONOFF_STATUS */
    meshx_gen_level_status_cb_t             level_status;            /*!< For MESHX_MODEL_OP_GEN_LEVEL_STATUS */
    meshx_gen_def_trans_time_status_cb_t    def_trans_time_status;   /*!< For MESHX_MODEL_OP_GEN_DEF_TRANS_TIME_STATUS */
    meshx_gen_onpowerup_status_cb_t         onpowerup_status;        /*!< For MESHX_MODEL_OP_GEN_ONPOWERUP_STATUS */
    meshx_gen_power_level_status_cb_t       power_level_status;      /*!< For MESHX_MODEL_OP_GEN_POWER_LEVEL_STATUS */
    meshx_gen_power_last_status_cb_t        power_last_status;       /*!< For MESHX_MODEL_OP_GEN_POWER_LAST_STATUS */
    meshx_gen_power_default_status_cb_t     power_default_status;    /*!< For MESHX_MODEL_OP_GEN_POWER_DEFAULT_STATUS */
    meshx_gen_power_range_status_cb_t       power_range_status;      /*!< For MESHX_MODEL_OP_GEN_POWER_RANGE_STATUS */
    meshx_gen_battery_status_cb_t           battery_status;          /*!< For MESHX_MODEL_OP_GEN_BATTERY_STATUS */
    meshx_gen_loc_global_status_cb_t        location_global_status;  /*!< For MESHX_MODEL_OP_GEN_LOC_GLOBAL_STATUS */
    meshx_gen_loc_local_status_cb_t         location_local_status;   /*!< MESHX_MODEL_OP_GEN_LOC_LOCAL_STATUS */
    meshx_gen_user_properties_status_cb_t   user_properties_status;  /*!< MESHX_MODEL_OP_GEN_USER_PROPERTIES_STATUS */
    meshx_gen_user_property_status_cb_t     user_property_status;    /*!< MESHX_MODEL_OP_GEN_USER_PROPERTY_STATUS */
    meshx_gen_admin_properties_status_cb_t  admin_properties_status; /*!< MESHX_MODEL_OP_GEN_ADMIN_PROPERTIES_STATUS */
    meshx_gen_admin_property_status_cb_t    admin_property_status;   /*!< MESHX_MODEL_OP_GEN_ADMIN_PROPERTY_STATUS */
    meshx_gen_manufacturer_properties_status_cb_t manufacturer_properties_status; /*!< MESHX_MODEL_OP_GEN_MANUFACTURER_PROPERTIES_STATUS */
    meshx_gen_manufacturer_property_status_cb_t   manufacturer_property_status;   /*!< MESHX_MODEL_OP_GEN_MANUFACTURER_PROPERTY_STATUS */
    meshx_gen_client_properties_status_cb_t       client_properties_status;       /*!< MESHX_MODEL_OP_GEN_CLIENT_PROPERTIES_STATUS */
} meshx_gen_client_status_cb_t;

/**
 * @brief Callback parameters for Generic Client Model events.
 *        This structure is used to pass information about the received
 *        messages and their context to the application.
 *        It includes the context, model pointer, event type, and status values.
 *        The status values are encapsulated in a union to handle different
 *        types of status messages that the Generic Client Model can receive.
 *        Each status type corresponds to a specific operation code (opcode) defined in the BLE Mesh
 *        specification, allowing the application to handle them appropriately.
 */
typedef struct meshx_gen_cli_cb_param
{
    int err_code;                               /*!< Error code */
    meshx_ctx_t ctx;                            /**< Context of the received messages */
    meshx_model_t model;                        /**< Pointer to Generic Client Models */
    meshx_gen_cli_evt_t evt;                    /**< Event type of the received message */
    meshx_gen_client_status_cb_t status;        /**< Value of the received Generic Messages */
} meshx_gen_cli_cb_param_t;

typedef control_task_msg_handle_t meshx_gen_client_cb_t;

/**
 * @brief Creates a Generic OnOff client model and its publication context.
 *
 * This function initializes the Generic OnOff client model, its publication
 * context, and allocates memory for the client instance. It checks for
 * invalid arguments and handles memory allocation failures.
 *
 * @param[out] p_model Pointer to the model structure to be created.
 * @param[out] p_pub Pointer to the publication context to be created.
 * @param[out] p_onoff_cli Pointer to the OnOff client instance to be allocated.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully created the model and publication context.
 *     - MESHX_INVALID_ARG: One or more arguments are invalid.
 *     - MESHX_NO_MEM: Memory allocation failed.
 */
meshx_err_t meshx_plat_on_off_gen_cli_create(meshx_ptr_t p_model, meshx_ptr_t* p_pub, meshx_ptr_t* p_onoff_cli);

/**
 * @brief Deletes the Generic OnOff Client model and its associated resources.
 *
 * This function frees the memory allocated for the Generic OnOff Client
 * and sets the pointer to NULL. It also deletes the model publication
 * resources associated with the client.
 *
 * @param[in,out] p_pub Pointer to the publication structure to be deleted.
 * @param[in,out] p_cli Pointer to the OnOff Client structure to be freed.
 *
 * @return
 *     - MESHX_SUCCESS: Model and publication deleted successfully.
 *     - MESHX_FAIL: Failed to delete the model or publication.
 */
meshx_err_t meshx_plat_gen_cli_delete(meshx_ptr_t* p_pub, meshx_ptr_t* p_cli);

/**
 * @brief Initialize the meshxuction generic client.
 *
 * This function sets up the necessary configurations and initializes the
 * meshxuction generic client for the BLE mesh node.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_FAIL: Failed to initialize the client
 */
meshx_err_t meshx_plat_gen_cli_init(void);

/**
 * @brief Sends a Generic Client message over BLE Mesh.
 *
 * This function sends a message from a Generic Client model to a specified address
 * within the BLE Mesh network, using the provided opcode and parameters.
 *
 * @param[in] p_model   Pointer to the Generic Client model instance.
 * @param[in] p_set     Pointer to the structure containing the message parameters to set.
 * @param[in] opcode    Operation code specifying the type of message to send.
 * @param[in] addr      Destination address within the BLE Mesh network.
 * @param[in] net_idx   Network index identifying the subnet to use.
 * @param[in] app_idx   Application key index to encrypt the message.
 * @param[in] is_get_opcode   Flag indicating if the opcode is a GET request.
 *
 * @return meshx_err_t  Result of the operation. Returns MESHX_OK on success or an error code on failure.
 */
meshx_err_t meshx_plat_gen_cli_send_msg(
    meshx_ptr_t p_model, meshx_gen_cli_set_t *p_set,
    uint16_t opcode, uint16_t addr,
    uint16_t net_idx, uint16_t app_idx,
    bool is_get_opcode
);
#endif /* __MESHX_BLE_MESH_GEN_CLI_H__ */
