/**
 * @file meshx_fwd_decl.hpp
 * @brief Forward declaration of MeshX classes
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 * @author Pranjal Chanda
 */

#ifndef __MESHX_FWD_DEC_H__
#define __MESHX_FWD_DEC_H__

/***************************************************************************************************************************************
 * Includes
 ***************************************************************************************************************************************/
#include <vector>
#include <memory>
#include <meshx_c_header.h>

/***************************************************************************************************************************************
 * Template Prototypes
 ***************************************************************************************************************************************/
#define MESHX_BASE_TEMPLATE_PROTO          template <typename ble_mesh_send_msg_params_t>
#define MESHX_BASE_TEMPLATE_PARAMS                  <ble_mesh_send_msg_params_t>
#define MESHX_BASE_CLIENT_TEMPLATE_PROTO   template <typename baseClientModelDerived_t, typename ble_mesh_send_msg_params_t, typename ble_mesh_plat_model_cb_params_t>
#define MESHX_BASE_CLIENT_TEMPLATE_PARAMS           <baseClientModelDerived_t, ble_mesh_send_msg_params_t, ble_mesh_plat_model_cb_params_t>
#define MESHX_BASE_SERVER_TEMPLATE_PROTO   template <typename baseServerModelDerived_t, typename ble_mesh_send_msg_params_t, typename ble_mesh_plat_restore_params_t, typename ble_mesh_plat_cb_params_t>
#define MESHX_BASE_SERVER_TEMPLATE_PARAMS           <baseServerModelDerived_t, ble_mesh_send_msg_params_t, ble_mesh_plat_restore_params_t, ble_mesh_plat_cb_params_t>

#define MESHX_MODEL_TEMPLATE_PROTO          template <typename meshxBaseModel_t, typename meshx_send_packet_params_t>
#define MESHX_MODEL_TEMPLATE_PARAMS                  <meshxBaseModel_t, meshx_send_packet_params_t>
#define MESHX_SERVER_MODEL_TEMPLATE_PROTO   template <typename meshxBaseServerModel_t, typename meshx_send_packet_params_t>
#define MESHX_SERVER_MODEL_TEMPLATE_PARAMS           <meshxBaseServerModel_t, meshx_send_packet_params_t>
#define MESHX_CLIENT_MODEL_TEMPLATE_PROTO   template <typename meshxBaseClientModel_t, typename meshx_send_packet_params_t>
#define MESHX_CLIENT_MODEL_TEMPLATE_PARAMS           <meshxBaseClientModel_t, meshx_send_packet_params_t>

#define MESHX_ELEMENT_TEMPLATE_PROTO        template <typename meshx_model_send_param_header_t>
#define MESHX_ELEMENT_TEMPLATE_PARAMS                <meshx_model_send_param_header_t>
#define MESHX_SERVER_ELEMENT_TEMPLATE_PROTO
#define MESHX_SERVER_ELEMENT_TEMPLATE_PARAMS
#define MESHX_CLIENT_ELEMENT_TEMPLATE_PROTO
#define MESHX_CLIENT_ELEMENT_TEMPLATE_PARAMS

/***************************************************************************************************************************************
 * Forward declaration of Classes
 ***************************************************************************************************************************************/

MESHX_BASE_TEMPLATE_PROTO class meshXBaseModel;
MESHX_BASE_SERVER_TEMPLATE_PROTO class meshXBaseServerModel;
MESHX_BASE_CLIENT_TEMPLATE_PROTO class meshXBaseClientModel;

class meshXModelIF;
MESHX_MODEL_TEMPLATE_PROTO class meshXModel;
MESHX_SERVER_MODEL_TEMPLATE_PROTO class meshXServerModel;
MESHX_CLIENT_MODEL_TEMPLATE_PROTO class meshXClientModel;

/*********************************************************************************
 * meshXElementIF
 *********************************************************************************/
enum class meshxElementType
{
    MESHX_ELEMENT_TYPE_SERVER = 0,
    MESHX_ELEMENT_TYPE_CLIENT = 1
};

/**
 * @class meshXElementIF
 * @brief Interface class for MeshX elements
 * @details This is an interface class defining the base functionality for mesh elements.
 */

using meshxElementType_t = enum meshxElementType;

/**
 * @brief Common element context structure for base handling.
 */
typedef struct {
    uint16_t app_id;
    uint16_t pub_addr;
} meshx_element_common_ctx_t;

class meshXElementIF
{
private:
    uint16_t element_idx;
public:
    /**
     * @brief Handle model callback from child models.
     *
     * This is a pure virtual function that must be implemented by derived classes.
     * It is called when a model event occurs, allowing the element to handle the event.
     *
     * @param[in] param      Pointer to the model event parameters.
     * @param[in] param_size Size of the parameter structure.
     * @return MESHX_SUCCESS on success, error code otherwise.
     */
    virtual meshx_err_t on_model_cb(meshx_ptr_t param, size_t param_size) = 0;

    void set_element_idx(uint16_t idx) { element_idx = idx; }
    uint16_t get_element_idx(void) const { return element_idx; }

    /**
     * @brief Called when the composition is baked to update the element's index.
     * @param index The final platform index of this element.
     */
    virtual void on_baked(uint16_t index) = 0;

    /**
     * @brief Get the element type
     * @return Element type (server or client)
     */
    virtual meshxElementType_t get_element_type(void) const = 0;

    /**
     * @brief Get the element variant
     * @return Element variant (meshx_element_type_t)
     */
    virtual meshx_element_type_t get_element_variant(void) const = 0;

    /**
     * @brief Get the number of SIG models supported by the element
     * @return Number of SIG models
     */
    virtual uint8_t get_no_of_sig_models(void) const = 0;

    /**
     * @brief Get the number of Vendor models supported by the element
     * @return Number of Vendor models
     */
    virtual uint8_t get_no_of_ven_models(void) const = 0;
    
    /**
     * @brief Lists and creates all required SIG models for the element.
     */
    virtual uint8_t list_sig_models(void) = 0;

    /**
     * @brief Lists and creates all required Vendor models for the element.
     */
    virtual uint8_t list_ven_models(void) = 0;

    /**
     * @brief Get the SIG models vector
     * @return Reference to the SIG models vector
     */
    virtual std::vector<std::unique_ptr<meshXModelIF>>& get_sig_models(void) = 0;

    /**
     * @brief Get the Vendor models vector
     * @return Reference to the Vendor models vector
     */
    virtual std::vector<std::unique_ptr<meshXModelIF>>& get_ven_models(void) = 0;

    /**
     * @brief Initialize the element
     * @return MESHX_SUCCESS on success, error code otherwise
     */
    virtual meshx_err_t initialize(void) = 0;

    /**
     * @brief Reset the element
     * @return MESHX_SUCCESS on success, error code otherwise
     */
    virtual meshx_err_t reset(void) = 0;

    /**
     * @brief Check if the element is initialized
     * @return true if initialized, false otherwise
     */
    virtual bool is_initialized(void) const = 0;

    /**
     * @brief Restore the element's context from NVS.
     * @return MESHX_SUCCESS on success, error code otherwise.
     */
    virtual meshx_err_t restore_nvs_context(void) = 0;

    /**
     * @brief Check if the element contains a specific model by ID
     * @param model_id The model ID to check for
     * @return true if the model exists in this element, false otherwise
     */
    virtual bool has_model(uint16_t model_id) const = 0;

    /**
     * @brief Get the element context structure
     * @return Pointer to the element context structure
     */
    virtual meshx_ptr_t get_element_ctx(void) const = 0;

    /**
     * @brief Get the size of the element context structure
     * @return Size of the element context structure
     */
    virtual size_t get_element_ctx_size(void) const = 0;

    /**
     * @brief Synchronize element state (Status broadcast or GET request)
     * @param evt The event trigger (STACK_READY or FRESH_BOOT)
     */
    virtual void sync(control_task_msg_evt_t evt) = 0;

    /**
     * @brief Handle configuration server events (AppKey bind, Publication, etc.)
     * @param evt The configuration event code
     * @param params Pointer to the configuration parameters
     */
    virtual void handle_config(control_task_msg_evt_t evt, const meshx_config_srv_cb_param_t *params) = 0;

    meshXElementIF() = delete;
    explicit meshXElementIF(uint16_t element_idx) : element_idx(element_idx) { }
    virtual ~meshXElementIF() = default;
};

MESHX_ELEMENT_TEMPLATE_PROTO        class meshXElement;
MESHX_SERVER_ELEMENT_TEMPLATE_PROTO class meshXElementServer;
MESHX_CLIENT_ELEMENT_TEMPLATE_PROTO class meshXElementClient;

#endif/* __MESHX_FWD_DEC_H__ */
