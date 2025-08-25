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

#define MESHX_LIGHT_CTL_CLI_MSG_SET 0
#define MESHX_LIGHT_CTL_CLI_MSG_GET 1
#define MESHX_LIGHT_CTL_CLI_MSG_ACK 1
#define MESHX_LIGHT_CTL_CLI_MSG_NO_ACK 0

/**
 * @brief Structure to hold arguments for sending Light CTL messages.
 */
typedef struct meshx_ctl_el_state
{
    uint16_t lightness;          /**< Lightness value to be sent. */
    uint16_t temperature;        /**< Temperature value to be sent. */
    uint16_t delta_uv;           /**< Delta UV value to be sent. */
    uint16_t temp_range_max;     /**< Maximum temperature range. */
    uint16_t temp_range_min;     /**< Minimum temperature range. */
    uint16_t temp_def;           /**< Default temperature value. */
    uint16_t lightness_def;      /**< Default lightness value. */
    uint16_t delta_uv_def;       /**< Default delta UV value. */
} meshx_ctl_el_state_t;

/**
 * @brief Structure to hold the On/Off Server to element message.
 */
typedef struct meshx_ctl_cli_el_msg
{
    uint8_t err_code;               /**< Error code */
    meshx_model_t model;            /**< Generic OnOff Server model */
    meshx_ctx_t ctx;                /**< Context of the message */
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

/**
 * @brief Sends a Light CTL (Color Temperature Lightness) message from the Light CTL Client model.
 *
 * This function constructs and sends a Light CTL message to a specified destination address
 * using the provided network and application indices. The message contains the desired lightness,
 * temperature, and a transaction identifier (TID).
 *
 * @param[in] model        Pointer to the Light CTL Client model instance.
 * @param[in] opcode       Opcode of the Light CTL message to be sent.
 * @param[in] addr         Destination address for the message.
 * @param[in] net_idx      Network index to be used for sending the message.
 * @param[in] app_idx      Application index to be used for sending the message.
 * @param[in] lightness    Desired lightness value to be set.
 * @param[in] temperature  Desired color temperature value to be set.
 * @param[in] delta_uv     Desired delta UV value to be set.
 * @param[in] tid          Transaction Identifier for the message.
 *
 * @return meshx_err_t     Returns the result of the message send operation.
 */
meshx_err_t meshx_light_ctl_client_send_msg(
        meshx_light_ctl_client_model_t *model,
        uint16_t opcode,  uint16_t addr,
        uint16_t net_idx, uint16_t app_idx,
        uint16_t lightness, uint16_t temperature,
        uint16_t delta_uv, uint8_t tid
);

/**
 * @brief Sends a Light CTL Temperature message from the client model.
 *
 * This function constructs and sends a Light CTL Temperature message to a specified address
 * using the provided network and application indices. It allows the client to control the
 * color temperature and delta UV of a lighting element in a mesh network.
 *
 * @param[in] model        Pointer to the Light CTL client model instance.
 * @param[in] opcode       Opcode of the message to be sent.
 * @param[in] addr         Destination address of the message.
 * @param[in] net_idx      Network index to be used for sending the message.
 * @param[in] app_idx      Application index to be used for sending the message.
 * @param[in] temperature  Desired color temperature value to be set.
 * @param[in] delta_uv     Delta UV value to be set.
 * @param[in] tid          Transaction Identifier for the message.
 *
 * @return meshx_err_t     Result of the message send operation.
 */
meshx_err_t meshx_light_ctl_temperature_client_send_msg(
        meshx_light_ctl_client_model_t *model,
        uint16_t opcode,  uint16_t addr,
        uint16_t net_idx, uint16_t app_idx,
        uint16_t temperature, uint16_t delta_uv, uint8_t tid
);

/**
 * @brief Sends a Light CTL Temperature Range message from the client model.
 *
 * This function constructs and sends a Light CTL Temperature Range message to a specified address
 * using the provided network and application indices. It allows the client to set or get the
 * temperature range of a lighting element in a mesh network.
 *
 * @param[in] model        Pointer to the Light CTL client model instance.
 * @param[in] opcode       Opcode of the message to be sent.
 * @param[in] addr         Destination address of the message.
 * @param[in] net_idx      Network index to be used for sending the message.
 * @param[in] app_idx      Application index to be used for sending the message.
 * @param[in] temp_min     Minimum temperature value of the range to be set.
 * @param[in] temp_max     Maximum temperature value of the range to be set.
 *
 * @return meshx_err_t     Result of the message send operation.
 */
meshx_err_t meshx_light_ctl_temp_range_client_send_msg(
        meshx_light_ctl_client_model_t *model,
        uint16_t opcode,  uint16_t addr,
        uint16_t net_idx, uint16_t app_idx,
        uint16_t temp_min, uint16_t temp_max
);
#endif /*__LIGHT_CTL_CLIENT_H__*/
