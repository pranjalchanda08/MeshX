/**
 * @file meshx_model_class.cpp
 * @brief Implementation of MeshX model wrapper classes
 *
 * This file contains the implementation of the constructor functions for
 * the MeshX model wrapper classes, providing the core initialization logic
 * for both client and server model wrappers.
 *
 * Key Features:
 * - Base model initialization
 * - Template-based constructor implementations
 * - Consistent initialization pattern for all model types
 * - Error handling and status reporting
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

/**
 * @defgroup meshx_model MeshX Model
 * @{
 */

#include <meshx_model_class.hpp>

/*****************************************************************************************************
 * meshXModel
 ******************************************************************************************************/
/**
 * @brief A template class for creating BLE mesh models.
 *
 * This class acts as a wrapper around the meshXBaseModel class and provides a
 * convenient interface for creating BLE mesh models.
 *
 * @tparam meshxBaseModel_t The type of the meshXBaseModel class to be used.
 * @tparam meshx_send_packet_params_t The type of the meshXSendPacketParams structure used
 * for sending packets.
 * @param model_id The unique identifier of the BLE mesh model.
 * @param from_ble_cb The callback function to be registered for the model.
 */
MESHX_MODEL_TEMPLATE_PROTO
meshXModel MESHX_MODEL_TEMPLATE_PARAMS::meshXModel(uint32_t model_id, const control_msg_cb& from_ble_cb)
{
    base_model = new meshxBaseModel_t(model_id, from_ble_cb);
}

/****************************************************************************************************
 * meshXServerModel
 ****************************************************************************************************/
/**
 * @brief Constructor for the meshXServerModel class.
 *
 * This constructor initializes a meshXServerModel object with the given model ID and control task message handle.
 *
 * @param model_id The model ID associated with the server model.
 * @param from_ble_cb The control task message handle associated with the server model.
 */
MESHX_SERVER_MODEL_TEMPLATE_PROTO
meshXServerModel MESHX_SERVER_MODEL_TEMPLATE_PARAMS::meshXServerModel(uint32_t model_id, const control_msg_cb& from_ble_cb)
    : meshXModel<meshxBaseServerModel_t, meshx_send_packet_params_t>(model_id, from_ble_cb) {}

/**************************************************************************************************
 * meshXClientModel
 **************************************************************************************************/
/**
 * @brief A class for the client model.
 *
 * This class is used to define the client model and its associated functionality.
 *
 * @param model_id The model ID associated with the client model.
 * @param from_ble_cb The control task message handle associated with the client model.
 */
MESHX_CLIENT_MODEL_TEMPLATE_PROTO
meshXClientModel MESHX_CLIENT_MODEL_TEMPLATE_PARAMS::meshXClientModel(uint32_t model_id, const control_msg_cb& from_ble_cb)
    : meshXModel<meshxBaseClientModel_t, meshx_send_packet_params_t>(model_id, from_ble_cb) {}

/**
 * @}
 */
