/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_light_ctl_srv.h
 * @brief Header file for the Light CTL Server module.
 *
 * This file contains the function declarations and necessary includes for
 * initializing and managing the Light CTL Server in the BLE mesh network.
 *
 * @author Pranjal Chanda
 */

#ifndef __MESHX_LIGHT_CTL_SRV_H__
#define __MESHX_LIGHT_CTL_SRV_H__

#include "meshx_common.h"
#include "meshx_light_server.h"
#include "meshx_control_task.h"
#include "interface/ble_mesh/meshx_ble_mesh_cmn.h"

#if CONFIG_ENABLE_LIGHT_CTL_SERVER

/**
 * @brief Structure representing the MeshX CTL (Color Temperature Lightness) Setup Server Model.
 *
 * This structure encapsulates pointers to the SIG model, publication structures,
 * and generic server structures required for the CTL setup server functionality.
 */
typedef meshx_model_interface_t meshx_ctl_setup_server_model_t;

/**
 * @brief Structure representing the MeshX CTL (Color Temperature Lightness) server model.
 *
 * This structure encapsulates pointers to the CTL server SIG model, publication structures,
 * and generic server structures used in the MeshX lighting control server implementation.
 */
typedef meshx_model_interface_t meshx_ctl_server_model_t;

/**
 * @brief Create and initialize a new CTL server model instance.
 *
 * This function allocates memory for a new CTL server model and initializes
 * it using the platform-specific creation function. It ensures that the model
 * is properly set up for handling Generic OnOff messages in a BLE Mesh network.
 *
 * @param[in,out] p_model Pointer to a pointer where the newly created CTL server model
 *                instance will be stored.
 * @param[in,out] p_sig_model Pointer to a pointer where the offset of the model will be stored.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully created and initialized the model.
 *     - MESHX_INVALID_ARG: The provided pointer is NULL.
 *     - MESHX_NO_MEM: Memory allocation failed.
 */
meshx_err_t meshx_light_ctl_server_create(meshx_ctl_server_model_t **p_model, meshx_ptr_t p_sig_model);

/**
 * @brief Delete the CTL server model instance.
 *
 * This function deletes an instance of the CTL server model, freeing
 * associated resources and setting the model pointer to NULL.
 *
 * @param[in,out] p_model Double pointer to the CTL server model instance to be deleted.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully deleted the model.
 *     - MESHX_INVALID_ARG: Invalid argument provided.
 */
meshx_err_t meshx_light_ctl_server_delete(meshx_ctl_server_model_t **p_model);

/**
 * @brief Restore the CTL state for the generic server model.
 *
 * This function restores the CTL state of the specified server model
 * using the provided state value. It checks for a valid model pointer
 * before proceeding with the restoration.
 *
 * @param p_model Pointer to the CTL server model structure.
 * @param ctl_state The CTL state to be restored.
 *
 * @return
 *     - MESHX_INVALID_STATE: If the model pointer is NULL.
 *     - Result of the platform-specific restoration function.
 */
meshx_err_t meshx_light_ctl_srv_state_restore(meshx_ctl_server_model_t *p_model, meshx_light_ctl_srv_state_t ctl_state);

/**
 * @brief Initialize the CTL server model.
 *
 * This function initializes the CTL server model for the BLE mesh node.
 *
 * @return
 *     - MESHX_SUCCESS: Success
 *     - MESHX_FAIL: Failure
 */
meshx_err_t meshx_light_ctl_server_init(void);

/**
 * @brief Retrieves the CTL (Color Temperature Lightness) Setup Server model instance.
 *
 * This function is used to obtain a reference or handle to the CTL Setup Server model,
 * which is responsible for managing color temperature and lightness control in a mesh network.
 *
 * @param[in] p_model Pointer to the model structure or context.
 *
 * @return meshx_err_t Returns an error code indicating the result of the operation.
 *                     Typically, MESHX_OK on success or an appropriate error code on failure.
 */
meshx_err_t meshx_get_ctl_setup_srv_model(meshx_ptr_t p_model);

/**
 * @brief Send the Light CTL status message.
 *
 * This function sends the Light CTL status message to the specified context.
 *
 * @param[in] p_model       Pointer to the MeshX model structure.
 * @param[in] ctx           Context structure containing the necessary parameters for sending the message.
 * @param[in] delta_uv      The Delta UV value to be included in the status message.
 * @param[in] lightness     The Lightness value to be included in the status message.
 * @param[in] temperature   The Temperature value to be included in the status message.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully sent the status message.
 *     - Appropriate error code on failure.
 */
meshx_err_t meshx_light_ctl_srv_status_send( meshx_model_t *p_model,
                                             meshx_ctx_t *ctx,
                                             int16_t delta_uv,
                                             uint16_t lightness,
                                             uint16_t temperature);

/**
 * @brief Create a Light CTL Server send message packet.
 *
 * This function creates a Light CTL Server send message packet with the provided parameters.
 *
 * @param[in]  p_model        Pointer to the MeshX model structure.
 * @param[in]  element_id     Element ID associated with the model.
 * @param[in]  net_idx        Network Index for the message.
 * @param[in]  app_idx        Application Index for the message.
 * @param[in]  pub_addr       Publication address for the message.
 * @param[in]  ctl_state      Current state of the Light CTL Server.
 * @param[out] light_srv_send Pointer to the structure where the created message packet will be stored.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully created the message packet.
 *     - MESHX_INVALID_ARG: Invalid argument provided.
 */
meshx_err_t meshx_light_ctl_srv_send_pack_create(
    meshx_ptr_t p_model,
    uint16_t element_id,
    uint16_t net_idx,
    uint16_t app_idx,
    uint16_t pub_addr,
    meshx_light_ctl_srv_state_t ctl_state,
    meshx_lighting_server_cb_param_t *light_srv_send);

#endif /* CONFIG_ENABLE_LIGHT_CTL_SERVER */
#endif /*__MESHX_LIGHT_CTL_SRV_H__*/
