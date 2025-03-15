#ifndef __MESHX_PLAT_GEN_SRV_H__
#define __MESHX_PLAT_GEN_SRV_H__

#include "meshx_ble_mesh_cmn.h"
#include "meshx_control_task.h"

/** Parameter of Generic OnOff Set state change event */
typedef struct
{
    uint8_t onoff; /*!< The value of Generic OnOff state */
} meshx_state_change_gen_onoff_set_t;

/** Parameter of Generic Level Set state change event */
typedef struct
{
    int16_t level; /*!< The value of Generic Level state */
} meshx_state_change_gen_level_set_t;

/** Parameter of Generic Delta Set state change event */
typedef struct
{
    int16_t level; /*!< The value of Generic Level state */
} meshx_state_change_gen_delta_set_t;

/** Parameter of Generic Move Set state change event */
typedef struct
{
    int16_t level; /*!< The value of Generic Level state */
} meshx_state_change_gen_move_set_t;

/** Parameter of Generic Default Transition Time Set state change event */
typedef struct
{
    uint8_t trans_time; /*!< The value of Generic Default Transition Time state */
} meshx_state_change_gen_def_trans_time_set_t;

/** Parameter of Generic OnPowerUp Set state change event */
typedef struct
{
    uint8_t onpowerup; /*!< The value of Generic OnPowerUp state */
} meshx_state_change_gen_onpowerup_set_t;

/** Parameter of Generic Power Level Set state change event */
typedef struct
{
    uint16_t power; /*!< The value of Generic Power Actual state */
} meshx_state_change_gen_power_level_set_t;

/** Parameter of Generic Power Default Set state change event */
typedef struct
{
    uint16_t power; /*!< The value of Generic Power Default state */
} meshx_state_change_gen_power_default_set_t;

/** Parameters of Generic Power Range Set state change event */
typedef struct
{
    uint16_t range_min; /*!< The minimum value of Generic Power Range state */
    uint16_t range_max; /*!< The maximum value of Generic Power Range state */
} meshx_state_change_gen_power_range_set_t;

/** Parameters of Generic Location Global Set state change event */
typedef struct
{
    int32_t latitude;  /*!< The Global Latitude value of Generic Location state */
    int32_t longitude; /*!< The Global Longitude value of Generic Location state */
    int16_t altitude;  /*!< The Global Altitude value of Generic Location state */
} meshx_state_change_gen_loc_global_set_t;

/** Parameters of Generic Location Local Set state change event */
typedef struct
{
    int16_t north;        /*!< The Local North value of Generic Location state */
    int16_t east;         /*!< The Local East value of Generic Location state */
    int16_t altitude;     /*!< The Local Altitude value of Generic Location state */
    uint8_t floor_number; /*!< The Floor Number value of Generic Location state */
    uint16_t uncertainty; /*!< The Uncertainty value of Generic Location state */
} meshx_state_change_gen_loc_local_set_t;

/** Parameters of Generic User Property Set state change event */
typedef struct
{
    uint16_t id; /*!< The property id of Generic User Property state */
    void *value; /*!< The property value of Generic User Property state */
} meshx_state_change_gen_user_property_set_t;

/** Parameters of Generic Admin Property Set state change event */
typedef struct
{
    uint16_t id;    /*!< The property id of Generic Admin Property state */
    uint8_t access; /*!< The property access of Generic Admin Property state */
    void *value;    /*!< The property value of Generic Admin Property state */
} meshx_state_change_gen_admin_property_set_t;

/** Parameters of Generic Manufacturer Property Set state change event */
typedef struct
{
    uint16_t id;    /*!< The property id of Generic Manufacturer Property state */
    uint8_t access; /*!< The property value of Generic Manufacturer Property state */
} meshx_state_change_gen_manu_property_set_t;

typedef union
{
    /**
     * The recv_op in ctx can be used to decide which state is changed.
     */
    meshx_state_change_gen_onoff_set_t onoff_set;                   /*!< Generic OnOff Set */
    meshx_state_change_gen_level_set_t level_set;                   /*!< Generic Level Set */
    meshx_state_change_gen_delta_set_t delta_set;                   /*!< Generic Delta Set */
    meshx_state_change_gen_move_set_t move_set;                     /*!< Generic Move Set */
    meshx_state_change_gen_def_trans_time_set_t def_trans_time_set; /*!< Generic Default Transition Time Set */
    meshx_state_change_gen_onpowerup_set_t onpowerup_set;           /*!< Generic OnPowerUp Set */
    meshx_state_change_gen_power_level_set_t power_level_set;       /*!< Generic Power Level Set */
    meshx_state_change_gen_power_default_set_t power_default_set;   /*!< Generic Power Default Set */
    meshx_state_change_gen_power_range_set_t power_range_set;       /*!< Generic Power Range Set */
    meshx_state_change_gen_loc_global_set_t loc_global_set;         /*!< Generic Location Global Set */
    meshx_state_change_gen_loc_local_set_t loc_local_set;           /*!< Generic Location Local Set */
    meshx_state_change_gen_user_property_set_t user_property_set;   /*!< Generic User Property Set */
    meshx_state_change_gen_admin_property_set_t admin_property_set; /*!< Generic Admin Property Set */
    meshx_state_change_gen_manu_property_set_t manu_property_set;   /*!< Generic Manufacturer Property Set */
} meshx_gen_srv_state_change_t;

typedef struct meshx_gen_srv_cb_param
{
    meshx_ctx_t ctx;
    meshx_model_t model;
    meshx_gen_srv_state_change_t state_change;
} meshx_gen_srv_cb_param_t;

typedef struct meshx_on_off_srv
{
    meshx_model_t model;
    uint8_t on_off_state;
}meshx_on_off_srv_t;

typedef control_task_msg_handle_t meshx_server_cb;

/**
 * @brief Retrieve the model ID of a generic server model.
 *
 * This function obtains the model ID associated with a specified generic server model.
 *
 * @param[in]  p_model   Pointer to the model whose ID is to be retrieved.
 * @param[out] model_id  Pointer to a variable where the retrieved model ID will be stored.
 *
 * @return MESHX_SUCCESS on success, or an appropriate error code on failure.
 */
meshx_err_t meshx_plat_get_gen_srv_model_id(void* p_model, uint16_t *model_id);

/**
 * @brief Set the state of a generic server model.
 *
 * This function updates the on/off state of a specified generic server model.
 *
 * @param[in] p_model       Pointer to the model whose state is to be set.
 * @param[in] on_off_state  The desired on/off state to set for the model.
 *
 * @return MESHX_SUCCESS on success, or an appropriate error code on failure.
 */
meshx_err_t meshx_plat_set_gen_srv_state(void * p_model, uint8_t on_off_state);

/**
 * @brief Initialize the generic server model platform.
 *
 * @return MESHX_SUCCESS on success, or an appropriate error code on failure.
 */
meshx_err_t meshx_plat_gen_srv_init(void);

#endif /* __MESHX_PLAT_GEN_SRV_H__ */
