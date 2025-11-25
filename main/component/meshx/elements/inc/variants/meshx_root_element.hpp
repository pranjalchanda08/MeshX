/**
 * @file meshx_root_element.hpp
 * @brief MeshX Root Element class definition
 * This file contains the meshXRootElement class which represents the root element
 * in the MeshX BLE mesh network.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#ifndef __MESHX_ROOT_ELEMENT_HPP__
#define __MESHX_ROOT_ELEMENT_HPP__

#include <meshx_element_class.hpp>

#define MESHX_ROOT_ELEMENT_TEMPLATE_PROTO
/*********************************************************************************
 * meshXRootElement
 *********************************************************************************/
/**
 * @class meshXRootElement
 * @brief Derived class for the root element
 */
MESHX_ROOT_ELEMENT_TEMPLATE_PROTO
class meshXRootElement : public meshXElementServer MESHX_SERVER_ELEMENT_TEMPLATE_PARAMS
{
public:
    meshXRootElement();
};

#endif /* __MESHX_ROOT_ELEMENT_HPP__ */
