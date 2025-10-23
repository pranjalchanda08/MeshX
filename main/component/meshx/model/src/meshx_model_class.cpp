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
 *
 * @param[in] p_plat_model  A pointer to the platform model (MESHX_MODEL).
 * @param[in] model_id      The unique identifier of the BLE mesh model.
 */
MESHX_MODEL_TEMPLATE_PROTO
meshXModel MESHX_MODEL_TEMPLATE_PARAMS
    ::meshXModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element)
    : parent_element(parent_element)
{
    this->p_plat_model = p_plat_model;
    /* base_model needs to be used logically by the element composition */
    base_model = new meshxBaseModel_t(model_id, model_from_ble_cb);
    model_intr = (meshx_model_interface_t *)MESHX_CALOC(1, sizeof(meshx_model_interface_t));
    if(nullptr == model_intr)
    {
        status = MESHX_NO_MEM;
        delete base_model;
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to allocate memory for model interface");
        return;
    }
    status = MESHX_SUCCESS;
}

/****************************************************************************************************
 * meshXServerModel
 ****************************************************************************************************/
/**
 * @brief Constructor for the meshXServerModel class.
 *
 * This constructor initializes a meshXServerModel object with the given model ID and control task message handle.
 *
 * @param[in] p_plat_model   A pointer to the platform model (MESHX_MODEL).
 * @param[in] model_id       The model ID associated with the server model.
 * @param[in] parent_element A pointer to the parent element.
 */
MESHX_SERVER_MODEL_TEMPLATE_PROTO
meshXServerModel MESHX_SERVER_MODEL_TEMPLATE_PARAMS
    ::meshXServerModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element)
    : meshXModel MESHX_SERVER_MODEL_TEMPLATE_PARAMS (p_plat_model, model_id, parent_element) {}

/**************************************************************************************************
 * meshXClientModel
 **************************************************************************************************/
/**
 * @brief A class for the client model.
 *
 * This class is used to define the client model and its associated functionality.
 * @note This constructor also creates the logical model for the client model and derivatives.
 *       Hence, this constructor should be used to create the client model by the derived client model.
 *
 * @param[in] p_plat_model A pointer to the platform model (MESHX_MODEL).
 * @param[in] model_id The model ID associated with the client model.
 */
MESHX_CLIENT_MODEL_TEMPLATE_PROTO
meshXClientModel MESHX_CLIENT_MODEL_TEMPLATE_PARAMS
    ::meshXClientModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element)
    : meshXModel MESHX_CLIENT_MODEL_TEMPLATE_PARAMS (p_plat_model, model_id, parent_element) { }

/**
 * @brief Creates and initializes a client model instance.
 *
 * This function creates a client model instance associated with the given platform model and
 * model ID. It also initializes the logical model for the client model and its derivatives.
 *
 * @param[in] p_plat_model  A pointer to the platform model (MESHX_MODEL).
 * @param[in] model_id      The model ID associated with the client model.
 *
 * @return meshx_err_t Returns an error code indicating the result of the operation.
 *                      - MESHX_SUCCESS on success
 *                      - Appropriate error code otherwise
 */
MESHX_CLIENT_MODEL_TEMPLATE_PROTO
meshx_err_t meshXClientModel MESHX_CLIENT_MODEL_TEMPLATE_PARAMS
    ::plat_model_create()
{
    meshx_err_t err = MESHX_SUCCESS;

    err = meshx_plat_client_create(
        this->get_plat_model(),
        &this->get_model_intr()->pub,
        &this->get_model_intr()->cli,
        this->get_model_id());
    if (err)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "Failed to create client model");
        err = meshx_plat_client_delete(this->get_plat_model(), &this->get_model_intr()->cli);
        if (err)
        {
            MESHX_LOGE(MODULE_ID_COMMON, "Failed to delete client model");
        }
    }
    return err;
}

/**
 * Destructor for the meshXClientModel class.
 *
 * This destructor is used to delete the client model and release associated resources.
 */
MESHX_CLIENT_MODEL_TEMPLATE_PROTO
meshXClientModel MESHX_CLIENT_MODEL_TEMPLATE_PARAMS::~meshXClientModel()
{
    meshx_plat_client_delete(this->get_plat_model(), &this->get_model_intr()->cli);
}
/**
 * @}
 */
