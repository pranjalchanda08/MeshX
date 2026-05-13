/**
 * @file meshx_io_factory.cpp
 * @brief MeshX IO Factory Implementation
 *
 * This file implements the IO factory for creating IO instances.
 *
 * @author MeshX Team
 * @date 2024
 */

#include "../inc/meshx_io_factory.hpp"
#include "../inc/meshx_io_types.hpp"
#include <stdexcept>

namespace meshx {

// Forward declarations of factory functions
std::unique_ptr<MeshXIoInterface> createGpioInstance(uint8_t logical_pin, const char* name);
std::unique_ptr<MeshXIoInterface> createPwmInstance(uint8_t logical_pin, const char* name);

/**
 * @brief Get singleton instance
 *
 * @return MeshXIoFactory& Reference to singleton instance
 */
MeshXIoFactory& MeshXIoFactory::getInstance() {
    static MeshXIoFactory instance;

    // Register default factory functions on first call
    static bool initialized = false;
    if (!initialized) {
        instance.registerType(IoType::GPIO, createGpioInstance);
        instance.registerType(IoType::PWM, createPwmInstance);
        initialized = true;
    }

    return instance;
}

/**
 * @brief Register IO type factory function
 *
 * @param type IO type
 * @param factory_func Factory function
 * @return true if registration successful
 * @return false if registration failed (type already registered)
 */
bool MeshXIoFactory::registerType(IoType type, IoFactoryFunction factory_func) {
    if (factory_func == nullptr) {
        return false;
    }

    auto it = factory_registry_.find(type);
    if (it != factory_registry_.end()) {
        // Type already registered
        return false;
    }

    factory_registry_[type] = factory_func;
    return true;
}

/**
 * @brief Create IO instance
 *
 * @param config IO configuration
 * @return std::unique_ptr<MeshXIoInterface> IO instance pointer
 */
std::unique_ptr<MeshXIoInterface> MeshXIoFactory::create(const IoConfig& config) {
    // Check if type is supported
    auto it = factory_registry_.find(config.type);
    if (it == factory_registry_.end()) {
        // Type not supported
        return nullptr;
    }

    // Call factory function
    return it->second(config.logical_pin, config.name.c_str());
}

/**
 * @brief Create IO instance from YAML configuration
 *
 * @param yaml_config YAML configuration node
 * @return std::unique_ptr<MeshXIoInterface> IO instance pointer
 */
std::unique_ptr<MeshXIoInterface> MeshXIoFactory::createFromYaml(const void* yaml_config) {
    // Note: YAML parsing would be implemented here
    // For now, this is a placeholder implementation
    (void)yaml_config;
    return nullptr;
}

/**
 * @brief Check if IO type is supported
 *
 * @param type IO type to check
 * @return true if type is supported
 * @return false if type is not supported
 */
bool MeshXIoFactory::isTypeSupported(IoType type) const {
    return factory_registry_.find(type) != factory_registry_.end();
}

} // namespace meshx
