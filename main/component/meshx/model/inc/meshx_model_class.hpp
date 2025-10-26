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

#include <meshx_base_model_class.hpp>

/*********************************************************************************
 * meshXModel
 *********************************************************************************/
/**
 * @brief meshXModel class
 * @details This is a base class for both client and server models.
 */
MESHX_MODEL_TEMPLATE_PROTO
class meshXModel
{
private:
    /* private members */
    meshXElementIF *parent_element;         /*<! Pointer to the parent element interface */
    meshxBaseModel_t *base_model;           /*<! Pointer to the base model */

    meshx_err_t status;                     /*<! Status of the model */
    MESHX_MODEL *p_plat_model;              /*<! Pointer to the platform model */
    meshx_model_interface_t *model_intr;    /*<! Pointer to the model interface */

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
     * @param[in] dev Device structure containing sender information
     * @param[in] evt Event type indicating the nature of the message
     * @param[in] data Event-specific data payload
     * @return MESHX_SUCCESS if event handled successfully, error code otherwise
     */
    virtual meshx_err_t model_from_ble_cb(dev_struct_t *dev, control_task_msg_evt_t evt, meshx_ptr_t data) = 0;

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
     * @brief Create logical model instance
     * @details Pure virtual function that derived classes must implement to create
     *          a logical model instance on the platform. This is called during
     *          model initialization to set up the model's runtime state.
     *
     * @return MESHX_SUCCESS if model created successfully, error code otherwise
     */
    virtual meshx_err_t plat_model_create(void) = 0;

    /***********************************************************
     * Accessor Functions
     ***********************************************************/
    /**
     * @brief Get the model initialization status
     * @return Status code indicating success or failure of initialization
     */
    meshx_err_t get_init_status(void) const { return status; }

    /**
     * @brief Get the platform-specific model instance
     * @return Pointer to the platform model instance
     */
    MESHX_MODEL * get_plat_model(void) const { return p_plat_model; }

    /**
     * @brief Get the base model instance
     * @return Pointer to the base model implementation
     */
    meshxBaseModel_t * get_base_model(void) const { return base_model; }

    /**
     * @brief Get the model interface
     * @return Pointer to the model interface structure
     */
    meshx_model_interface_t * get_model_intr(void) const { return model_intr; }

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

    meshXModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element = nullptr);
    ~meshXModel() = default;
};

/*********************************************************************************
 * meshXServerModel
 *********************************************************************************/
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
public:
    /**
     * @brief Construct a new Server Model
     * @param[in] p_plat_model Platform-specific model instance
     * @param[in] model_id Unique identifier for this model
     * @param[in] parent_element Parent element interface (optional)
     * @details Initializes a server model with platform-specific implementation
     *          and associates it with an optional parent element
     */
    meshXServerModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element = nullptr);

    /**
     * @brief Default destructor
     */
    ~meshXServerModel() = default;

    /**
     * @brief Deleted default constructor
     * @details Server models must be initialized with a platform model and ID
     */
    meshXServerModel() = delete;
};

/*********************************************************************************
 * meshXClientModel
 *********************************************************************************/
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
public:
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
     * @brief Construct a new meshXClientModel
     * @param[in] p_plat_model Platform model instance
     * @param[in] model_id Model identifier
     * @param[in] parent_element Parent element interface (optional)
     */
    meshXClientModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element = nullptr);

    meshXClientModel() = delete;

    /**
     * @brief Destroy the meshXClientModel
     */
    ~meshXClientModel();
};
