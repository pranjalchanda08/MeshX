/**
 * @file meshx_element_class.hpp
 * @brief MeshX Element class and interface definitions
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

/*********************************************************************************
 * meshXElementIF
 *********************************************************************************/
/**
 * @class meshXElementIF
 * @brief Interface class for MeshX elements
 * @details This is an interface class defining the base functionality for mesh elements.
 */
class meshXElementIF
{
private:
    uint16_t element_idx;
public:
    virtual meshx_err_t on_model_cb(meshx_ptr_t param) = 0;

    void set_element_idx(uint16_t idx) { element_idx = idx; }
    uint16_t get_element_idx(void) { return element_idx; }

    meshXElementIF() { }
    meshXElementIF(uint16_t element_idx) : element_idx(element_idx) { }
    virtual ~meshXElementIF() { }
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
public:
    meshXElement() { }
    meshXElement(uint16_t element_idx) : meshXElementIF(element_idx) { }
    virtual ~meshXElement() { }
};

#endif /* __MESHX_ELEMENT_CLASS__ */
