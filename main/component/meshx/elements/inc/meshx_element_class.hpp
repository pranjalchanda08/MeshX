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

    MESHX_MODEL *p_plat_sig_model_array;      /* MESHX_MODEL type array */
    MESHX_MODEL *p_plat_ven_model_array;      /* MESHX_MODEL type array */

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

    MESHX_ELEMENT_ADD_MODEL_TEMPLATE_PROTO
    meshx_err_t add_sig_model(ConstructorsArgs&&... args);

    MESHX_ELEMENT_ADD_MODEL_TEMPLATE_PROTO
    meshx_err_t add_ven_model(ConstructorsArgs&&... args);

    meshXElement() = delete;
    explicit meshXElement(uint16_t element_idx);
    explicit meshXElement(
        uint16_t element_idx,
        uint8_t no_of_sig_models = 0,
        uint8_t no_of_ven_models = 0,
        meshxElementType_t element_type = meshxElementType_t::MESHX_ELEMENT_TYPE_SERVER);

    ~meshXElement() override = default;
};

/**
 * @class meshXElementServer
 * @brief Derived class for server elements
 */
MESHX_SERVER_ELEMENT_TEMPLATE_PROTO
class meshXElementServer : public meshXElement
{
public:
    meshXElementServer() = delete;
    explicit meshXElementServer(uint16_t element_idx, uint8_t no_of_sig_models = 0, uint8_t no_of_ven_models = 0)
        : meshXElement(element_idx, no_of_sig_models, no_of_ven_models, meshxElementType_t::MESHX_ELEMENT_TYPE_SERVER) { }
};

/**
 * @class meshXElementClient
 * @brief Derived class for client elements
 */
MESHX_CLIENT_ELEMENT_TEMPLATE_PROTO
class meshXElementClient : public meshXElement
{
public:
    meshXElementClient() = delete;
    explicit meshXElementClient(uint16_t element_idx, uint8_t no_of_sig_models = 0, uint8_t no_of_ven_models = 0)
        : meshXElement(element_idx, no_of_sig_models, no_of_ven_models, meshxElementType_t::MESHX_ELEMENT_TYPE_CLIENT) { }
};

#endif /* __MESHX_ELEMENT_CLASS__ */
