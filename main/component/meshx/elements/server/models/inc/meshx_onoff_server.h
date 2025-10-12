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

#include "meshx_common.h"
#include "meshx_gen_server.h"
#include "meshx_control_task.h"

#if CONFIG_ENABLE_GEN_ONOFF_SERVER
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
typedef meshx_model_interface_t meshx_onoff_server_model_t;

/**
 * @brief Send the On/Off status message to the client.
 *
 * This function sends the On/Off status message to the client in response to a
 * Generic OnOff Set or Get request. It uses the provided model and context to
 * construct and send the message.
 *
 * @param[in] model         The model instance that is sending the status.
 * @param[in] ctx           The context containing information about the message.
 * @param[in] on_off_state  The current On/Off state to be sent in the status message.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully sent the status message.
 *     - MESHX_INVALID_ARG: Invalid argument provided.
 *     - MESHX_ERR_PLAT: Platform-specific error occurred.
 */
meshx_err_t meshx_gen_on_off_srv_status_send(
    meshx_model_t *model,
    meshx_ctx_t *ctx,
    uint8_t on_off_state
);
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

/**
 * @brief Create a message packet for sending On/Off status.
 *
 * This function prepares a message packet containing the On/Off status
 * information to be sent to a client. It populates the provided
 * `meshx_gen_srv_cb_param_t` structure with the necessary details.
 *
 * @param[in] p_model       Pointer to the model instance sending the status.
 * @param[in] element_id    The element ID associated with the model.
 * @param[in] key_id        The network key index to be used for sending the message.
 * @param[in] app_id        The application key index to be used for sending the message.
 * @param[in] addr          The destination address to which the message is sent.
 * @param[in] state         The On/Off state value to be included in the message.
 * @param[out] p_send_pack  Pointer to the structure where the message packet will be created.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully created the message packet.
 *     - MESHX_INVALID_ARG: Invalid argument provided (NULL pointers).
 */
meshx_err_t meshx_gen_on_off_srv_send_pack_create(
    meshx_ptr_t p_model,
    uint16_t element_id,
    uint8_t key_id,
    uint8_t app_id,
    uint16_t addr,
    uint8_t state,
    meshx_gen_srv_cb_param_t *p_send_pack
);
#endif /* CONFIG_ENABLE_GEN_ONOFF_SERVER */
#endif /* __MESHX_ONOFF_SERVER__ */
