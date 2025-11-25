/**
 * @file meshx_element_class.hpp
 * @brief MeshX Element class and interface definations
 * This file contains the meshXElement class and its interface meshXElementIF.
 * The meshXElement class represents an element in the MeshX BLE mesh network,
 * while the meshXElementIF interface defines the callback function for model events.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#include <variants/meshx_root_element.hpp>

constexpr uint16_t MESHX_ROOT_ELEMENT_INDEX = 0;
/*********************************************************************************
 * meshXRootElement
 *********************************************************************************/

meshXRootElement
    :: meshXRootElement()
    : meshXElementServer( MESHX_ROOT_ELEMENT_INDEX )
{

}
