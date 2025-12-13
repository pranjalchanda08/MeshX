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

enum class meshxElementType
{
    MESHX_ELEMENT_TYPE_SERVER = 0,
    MESHX_ELEMENT_TYPE_CLIENT = 1
};

/*********************************************************************************
 * meshXElementIF
 *********************************************************************************/
/**
 * @class meshXElementIF
 * @brief Interface class for MeshX elements
 * @details This is an interface class defining the base functionality for mesh elements.
 */

using meshxElementType_t = enum meshxElementType;

class meshXElementIF
{
private:
    uint16_t element_idx;
public:
    virtual meshx_err_t on_model_cb(meshx_ptr_t param) = 0;

    void set_element_idx(uint16_t idx) { element_idx = idx; }
    uint16_t get_element_idx(void) const { return element_idx; }

    /**
     * @brief Get the element type
     * @return Element type (server or client)
     */
    virtual meshxElementType_t get_element_type(void) const = 0;

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

    meshXElementIF() = delete;
    explicit meshXElementIF(uint16_t element_idx) : element_idx(element_idx) { }
    virtual ~meshXElementIF() = default;
};

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

    std::vector<MESHX_MODEL> sig_model_array;
    std::vector<MESHX_MODEL> ven_model_array;

    std::vector<std::unique_ptr<meshXModelIF>> sig_models;
    std::vector<std::unique_ptr<meshXModelIF>> ven_models;
    meshxElementType_t element_type;
public:

    void set_element_type(meshxElementType_t type) { element_type = type; }
    meshxElementType_t get_element_type(void) const { return element_type; }

    void set_no_of_sig_models(uint8_t cnt) { no_of_sig_models = cnt; }
    uint8_t get_no_of_sig_models(void) const { return no_of_sig_models; }

    void set_no_of_ven_models(uint8_t cnt) { no_of_ven_models = cnt; }
    uint8_t get_no_of_ven_models(void) const { return no_of_ven_models; }

    std::vector<std::unique_ptr<meshXModelIF>>& get_sig_models(void) { return sig_models; }
    std::vector<std::unique_ptr<meshXModelIF>>& get_ven_models(void) { return ven_models; }

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
    std::vector<MESHX_MODEL>& get_sig_model_array(void) { return sig_model_array; }

    /**
     * @brief Get the Vendor model array
     * @return Reference to the Vendor model array
     */
    std::vector<MESHX_MODEL>& get_ven_model_array(void) { return ven_model_array; }

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
    meshx_err_t add_sig_models();

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
    meshx_err_t add_ven_models();

    /**
     * @brief Add both SIG and Vendor models to the element
     * @details This function combines the functionality of add_sig_models() and add_ven_models()
     *          to add all available models (both SIG and Vendor) to the element in a single call.
     *          It validates input parameters, checks capacity constraints for both model types,
     *          sets parent element for each model, and transfers ownership.
     *
     * @return MESHX_SUCCESS on success, error code otherwise (MESHX_NOT_SUPPORTED, MESHX_INVALID_ARG, MESHX_NO_MEM)
     */
    meshx_err_t add_models();

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
    virtual uint8_t list_sig_models();
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
    virtual uint8_t list_ven_models();

    /**
     * @brief Handle model callback for element
     * @param[in] param Callback parameter
     * @return MESHX_SUCCESS on success, error code otherwise
     */
    meshx_err_t on_model_cb(meshx_ptr_t param) override;

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
    meshXElement();

    /**
     * @brief Constructor for meshXElement with element index
     * @param[in] element_idx Index of the element in the mesh network
     */
    explicit meshXElement(uint16_t element_idx);

    /**
     * @brief Constructor for meshXElement with element index, type, and model counts
     * @param[in] element_idx Index of the element in the mesh network
     * @param[in] type Type of the element (server or client)
     * @param[in] no_of_sig_models Number of SIG models supported by the element
     * @param[in] no_of_ven_models Number of vendor models supported by the element
     */
    meshXElement(uint16_t element_idx, meshxElementType_t type, uint8_t no_of_sig_models, uint8_t no_of_ven_models);

    ~meshXElement() override = default;
};

/***********************************************************************************************************
 * meshXElementServer and meshXElementClient Classes
 ***********************************************************************************************************/
/**
 * @class meshXElementServer
 * @brief Derived class for server elements
 */
MESHX_SERVER_ELEMENT_TEMPLATE_PROTO
class meshXElementServer : public meshXElement
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
class meshXElementClient : public meshXElement
{
public:
    meshXElementClient() = default;
    explicit meshXElementClient(uint16_t element_idx, uint8_t no_of_sig_models = 0, uint8_t no_of_ven_models = 0)
        : meshXElement(element_idx, meshxElementType_t::MESHX_ELEMENT_TYPE_CLIENT, no_of_sig_models, no_of_ven_models) { }
};

#endif /* __MESHX_ELEMENT_CLASS__ */
