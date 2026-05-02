/**
 * @file meshx_model_class.hpp
 * @brief Template declarations for MeshX model wrapper classes
 *
 * This file contains the template declarations for the wrapper classes that
 * provide a convenient interface around the MeshX base model classes. It includes
 * the base wrapper (meshXModel) and specialized wrappers for server and client models.
 *
 * Key Features:
 * - Template-based wrapper architecture
 * - Unified interface for both client and server models
 * - Type-safe model creation and management
 * - Simplified integration with platform-specific implementations
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#ifndef _MESHX_MODEL_CLASS_HPP_
#define _MESHX_MODEL_CLASS_HPP_

#include <meshx_base_model_class.hpp>

/*********************************************************************************
 * meshXModel
 *********************************************************************************/

using evt_model_id_t = control_task_msg_evt_t;

class meshXModelIF
{
public:
    meshXModelIF() = default;
    explicit meshXModelIF(MESHX_MODEL *p_plat_model) : p_plat_model(p_plat_model) { }
    virtual ~meshXModelIF() = default;

private:
    MESHX_MODEL     *p_plat_model;               /**< Pointer to the platform model */
    meshx_ptr_t      p_plat_pub;                 /**< publication structures */
    meshx_ptr_t      p_plat_gen;                 /**< generic structures */
    meshXElementIF  *parent_element;             /**< Pointer to the parent element interface */
    meshx_ptr_t      p_parent_element_state;     /**< Pointer to the parent element state */
public:

    /***********************************************************
     * Virtual Functions
     ***********************************************************/
    /**
     * @brief Create logical model instance
     * @details Pure virtual function that derived classes must implement to create
     *          a logical model instance on the platform. This is called during
     *          model initialization to set up the model's runtime state.
     *
     * @return MESHX_SUCCESS if model created successfully, error code otherwise
     */
    virtual meshx_err_t plat_model_create(void) = 0;

    /**
     * @brief Delete logical model instance
     * @details Pure virtual function that derived classes must implement to delete
     *          the logical model instance from the platform. This is called during
     *          model cleanup to release resources associated with the model.
     */
    virtual meshx_err_t plat_model_delete(void) = 0;

    /**
     * @brief Handle state change request from element
     * @details Pure virtual function that derived classes must implement to handle
     *          state change requests from the parent element. The model validates
     *          the request, updates its internal state, and returns the result.
     *
     * @return MESHX_SUCCESS on success, error code otherwise
     */
    virtual meshx_err_t element_state_change_handle(void) = 0;

    /**
     * @brief Prepare message for element notification
     * @details Pure virtual function that derived classes must implement to prepare
     *          a message structure that will be sent to the parent element.
     *          The message must persist after this function returns.
     *
     * @param[out] msg_ptr   Pointer to message structure (output parameter)
     * @param[out] msg_size  Size of the message structure (output parameter)
     * @return MESHX_SUCCESS if message prepared successfully, error code otherwise
     */
    virtual meshx_err_t prepare_element_msg(meshx_ptr_t *msg_ptr, size_t *msg_size) = 0;

    /***********************************************************
     * Accessor Functions
     ***********************************************************/
    /**
     * @brief Set the platform-specific model instance
     * @param[in] p_model Pointer to the platform model instance
     */
    void set_plat_model(MESHX_MODEL *p_model) { p_plat_model = p_model; }

    /**
     * @brief Get the platform-specific model instance
     * @return Pointer to the platform model instance
     */
    MESHX_MODEL * get_plat_model(void) const { return p_plat_model; }

    /**
     * @brief Get the publication structures
     * @return Pointer to the publication structures
     */
    meshx_ptr_t get_pub_struct(void) const { return p_plat_pub; }

    /**
     * @brief Get the generic structures
     * @return Pointer to the generic structures
     */
    meshx_ptr_t get_gen_struct(void) const { return p_plat_gen; }

    /**
     * @brief Set the publication structures
     * @param[in] pub Pointer to the publication structures
     */
    void set_pub_struct(meshx_ptr_t pub) { p_plat_pub = pub; }

    /**
     * @brief Set the generic structures
     * @param[in] gen Pointer to the generic structures
     */
    void set_gen_struct(meshx_ptr_t gen) { p_plat_gen = gen; }

        /**
     * @brief Set the parent element for this model
     * @param[in] parent Pointer to the parent element interface
     */
    void set_parent_element(meshXElementIF *parent) { parent_element = parent; }

    /**
     * @brief Get the parent element of this model
     * @return Pointer to the parent element interface
     */
    meshXElementIF * get_parent_element(void) const { return parent_element; }

    /**
     * @brief Set the parent element state pointer
     * @param[in] state Pointer to the parent element state
     */
    void set_parent_element_state(meshx_ptr_t state) { p_parent_element_state = state; }
    /**
     * @brief Get the parent element state pointer
     * @return Pointer to the parent element state
     */
    meshx_ptr_t get_parent_element_state(void) const { return p_parent_element_state; }
};

/**
 * @brief meshXModel class
 * @details This is a base class for both client and server models.
 */
MESHX_MODEL_TEMPLATE_PROTO
class meshXModel : public meshXModelIF
{
private:
    /* private members */
    meshxBaseModel_t    *base_model;   /*<! Pointer to the base model */
    meshx_err_t         status;        /*<! Status of the model */
    uint16_t            model_id;      /*<! Model identifier */
    uint16_t            model_func_id; /*<! Model function identifier. This is the function ID of the model within an element */

    /**
     * @brief Handle upstream BLE Mesh events
     * @details Static callback function that routes messages and events coming from the BLE Mesh network
     *          to the appropriate model instance. This function extracts the model instance from the
     *          device structure and invokes the instance's model_from_ble_cb method.
     * @param[in] p_dev         Device structure containing sender information
     * @param[in] evt_model_id  Event type indicating the nature of the message
     * @param[in] params        Event-specific data payload
     * @return MESHX_SUCCESS if event handled successfully, error code otherwise
     */
    meshx_err_t model_handle_from_ble_cb(
        dev_struct_t  *p_dev,
        evt_model_id_t evt_model_id,
        meshx_ptr_t    params);

protected:
    /**
     * @brief Update element_state_change field in message header
     * @details Protected virtual method that derived classes (meshXClientModel, meshXServerModel)
     *          must implement to handle type-specific header casting. This method is called by
     *          send_to_parent_element to update the element_state_change field.
     *
     * @param[in] element_state_change Result from element_state_change_handle()
     * @param[in] msg_ptr  Pointer to the message structure
     */
    virtual void update_element_state_change_header(meshx_err_t element_state_change, meshx_ptr_t msg_ptr) = 0;
public:
    /***********************************************************
     * Virtual Functions
     ***********************************************************/
    /**
     * @brief Handle upstream BLE Mesh events
     * @details Pure virtual function that derived classes must implement to process
     *          messages and events coming from the BLE Mesh network. The implementation
     *          will be automatically registered with base_model->base_client_model_cb_list.
     *
     * @param[in] dev       Device structure containing sender information
     * @param[in] evt       Event type indicating the nature of the message
     * @param[in] data      Event-specific data payload
     * @return MESHX_SUCCESS if event handled successfully, error code otherwise
     */
    virtual meshx_err_t model_from_ble_cb(
        dev_struct_t *dev,
        control_task_msg_evt_t evt,
        meshx_ptr_t data) = 0;

    /**
     * @brief Send message through the model
     * @details Pure virtual function that derived classes must implement to send
     *          messages through the model to the BLE Mesh network.
     *
     * @param[in] params Message parameters including destination, opcode, and data
     * @return MESHX_SUCCESS if message sent successfully, error code otherwise
     */
    virtual meshx_err_t model_send(meshx_send_packet_params_t *params) = 0;

    /**
     * @brief Destructor for meshXModel
     * @details Virtual destructor to ensure proper cleanup of derived classes
     *          and the base_model member.
     */
    virtual ~meshXModel();

    /**
     * @brief Send message to parent element
     * @details Common implementation for sending messages to the parent element.
     *          This handles the common pattern of checking parent element,
     *          calling element_state_change_handle(), and calling on_model_cb().
     *          Type-specific header casting is delegated to update_element_state_change_header().
     *
     * @param[in] msg_ptr  Pointer to the message structure
     * @param[in] msg_size Size of the message structure
     * @return MESHX_SUCCESS if message sent successfully, error code otherwise
     */
    meshx_err_t send_to_parent_element(meshx_ptr_t msg_ptr, size_t msg_size);

    /***********************************************************
     * Accessor Functions
     ***********************************************************/
    /**
     * @brief Get the model function identifier
     * @return Model function ID value
     */
    uint16_t get_model_func_id(void) const { return model_func_id; }

    /**
     * @brief Set the model function identifier
     * @param[in] func_id Model function ID to set
     */
    void set_model_func_id(uint16_t func_id) { model_func_id = func_id; }
    /**
     * @brief Get the model identifier
     * @return Model ID value
     */
    uint16_t get_model_id(void) const { return model_id; }

    /**
     * @brief Set the model identifier
     * @param[in] id Model ID to set
     */
    void set_model_id(uint16_t id) { model_id = id; }
    /**
     * @brief Get the model initialization status
     * @return Status code indicating success or failure of initialization
     */
    meshx_err_t get_init_status(void) const { return status; }

    /**
     * @brief Get the base model instance
     * @return Pointer to the base model implementation
     */
    meshxBaseModel_t * get_base_model(void) const { return base_model; }

    /**
     * @brief Constructs a new meshXModel instance.
     *
     * This constructor initializes a meshXModel object with the given platform model,
     * model ID, and optional parent element. It sets up the base model and model interface
     * for BLE mesh communication.
     *
     * @param[in] p_plat_model   Pointer to the platform model instance
     * @param[in] model_id       Unique identifier for this model
     * @param[in] parent_element Optional pointer to the parent element
     * @param[in] model_func_id  Optional model function ID within the element
     *
     * @note The constructor allocates memory for the base model and model interface.
     *       If memory allocation fails, the status will be set to MESHX_NO_MEM.
     */
    meshXModel(
        MESHX_MODEL *p_plat_model,
        uint32_t model_id,
        meshXElementIF *parent_element = nullptr,
        uint16_t model_func_id = 0
    );

};

/*********************************************************************************
 * meshXServerModel
 *********************************************************************************/

/**
 * @brief Structure for server model send parameters
 */
struct meshx_srv_model_send_param_header
{
    meshx_model_t   model;                  /**< Server model Pointer */
    meshx_err_t     element_state_change;   /**< Return value from element_state_change_handle */
};

using meshx_srv_model_send_param_header_t = struct meshx_srv_model_send_param_header;

/**
 * @class meshXServerModel
 * @brief Base class for all server models in MeshX
 * @tparam MESHX_MODEL Platform-specific model type
 * @tparam meshxBaseModel_t Base model implementation type
 * @tparam meshx_send_packet_params_t Type for send packet parameters
 * @details Server model implementation providing core server functionality
 */
MESHX_SERVER_MODEL_TEMPLATE_PROTO
class meshXServerModel : public meshXModel MESHX_SERVER_MODEL_TEMPLATE_PARAMS
{
protected:
    /**
     * @brief Update element_state_change field in server message header
     * @details Overrides base class implementation to handle server-specific header structure
     *          (meshx_srv_model_send_param_header_t) which doesn't include err_code and ctx.
     *
     * @param[in] element_state_change Result from element_state_change_handle()
     * @param[in] msg_ptr  Pointer to the message structure
     */
    void update_element_state_change_header(meshx_err_t element_state_change, meshx_ptr_t msg_ptr) override;

public:
    /**
     * @brief Construct a new Server Model
     * @param[in] p_plat_model              Platform-specific model instance
     * @param[in] model_id                  Unique identifier for this model
     * @param[in] parent_element            Parent element interface (optional)
     * @param[in] parent_element_state      Parent element state pointer (optional)
     * @param[in] model_func_id             Model function ID within the element (optional)
     *
     * @details Initializes a server model with platform-specific implementation
     *          and associates it with an optional parent element
     */
    meshXServerModel(
        MESHX_MODEL    *p_plat_model,
        uint32_t        model_id,
        meshXElementIF *parent_element = nullptr,
        meshx_ptr_t     parent_element_state = nullptr,
        uint16_t        model_func_id = 0
    );

    /**
     * @brief Deleted default constructor
     * @details Server models must be initialized with a platform model and ID
     */
    meshXServerModel() = delete;
};

/*********************************************************************************
 * meshXClientModel
 *********************************************************************************/

struct meshx_cli_model_send_param_header
{
    uint8_t          err_code;               /**< Error code */
    meshx_model_t    model;                  /**< Generic OnOff Server model */
    meshx_ctx_t      ctx;                    /**< Context of the message */
    meshx_err_t      element_state_change;   /**< Return value from element_state_change_handle */
};

using meshx_cli_model_send_param_header_t = struct meshx_cli_model_send_param_header;

/**
 * @class meshXClientModel
 * @brief Base class for all client models in the mesh network
 *
 * @tparam MESHX_MODEL Platform-specific client model implementation type
 * @details Implements core client model functionality including model creation,
 *          message sending, and client-specific operations
 */
MESHX_CLIENT_MODEL_TEMPLATE_PROTO
class meshXClientModel : public meshXModel MESHX_CLIENT_MODEL_TEMPLATE_PARAMS
{
private:
    /**
     * @brief Create platform-specific client model instance
     * @details Final implementation of the model creation process for client models.
     *          This function handles the initialization of client-specific features
     *          and cannot be overridden by derived classes.
     *
     * @return MESHX_SUCCESS on successful model creation and initialization,
     *         error code otherwise
     */
    meshx_err_t plat_model_create(void) final;

    /**
     * @brief Delete platform-specific client model instance
     * @details Final implementation of the model deletion process for client models.
     *          This function handles the cleanup of client-specific resources
     *          and cannot be overridden by derived classes.
     */
    meshx_err_t plat_model_delete(void) final;

protected:
    /**
     * @brief Update element_state_change field in client message header
     * @details Overrides base class implementation to handle client-specific header structure
     *          (meshx_cli_model_send_param_header_t) which includes err_code and ctx fields.
     *
     * @param[in] element_state_change Result from element_state_change_handle()
     * @param[in] msg_ptr  Pointer to the message structure
     */
    void update_element_state_change_header(meshx_err_t element_state_change, meshx_ptr_t msg_ptr) override;

public:

    /**
     * @brief Construct a new meshXClientModel
     * @param[in] p_plat_model          Platform model instance
     * @param[in] model_id              Model identifier
     * @param[in] parent_element        Parent element interface (optional)
     * @param[in] parent_element_state  Parent element state pointer (optional)
     * @param[in] model_func_id         Model function ID within the element (optional)
     */
    meshXClientModel(
        MESHX_MODEL    *p_plat_model,
        uint32_t        model_id,
        meshXElementIF *parent_element = nullptr,
        meshx_ptr_t     parent_element_state = nullptr,
        uint16_t        model_func_id = 0
    );

    meshXClientModel() = delete;
};

#endif /* _MESHX_MODEL_CLASS_HPP_ */
