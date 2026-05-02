/**
 * @file meshx_element_registry.hpp
 * @brief Registry for tracking MeshX element instances.
 * 
 * This file provides a global registry to map element indices to element instances,
 * eliminating the need for per-variant static instance arrays.
 * 
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#ifndef __MESHX_ELEMENT_REGISTRY_HPP__
#define __MESHX_ELEMENT_REGISTRY_HPP__

#include <meshx_fwd_decl.hpp>
#include <map>
#include <mutex>

/**
 * @class meshXElementRegistry
 * @brief Singleton registry for MeshX elements.
 */
class meshXElementRegistry
{
private:
    std::map<uint16_t, meshXElementIF*> element_map;
    std::mutex registry_mutex;

    meshXElementRegistry() = default;

public:
    static meshXElementRegistry& get_instance()
    {
        static meshXElementRegistry instance;
        return instance;
    }

    /**
     * @brief Register an element instance.
     * @param element Pointer to the element interface.
     */
    void register_element(meshXElementIF* element)
    {
        if (!element) return;
        std::lock_guard<std::mutex> lock(registry_mutex);
        element_map[element->get_element_idx()] = element;
    }

    /**
     * @brief Unregister an element instance.
     * @param element_idx Index of the element to unregister.
     */
    void unregister_element(uint16_t element_idx)
    {
        std::lock_guard<std::mutex> lock(registry_mutex);
        element_map.erase(element_idx);
    }

    /**
     * @brief Find an element instance by its index.
     * @param element_idx Index of the element.
     * @return meshXElementIF* Pointer to the element instance, or nullptr if not found.
     */
    meshXElementIF* find_element(uint16_t element_idx)
    {
        std::lock_guard<std::mutex> lock(registry_mutex);
        auto it = element_map.find(element_idx);
        if (it != element_map.end())
        {
            return it->second;
        }
        return nullptr;
    }
 
    /**
     * @brief Find and cast an element instance.
     * @tparam T The target element type.
     * @param element_idx Index of the element.
     * @param expected_variant The expected variant type discriminator.
     * @return T* Pointer to the casted element instance, or nullptr if not found or type mismatch.
     */
    template <typename T>
    T* find_and_cast(uint16_t element_idx, meshx_element_type_t expected_variant)
    {
        meshXElementIF* base = find_element(element_idx);
        if (base && base->get_element_variant() == expected_variant)
        {
            return static_cast<T*>(base);
        }
        return nullptr;
    }

    /**
     * @brief Get all registered elements.
     * @return std::map<uint16_t, meshXElementIF*> A copy of the element map.
     */
    std::map<uint16_t, meshXElementIF*> get_all_elements()
    {
        std::lock_guard<std::mutex> lock(registry_mutex);
        return element_map;
    }

    /* Prevent copying */
    meshXElementRegistry(const meshXElementRegistry&) = delete;
    meshXElementRegistry& operator=(const meshXElementRegistry&) = delete;
};

#endif /* __MESHX_ELEMENT_REGISTRY_HPP__ */
