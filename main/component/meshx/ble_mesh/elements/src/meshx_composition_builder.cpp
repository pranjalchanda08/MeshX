/**
 * @file meshx_composition_builder.cpp
 * @brief Implementation of MeshX Composition Builder and its C-wrapper.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 */

#include <meshx_composition_builder.hpp>
#include <meshx_composition.hpp>
#include <meshx_device.hpp>
#include <variants/meshx_relay_element.hpp>
#include <variants/meshx_cwww_element.hpp>
#include <variants/meshx_sensor_element.hpp>
#include <variants/meshx_rgb_element.hpp>
#include <variants/meshx_root_element.hpp>

// Removed std::map for embedded footprint and performance

/**
 * @brief Fluent API Implementation
 */
meshXCompositionBuilder& meshXCompositionBuilder::begin() {
    auto& comp = meshXComposition::get_instance();
    comp.clear_elements();
    
    // All compositions must start with a Root Element at Index 0
    comp.get_elements().push_back(std::make_unique<meshXRootElement>());
    
    return *this;
}

meshXCompositionBuilder& meshXCompositionBuilder::add_relay_server() {
#if CONFIG_RELAY_SERVER_COUNT > 0
    auto& comp = meshXComposition::get_instance();
    uint16_t next_idx = (uint16_t)(comp.get_elements().size() + 1);
    comp.get_elements().push_back(std::make_unique<meshXRelayServerElement>(next_idx));
#endif
    return *this;
}

meshXCompositionBuilder& meshXCompositionBuilder::add_relay_client() {
#if CONFIG_RELAY_CLIENT_COUNT > 0
    auto& comp = meshXComposition::get_instance();
    uint16_t next_idx = (uint16_t)(comp.get_elements().size() + 1);
    comp.get_elements().push_back(std::make_unique<meshXRelayClientElement>(next_idx));
#endif
    return *this;
}

meshXCompositionBuilder& meshXCompositionBuilder::add_cwww_server() {
#if CONFIG_LIGHT_CWWW_SRV_COUNT > 0
    auto& comp = meshXComposition::get_instance();
    uint16_t next_idx = (uint16_t)(comp.get_elements().size() + 1);
    comp.get_elements().push_back(std::make_unique<meshXCWWWServerElement>(next_idx));
#endif
    return *this;
}

meshXCompositionBuilder& meshXCompositionBuilder::add_cwww_client() {
#if CONFIG_LIGHT_CWWW_CLIENT_COUNT > 0
    auto& comp = meshXComposition::get_instance();
    uint16_t next_idx = (uint16_t)(comp.get_elements().size() + 1);
    comp.get_elements().push_back(std::make_unique<meshXCWWWClientElement>(next_idx));
#endif
    return *this;
}

meshXCompositionBuilder& meshXCompositionBuilder::add_sensor_server() {
#if CONFIG_SENSOR_SERVER_COUNT > 0
    auto& comp = meshXComposition::get_instance();
    uint16_t next_idx = (uint16_t)(comp.get_elements().size() + 1);
    comp.get_elements().push_back(std::make_unique<meshXSensorElement>(next_idx));
#endif
    return *this;
}

meshXCompositionBuilder& meshXCompositionBuilder::add_rgb_server() {
#if CONFIG_RGB_SERVER_COUNT > 0
    auto& comp = meshXComposition::get_instance();
    uint16_t next_idx = (uint16_t)(comp.get_elements().size() + 1);
    comp.get_elements().push_back(std::make_unique<meshXRGBServerElement>(next_idx));
#endif
    return *this;
}

meshXCompositionBuilder& meshXCompositionBuilder::commit() {
    /* Currently just a marker, bake() happens during meshx_init */
    return *this;
}

/**
 * @brief C-Wrapper Implementation
 */
extern "C" {

#include <meshx_api.h>

bool meshx_builder_is_active(void) {
    return meshXComposition::get_instance().has_elements();
}

meshx_err_t meshx_builder_bake(dev_struct_t *pdev, uint16_t cid, uint16_t pid, uint16_t vid) {
    // 1. Initialize the C++ Device wrapper
    meshXDevice::get_instance().init(pdev);
 
    // 2. Perform the bake
    meshXComposition::get_instance().set_device_struct(pdev);
    meshx_err_t err = meshXComposition::get_instance().bake(cid, pid, vid);
 
    // 3. Visualize the result
    if (err == MESHX_SUCCESS) {
        meshXDevice::get_instance().visualize_status();
    }
 
    return err;
}

void meshx_builder_add_element(meshx_element_type_t type, uint16_t count) {
    meshXCompositionBuilder builder;
    switch(type) {
#if CONFIG_RELAY_SERVER_COUNT > 0
        case MESHX_ELEMENT_TYPE_RELAY_SERVER:
            for(uint16_t i=0; i<count; ++i) builder.add_relay_server();
            break;
#endif
#if CONFIG_RELAY_CLIENT_COUNT > 0
        case MESHX_ELEMENT_TYPE_RELAY_CLIENT:
            for(uint16_t i=0; i<count; ++i) builder.add_relay_client();
            break;
#endif
#if CONFIG_LIGHT_CWWW_SRV_COUNT > 0
        case MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER:
            for(uint16_t i=0; i<count; ++i) builder.add_cwww_server();
            break;
#endif
#if CONFIG_LIGHT_CWWW_CLIENT_COUNT > 0
        case MESHX_ELEMENT_TYPE_LIGHT_CWWW_CLIENT:
            for(uint16_t i=0; i<count; ++i) builder.add_cwww_client();
            break;
#endif
#if CONFIG_SENSOR_SERVER_COUNT > 0
        case MESHX_ELEMENT_TYPE_SENSOR_SERVER:
            for(uint16_t i=0; i<count; ++i) builder.add_sensor_server();
            break;
#endif
#if CONFIG_RGB_SERVER_COUNT > 0
        case MESHX_ELEMENT_TYPE_LIGHT_HSL_SERVER:
            for(uint16_t i=0; i<count; ++i) builder.add_rgb_server();
            break;
#endif
        default:
            MESHX_LOGW(MODULE_ID_COMMON, "Builder: No implementation for element type %d", type);
            break;
    }
}

/**
 * @brief C-Friendly Builder functions
 */

void meshx_builder_begin(void) {
    meshXCompositionBuilder builder;
    builder.begin();
}

void meshx_builder_commit(void) {
    meshXCompositionBuilder builder;
    builder.commit();
}

} /* extern "C" */
