/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_light_ctl_client.h
 * @brief Header file for the Light CTL (Color Temperature Light) Client model.
 *
 * This file contains the definitions and function declarations for the Light CTL Client model
 * used in ESP32 BLE Mesh applications. The Light CTL Client model is responsible for sending
 * messages to control the color temperature and lightness of a light.
 *
 * The file includes necessary BLE Mesh headers and defines the event types, callback function
 * types, and structures used for registering and handling Light CTL Client events.
 *
 * @author Pranjal Chanda
 */

#ifndef __LIGHT_CTL_CLIENT_H__
#define __LIGHT_CTL_CLIENT_H__

#include "app_common.h"
#include "meshx_control_task.h"
#include "meshx_gen_light_cli.h"

/**
 * @brief Structure to hold arguments for sending Light CTL messages.
 */
typedef struct meshx_ctl_el_state
{
    uint16_t lightness;          /**< Lightness value to be sent. */
    uint16_t temperature;        /**< Temperature value to be sent. */
    uint16_t delta_uv;           /**< Delta UV value to be sent. */
} meshx_ctl_el_state_t;

/**
 * @brief Structure to hold the On/Off Server to element message.
 */
typedef struct meshx_ctl_cli_el_msg
{
    uint8_t err_code;               /**< Error code */
    meshx_ctx_t ctx;                /**< Context of the message */
    meshx_model_t model;            /**< Generic OnOff Server model */
    meshx_ctl_el_state_t ctl_state; /**< The present value of Generic OnOff state */
}meshx_ctl_cli_el_msg_t;

/**
 * @brief Structure representing the Light CTL (Color Temperature Lightness) client model in MeshX.
 *
 * This structure holds pointers to the SIG model, publication structures, and generic structures
 * associated with the Light CTL client functionality.
 */
typedef struct meshx_light_ctl_client_model
{
    meshx_ptr_t meshx_light_ctl_client_sig_model;     /**< Pointer to the Light CTL client SIG model. */
    meshx_ptr_t meshx_light_ctl_client_pub;           /**< Pointer to the list of Light CTL client publication structures. */
    meshx_ptr_t meshx_light_ctl_client_gen_cli;       /**< Pointer to the list of Light CTL client generic structures. */
} meshx_light_ctl_client_model_t;

/**
 * @brief Initialize the Light CTL Client model.
 *
 * This function initializes the Light CTL Client model, setting up necessary
 * resources and configurations.
 *
 * @return
 *    - MESHX_SUCCESS: Success
 *    - MESHX_NO_MEM: Out of memory
 *    - MESHX_FAIL: Other failures
 */
meshx_err_t meshx_light_ctl_client_init();

/**
 * @brief Registers a callback function for the Light CTL (Color Temperature Lightness) client model.
 *
 * This function associates a user-defined callback with a specific Light CTL client model,
 * allowing the application to handle events or responses related to the model.
 *
 * @param[in] model_id The unique identifier of the Light CTL client model instance.
 * @param[in] cb       The callback function to be registered. This function will be called
 *                     when relevant events occur for the specified model.
 *
 * @return meshx_err_t Returns MESHX_OK on success, or an appropriate error code on failure.
 */
meshx_err_t meshx_light_ctl_cli_reg_cb(uint32_t model_id, meshx_gen_light_cli_cb_t cb);

/**
 * @brief Creates and initializes a Generic Light Client model instance.
 *
 * This function allocates and sets up a Generic Light Client model, associating it with the provided
 * SIG model context.
 *
 * @param[out] p_model      Pointer to a pointer where the created model instance will be stored.
 * @param[in]  p_sig_model  Pointer to the SIG model context to associate with the client model.
 *
 * @return meshx_err_t      Returns an error code indicating the result of the operation.
 *                         - MESHX_OK on success
 *                         - Appropriate error code otherwise
 */
meshx_err_t meshx_light_ctl_client_create(meshx_light_ctl_client_model_t **p_model, void *p_sig_model);

/**
 * @brief Delete the Light client model instance.
 *
 * This function deletes an instance of the Light client model, freeing
 * associated resources and setting the model pointer to NULL.
 *
 * @param[in,out] p_model Double pointer to the Light client model instance to be deleted.
 *
 * @return
 *     - MESHX_SUCCESS: Successfully deleted the model.
 *     - MESHX_INVALID_ARG: Invalid argument provided.
 */
meshx_err_t meshx_light_ctl_client_delete(meshx_light_ctl_client_model_t **p_model);

#endif /*__LIGHT_CTL_CLIENT_H__*/
