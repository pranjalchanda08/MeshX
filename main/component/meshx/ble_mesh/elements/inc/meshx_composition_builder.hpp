/**
 * @file meshx_composition_builder.hpp
 * @brief Header file for the BLE Mesh composition builder.
 *
 * This file contains the meshXCompositionBuilder class definition which provides
 * a fluent API for building the mesh composition.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#ifndef __MESHX_COMPOSITION_BUILDER_HPP__
#define __MESHX_COMPOSITION_BUILDER_HPP__

#include <meshx_fwd_decl.hpp>
#include <meshx_composition.hpp>

/**
 * @class meshXCompositionBuilder
 * @brief Fluent API for building the mesh composition.
 */
class meshXCompositionBuilder {
public:
    meshXCompositionBuilder() = default;

    /**
     * @brief Start building the composition
     * @return Reference to the builder
     */
    meshXCompositionBuilder& begin();

    /**
     * @brief Add a Relay Server element
     * @return Reference to the builder
     */
    meshXCompositionBuilder& add_relay_server();

    /**
     * @brief Add a Relay Client element
     * @return Reference to the builder
     */
    meshXCompositionBuilder& add_relay_client();

    /**
     * @brief Add a Light CWWW Server element
     * @return Reference to the builder
     */
    meshXCompositionBuilder& add_cwww_server();

    /**
     * @brief Add a Light CWWW Client element
     * @return Reference to the builder
     */
    meshXCompositionBuilder& add_cwww_client();

    /**
     * @brief Add a Sensor Server element
     * @return Reference to the builder
     */
    meshXCompositionBuilder& add_sensor_server();

    /**
     * @brief Add an RGB (HSL) Server element
     * @return Reference to the builder
     */
    meshXCompositionBuilder& add_rgb_server();

    /**
     * @brief Commit the composition (Triggers Baking)
     * @return Reference to the builder
     */
    meshXCompositionBuilder& commit();
};

#endif /* __MESHX_COMPOSITION_BUILDER_HPP__ */
