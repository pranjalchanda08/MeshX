/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_onoff_server.h
 * @brief Header file for the On/Off Server model in the BLE Mesh Node application.
 *
 * This file contains the function declarations and necessary includes for the
 * On/Off Server model used in the BLE Mesh Node application.
 *
 * @author Pranjal Chanda
 */

#ifndef __MESHX_ONOFF_SERVER__
#define __MESHX_ONOFF_SERVER__

#include "app_common.h"
#include "meshx_gen_server.h"
#include "meshx_control_task.h"

/**
 * @brief Structure to hold the CW-WW server on/off state.
 */
typedef struct meshx_on_off_srv_el_state
{
    uint8_t on_off; /**< On/Off state */
} meshx_on_off_srv_el_state_t;

/**
 * @brief Structure to hold the On/Off Server to element message.
 */
typedef struct meshx_on_off_srv_el_msg
{
    meshx_model_t model;        /**< Generic OnOff Server model */
    uint8_t on_off_state;       /**< The present value of Generic OnOff state */
}meshx_on_off_srv_el_msg_t;

/**
 * @brief Structure representing the MeshX On/Off Server Model.
 *
 * This structure is used to define the On/Off server model in the MeshX framework.
 * It contains pointers to various components required for the On/Off server functionality.
 */
typedef struct meshx_onoff_server_model
{
    meshx_ptr_t meshx_server_sig_model;     /**< Pointer to the On/Off server SIG model. */
    meshx_ptr_t meshx_server_pub;           /**< Pointer to the list of relay server publication structures. */
    meshx_ptr_t meshx_server_onoff_gen_srv; /**< Pointer to the list of relay server On/Off generic structures. */
} meshx_onoff_server_model_t;

/**
 * @brief Create and initialize a new On/Off server model instance.
 *
 * This function allocates memory for a new On/Off server model and initializes
 * it using the platform-specific creation function. It ensures that the model
 * is properly set up for handling Generic OnOff messages in a BLE Mesh network.
 *
 * @param[in,out] p_model Pointer to a pointer where the newly created On/Off server model
 *                instance will be stored.
 * @param[in,out] p_sig_model Pointer to a pointer where the offset of the model will be stored.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully created and initialized the model.
 *     - MESHX_INVALID_ARG: The provided pointer is NULL.
 *     - MESHX_NO_MEM: Memory allocation failed.
 */
meshx_err_t meshx_on_off_server_create(meshx_onoff_server_model_t **p_model, meshx_ptr_t p_sig_model);

/**
 * @brief Delete the On/Off server model instance.
 *
 * This function deletes an instance of the On/Off server model, freeing
 * associated resources and setting the model pointer to NULL.
 *
 * @param[in,out] p_model Double pointer to the On/Off server model instance to be deleted.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully deleted the model.
 *     - MESHX_INVALID_ARG: Invalid argument provided.
 */
meshx_err_t meshx_on_off_server_delete(meshx_onoff_server_model_t **p_model);

/**
 * @brief Initialize the On/Off server model.
 *
 * This function initializes the On/Off server model for the BLE mesh node.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_FAIL: Failure
 */
meshx_err_t meshx_on_off_server_init(void);

/**
 * @brief Restore the On/Off state for the generic server model.
 *
 * This function restores the On/Off state of the specified server model
 * using the provided state value. It checks for a valid model pointer
 * before proceeding with the restoration.
 *
 * @param[in] p_model       Pointer to the On/Off server model structure.
 * @param[in] onoff_state   The On/Off state to be restored.
 *
 * @return
 *     - MESHX_INVALID_STATE: If the model pointer is NULL.
 *     - Result of the platform-specific restoration function.
 */
meshx_err_t meshx_gen_on_off_srv_state_restore(meshx_ptr_t p_model, meshx_on_off_srv_el_state_t onoff_state);

#endif /* __MESHX_ONOFF_SERVER__ */
