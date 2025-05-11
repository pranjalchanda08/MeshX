/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file meshx_ble_mesh_gen_srv.h
 * @brief Header file for Generic Server Models in MeshX BLE Mesh.
 *
 * This file defines the structures, types, and APIs for implementing
 * Generic Server Models in the MeshX BLE Mesh stack. It includes
 * definitions for various state change events, server callbacks, and
 * functions to create, delete, and manage the state of Generic Server Models.
 *
 * @author Pranjal Chanda
 *
 */


#ifndef __MESHX_PLAT_GEN_SRV_H__
#define __MESHX_PLAT_GEN_SRV_H__

#include "../meshx_ble_mesh_cmn.h"
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
 * @brief Creates a Generic OnOff Server model and its publication context.
 *
 * This function initializes the Generic OnOff Server model, its publication
 * context, and allocates memory for the server instance. It checks for
 * invalid arguments and handles memory allocation failures.
 *
 * @param[out] p_model Pointer to the model structure to be created.
 * @param[out] p_pub Pointer to the publication context to be created.
 * @param[out] p_onoff_srv Pointer to the OnOff server instance to be allocated.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully created the model and publication context.
 *     - MESHX_INVALID_ARG: One or more arguments are invalid.
 *     - MESHX_NO_MEM: Memory allocation failed.
 */
meshx_err_t meshx_plat_on_off_gen_srv_create(void* p_model, void** p_pub, void** p_onoff_srv);

/**
 * @brief Deletes the Generic OnOff Server model and its associated resources.
 *
 * This function frees the memory allocated for the Generic OnOff Server
 * and sets the pointer to NULL. It also deletes the model publication
 * resources associated with the server.
 *
 * @param[in,out] p_pub Pointer to the publication structure to be deleted.
 * @param[in,out] p_onoff_srv Pointer to the OnOff Server structure to be freed.
 *
 * @return
 *     - MESHX_SUCCESS: Model and publication deleted successfully.
 *     - MESHX_FAIL: Failed to delete the model or publication.
 */
meshx_err_t meshx_plat_on_off_gen_srv_delete(void** p_pub, void** p_onoff_srv);

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

/**
 * @brief Restores the state of the Generic OnOff Server model.
 *
 * This function sets the user data of the specified model to the given state.
 * It checks if the model pointer is valid before proceeding with the operation.
 *
 * @param[in] p_model Pointer to the model structure.
 * @param[in] state The state to be restored in the model.
 *
 * @return
 *     - MESHX_SUCCESS: State restored successfully.
 *     - MESHX_INVALID_ARG: Invalid model pointer.
 */
meshx_err_t meshx_plat_gen_on_off_srv_restore(void* p_model, uint8_t state);

#endif /* __MESHX_PLAT_GEN_SRV_H__ */
