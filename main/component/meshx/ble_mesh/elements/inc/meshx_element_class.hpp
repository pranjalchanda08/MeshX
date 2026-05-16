/**
 * @file meshx_element_class.hpp
 * @brief MeshX Element class and interface declaration
 * This file contains the meshXElement class and its interface meshXElementIF.
 * The meshXElement class represents an element in the MeshX BLE mesh network,
 * while the meshXElementIF interface defines the callback function for model events.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#ifndef __MESHX_ELEMENT_CLASS__
#define __MESHX_ELEMENT_CLASS__

#include <meshx_fwd_decl.hpp>
#include <meshx_model_class.hpp>
#include <memory>
#include <vector>

#define MESHX_ELEMENT_ADD_MODEL_TEMPLATE_PROTO  template <typename meshXModelT, typename... ConstructorsArgs>
#define MESHX_ELEMENT_ADD_MODEL_TEMPLATE_PARAMS <meshXModelT, typename... ConstructorsArgs>

/*********************************************************************************
 * meshXElement
 *********************************************************************************/
/**
 * @class meshXElement
 * @brief Base class for MeshX elements
 * @details This is a base class for elements.
 */
MESHX_ELEMENT_TEMPLATE_PROTO
class meshXElement : public meshXElementIF
{
private:
    uint8_t no_of_sig_models;
    uint8_t no_of_ven_models;

    std::vector<uint8_t> sig_model_array;
    std::vector<uint8_t> ven_model_array;

    std::vector<std::unique_ptr<meshXModelIF>> sig_models;
    std::vector<std::unique_ptr<meshXModelIF>> ven_models;
    meshxElementType_t element_type;
    meshx_element_type_t element_variant;

    meshx_ptr_t element_ctx;        /**< Pointer to element context structure */
    size_t      element_ctx_size;   /**< Size of the element context structure */

    /**
     *
     * @brief Handle model callback from child models.
     *
     * This function is called by child models when a state change occurs.
     * It handles the state change by storing in element context, saving to NVS,
     * and notifying the application.
     * @param[in] param Pointer to the model callback parameter
     * @param[in] param_size Size of the parameter structure
     * @return
     *     - MESHX_SUCCESS: State change handled successfully
     *     - MESHX_INVALID_ARG: Invalid parameter
     */
    meshx_err_t on_model_cb(meshx_ptr_t param, size_t param_size) final;
public:

    void set_element_type(meshxElementType_t type) { element_type = type; }
    meshxElementType_t get_element_type(void) const final { return element_type; }

    void set_element_variant(meshx_element_type_t variant) { element_variant = variant; }
    meshx_element_type_t get_element_variant(void) const final { return element_variant; }

    void set_no_of_sig_models(uint8_t cnt) { no_of_sig_models = cnt; }
    uint8_t get_no_of_sig_models(void) const final { return no_of_sig_models; }

    void set_no_of_ven_models(uint8_t cnt) { no_of_ven_models = cnt; }
    uint8_t get_no_of_ven_models(void) const final { return no_of_ven_models; }

    std::vector<std::unique_ptr<meshXModelIF>>& get_sig_models(void) final { return sig_models; }
    std::vector<std::unique_ptr<meshXModelIF>>& get_ven_models(void) final { return ven_models; }

    const char* get_element_name(void) const override;

    /**
     * @brief Lists and creates all required SIG models for the element.
     *
     * This function is responsible for creating and populating the sig_models vector
     * with all essential SIG models that the element must have.
     *
     * The function can be extended to include additional models based on configuration
     * flags or specific requirements.
     *
     * @return uint8_t The total number of SIG models created and added to the root_sig_models vector
     */
    virtual uint8_t list_sig_models(void) { return 0; };
    /**
     * @brief Lists and creates all required Vendor models for the element.
     *
     * This function is responsible for creating and populating the ven_models vector
     * with all essential Vendor models that the element must have.
     *
     * The function can be extended to include additional models based on configuration
     * flags or specific requirements.
     *
     * @return uint8_t The total number of Vendor models created and added to the root_sig_models vector
     */
    virtual uint8_t list_ven_models(void) { return 0; };

    /**
     * @brief Notify element about state change
     * @note  This function shall be derived by the specific element class to handle
     *       state change notifications from child models (if required).
     *
     * @param[in] param      Pointer to the state change parameter
     * @param[in] param_size Size of the parameter structure
     *
     * @return MESHX_SUCCESS on success, error code otherwise
     */
    virtual meshx_err_t element_state_change_notify(meshx_ptr_t param, size_t param_size)
    {
        /* If not derived, return success */
        return MESHX_SUCCESS;
    }

    /**
     * @brief Register element context structure
     * @details This function registers the element context structure used to
     *          maintain state information for the element.
     *
     * @param[in] ctx      Pointer to the element context structure
     * @param[in] ctx_size Size of the context structure
     */
    void register_element_ctx(meshx_ptr_t ctx, size_t ctx_size)
    {
        element_ctx      = ctx;
        element_ctx_size = ctx_size;
    }
    /**
     * @brief Get the element context structure
     * @return Pointer to the element context structure
     */
    meshx_ptr_t get_element_ctx(void) const override { return element_ctx; }
    /**
     * @brief Get the size of the element context structure
     * @return Size of the element context structure
     */
    size_t get_element_ctx_size(void) const override { return element_ctx_size; }

    /**
     * @brief Allocate memory for SIG model platform array
     * @details This function allocates memory for the SIG model platform array
     *          based on the number of SIG models supported by the element.
     *          It reserves capacity for the specified number of models.
     *
     * @return MESHX_SUCCESS on success, MESHX_NOT_SUPPORTED if no SIG models supported
     */
    meshx_err_t sig_plat_model_array_allocate(void);

    /**
     * @brief Allocate memory for Vendor model platform array
     * @details This function allocates memory for the Vendor model platform array
     *          based on the number of Vendor models supported by the element.
     *          It reserves capacity for the specified number of models.
     *
     * @return MESHX_SUCCESS on success, MESHX_NOT_SUPPORTED if no Vendor models supported
     */
    meshx_err_t ven_plat_model_array_allocate(void);

    /**
     * @brief Get the SIG model array
     * @return Reference to the SIG model array
     */
    std::vector<uint8_t>& get_sig_model_array(void) { return sig_model_array; }

    /**
     * @brief Get the Vendor model array
     * @return Reference to the Vendor model array
     */
    std::vector<uint8_t>& get_ven_model_array(void) { return ven_model_array; }

    /**
     * @brief Add a SIG model to the element
     * @tparam meshXModelT Type of the model to add
     * @tparam ConstructorsArgs Variadic template parameter pack for constructor arguments
     *
     * This function adds a SIG model to the element using perfect forwarding.
     * It validates the model type, checks capacity constraints, and transfers ownership.
     *
     * @param[in] args Constructor arguments for the model
     * @return MESHX_SUCCESS on success, error code otherwise
     */
    MESHX_ELEMENT_ADD_MODEL_TEMPLATE_PROTO
    meshx_err_t add_sig_model(ConstructorsArgs&&... args);

    /**
     * @brief Add a Vendor model to the element
     * @tparam meshXModelT Type of the model to add
     * @tparam ConstructorsArgs Variadic template parameter pack for constructor arguments
     *
     * This function adds a Vendor model to the element using perfect forwarding.
     * It validates the model type, checks capacity constraints, and transfers ownership.
     *
     * @param[in] args Constructor arguments for the model
     * @return MESHX_SUCCESS on success, error code otherwise
     */
    MESHX_ELEMENT_ADD_MODEL_TEMPLATE_PROTO
    meshx_err_t add_ven_model(ConstructorsArgs&&... args);

    /**
     * @brief Get the number of SIG models currently added to the element
     * @return Number of SIG models in the element
     */
    uint8_t get_sig_model_count(void) const { return sig_models.size(); }

    /**
     * @brief Get the number of Vendor models currently added to the element
     * @return Number of Vendor models in the element
     */
    uint8_t get_ven_model_count(void) const { return ven_models.size(); }

    /**
     * @brief Add multiple SIG models to the element
     * @details This function allows adding multiple SIG models at once to the element.
     *          It accepts a vector of unique_ptr to meshXModelIF objects.
     *          The function validates input parameters, checks capacity constraints,
     *          sets parent element for each model, and transfers ownership.
     *
     * @return MESHX_SUCCESS on success, MESHX_NOT_SUPPORTED if no SIG models supported,
     *         MESHX_INVALID_ARG if input vector is empty or contains null pointers,
     *         MESHX_NO_MEM if adding models would exceed capacity
     */
    meshx_err_t add_sig_models(void);

    /**
     * @brief Add multiple Vendor models to the element
     * @details This function allows adding multiple Vendor models at once to the element.
     *          It accepts a vector of unique_ptr to meshXModelIF objects.
     *          The function validates input parameters, checks capacity constraints,
     *          sets parent element for each model, and transfers ownership.
     *
     * @return MESHX_SUCCESS on success, MESHX_NOT_SUPPORTED if no Vendor models supported,
     *         MESHX_INVALID_ARG if input vector is empty or contains null pointers,
     *         MESHX_NO_MEM if adding models would exceed capacity
     */
    meshx_err_t add_ven_models(void);

    /**
     * @brief Add both SIG and Vendor models to the element
     * @details This function combines the functionality of add_sig_models() and add_ven_models()
     *          to add all available models (both SIG and Vendor) to the element in a single call.
     *          It validates input parameters, checks capacity constraints for both model types,
     *          sets parent element for each model, and transfers ownership.
     *
     * @return MESHX_SUCCESS on success, error code otherwise (MESHX_NOT_SUPPORTED, MESHX_INVALID_ARG, MESHX_NO_MEM)
     */
    meshx_err_t add_models(void);

    /**
     * @brief Get the platform model array for SIG models
     * @return Pointer to the SIG model array
     */
    MESHX_MODEL* get_sig_plat_model_array(void);

    /**
     * @brief Get the platform model array for Vendor models
     * @return Pointer to the Vendor model array
     */
    MESHX_MODEL* get_ven_plat_model_array(void);

    /**
     * @brief Default constructor for meshXElement
     * @details Creates an element with default parameters (index 0, no models)
     */
    meshXElement(void);

    /**
     * @brief Constructor for meshXElement with element index
     * @param[in] element_idx Index of the element in the mesh network
     */
    explicit meshXElement(uint16_t element_idx);

    /**
     * @brief Constructor for meshXElement with element index, type, and model counts
     * @param[in] element_idx       Index of the element in the mesh network
     * @param[in] type              Type of the element (server or client)
     * @param[in] no_of_sig_models  Number of SIG models supported by the element
     * @param[in] no_of_ven_models  Number of vendor models supported by the element
     */
    meshXElement(uint16_t element_idx, meshxElementType_t type, uint8_t no_of_sig_models, uint8_t no_of_ven_models);

    meshx_err_t initialize(void) override;
    meshx_err_t reset(void) override;
    bool is_initialized(void) const override;
    meshx_err_t restore_nvs_context(void) override;

    /**
     * @param[in] pdev       Pointer to the device structure.
     * @param[in] evt        The event type.
     * @param[in] params     Pointer to the message parameters.
     * @param[in] params_len Length of the message parameters.
     * @return MESHX_SUCCESS on success, error code otherwise.
     */
    static meshx_err_t static_prov_srv_cb(dev_struct_t *pdev, control_task_msg_evt_t evt, void *params, uint16_t params_len);

    /**
     * @brief Global static provisioning callback for Client elements.
     * @note Signature matches prov_srv_cb_t / control_task_msg_handle_t.
     *
     * @param[in] pdev       Pointer to the device structure.
     * @param[in] evt        The event type.
     * @param[in] params     Pointer to the message parameters.
     * @param[in] params_len Length of the message parameters.
     * @return MESHX_SUCCESS on success, error code otherwise.
     */
    static meshx_err_t static_prov_cli_cb(dev_struct_t *pdev, control_task_msg_evt_t evt, void *params, uint16_t params_len);

    /**
     * @brief Global static configuration callback.
     * @note Signature matches config_srv_cb_t / control_task_msg_handle_t.
     *
     * @param[in] pdev       Pointer to the device structure.
     * @param[in] evt        The event type.
     * @param[in] params     Pointer to the message parameters.
     * @param[in] params_len Length of the message parameters.
     * @return MESHX_SUCCESS on success, error code otherwise.
     */
    static meshx_err_t static_config_cb(dev_struct_t *pdev, control_task_msg_evt_t evt, void *params, uint16_t params_len);

    /**
     * @brief Register the global callbacks with the provisioning server.
     */
    static void register_global_callbacks(void);

    bool has_model(uint16_t model_id) const override;

    void on_baked(uint16_t index) override {
        this->set_element_idx(index);
        for (auto& m : sig_models) m->on_baked();
        for (auto& m : ven_models) m->on_baked();
    }

    ~meshXElement() override;
};

/***********************************************************************************************************
 * meshXElementServer and meshXElementClient Classes
 ***********************************************************************************************************/
/**
 * @class meshXElementServer
 * @brief Derived class for server elements
 */
MESHX_SERVER_ELEMENT_TEMPLATE_PROTO
class meshXElementServer : public meshXElement <meshx_srv_model_send_param_header_t>
{
public:
    meshXElementServer() = default;
    explicit meshXElementServer(uint16_t element_idx, uint8_t no_of_sig_models = 0, uint8_t no_of_ven_models = 0)
        : meshXElement(element_idx, meshxElementType_t::MESHX_ELEMENT_TYPE_SERVER, no_of_sig_models, no_of_ven_models) { }
};

/**
 * @class meshXElementClient
 * @brief Derived class for client elements
 */
MESHX_CLIENT_ELEMENT_TEMPLATE_PROTO
class meshXElementClient : public meshXElement <meshx_cli_model_send_param_header_t>
{
public:
    meshXElementClient() = default;
    explicit meshXElementClient(uint16_t element_idx, uint8_t no_of_sig_models = 0, uint8_t no_of_ven_models = 0)
        : meshXElement(element_idx, meshxElementType_t::MESHX_ELEMENT_TYPE_CLIENT, no_of_sig_models, no_of_ven_models) { }
};

#endif /* __MESHX_ELEMENT_CLASS__ */
