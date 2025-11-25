/**
 * @file meshx_element_class.cpp
 * @brief MeshX Element class and interface definations
 *
 * This file contains the meshXElement class and its interface meshXElementIF.
 * The meshXElement class represents an element in the MeshX BLE mesh network,
 * while the meshXElementIF interface defines the callback function for model events.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#include <meshx_element_class.hpp>
#include <memory>

/*****************************************************************************************************
 * meshXElement
 *****************************************************************************************************/
/**
 * @brief Constructs a new meshXElement instance.
 *
 * @param[in] element_idx Index of the element in the mesh network
 */
MESHX_ELEMENT_TEMPLATE_PROTO
meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::meshXElement()
    : meshXElementIF(0), no_of_sig_models(0), no_of_ven_models(0)
{ }

/**
 * @brief Constructs a new meshXElement instance.
 *
 * @param[in] element_idx Index of the element in the mesh network
 */
MESHX_ELEMENT_TEMPLATE_PROTO
meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::meshXElement(uint16_t element_idx)
    : meshXElementIF(element_idx), no_of_sig_models(0), no_of_ven_models(0)
{ }

/**
 * @brief Constructs a new meshXElement instance.
 *
 * @param[in] element_idx Index of the element in the mesh network
 * @param[in] no_of_sig_models Number of SIG models in the element
 * @param[in] no_of_ven_models Number of Ven models in the element
 */
MESHX_ELEMENT_TEMPLATE_PROTO
meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::meshXElement(uint16_t element_idx, meshxElementType_t element_type, uint8_t no_of_sig_models, uint8_t no_of_ven_models)
    : meshXElementIF(element_idx), no_of_sig_models(no_of_sig_models), no_of_ven_models(no_of_ven_models), element_type(element_type)
{
    sig_model_array_allocate();
    ven_model_array_allocate();
}

MESHX_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::sig_model_array_allocate()
{
    if (no_of_sig_models > 0)
    {
        sig_model_array.reserve(no_of_sig_models);
        return MESHX_SUCCESS;
    }
    return MESHX_NOT_SUPPORTED;
}

MESHX_ELEMENT_TEMPLATE_PROTO
meshx_err_t meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::ven_model_array_allocate()
{
    if (no_of_ven_models > 0)
    {
        ven_model_array.reserve(no_of_ven_models);
        return MESHX_SUCCESS;
    }
    return MESHX_NOT_SUPPORTED;
}

/*****************************************************************************************************
 * meshXElement - Template Functions
 *****************************************************************************************************/
/**
 * @brief Add a SIG model to the element
 * @tparam meshXModelT Type of the model to add
 * @tparam ConstructorsArgs
 *
 * @param[in] args Constructor arguments for the model
 * @return MESHX_SUCCESS on success, error code otherwise
 */
MESHX_ELEMENT_TEMPLATE_PROTO
MESHX_ELEMENT_ADD_MODEL_TEMPLATE_PROTO
meshx_err_t meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::add_sig_model(ConstructorsArgs&&... args)
{
    static_assert(std::is_base_of<meshXModelIF, meshXModelT>::std::is_base_of_v,
        "meshXModelT must be derived from meshXModelIF");

    if(no_of_sig_models == 0)
    {
        return MESHX_NOT_SUPPORTED;
    }
    if (sig_models.size() >= no_of_sig_models)
    {
        return MESHX_NO_MEM;
    }
    sig_models.emplace_back(std::forward<ConstructorsArgs>(args)...);
    return MESHX_SUCCESS;
}

/**
 * @brief Add a Vendor model to the element
 * @tparam meshXModelT Type of the model to add
 * @tparam ConstructorsArgs
 *
 * @param[in] args Constructor arguments for the model
 * @return MESHX_SUCCESS on success, error code otherwise
 */
MESHX_ELEMENT_TEMPLATE_PROTO
MESHX_ELEMENT_ADD_MODEL_TEMPLATE_PROTO
meshx_err_t meshXElement MESHX_ELEMENT_TEMPLATE_PARAMS
    ::add_ven_model(ConstructorsArgs&&... args)
{
    static_assert(std::is_base_of<meshXModelIF, meshXModelT>::std::is_base_of_v,
        "meshXModelT must be derived from meshXModelIF");
    if(no_of_ven_models == 0)
    {
        return MESHX_NOT_SUPPORTED;
    }

    if (ven_models.size() >= no_of_ven_models)
    {
        return MESHX_NO_MEM;
    }
    ven_models.emplace_back(std::forward<ConstructorsArgs>(args)...);
    return MESHX_SUCCESS;
}
