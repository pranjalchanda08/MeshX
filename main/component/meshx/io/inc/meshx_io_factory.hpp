/**
 * @file meshx_io_factory.hpp
 * @brief MeshX IO Factory
 *
 * This file defines the IO factory for creating IO instances based on configuration.
 *
 * @author MeshX Team
 * @date 2024
 */

#ifndef __MESHX_IO_FACTORY_HPP
#define __MESHX_IO_FACTORY_HPP

#include <memory>
#include <unordered_map>
#include <string>
#include "meshx_io_interface.hpp"

namespace meshx {

/**
 * @brief IO Type enumeration
 */
enum class IoType {
    GPIO,          /**< General Purpose Input/Output */
    PWM,           /**< Pulse Width Modulation */
    ADC,           /**< Analog to Digital Converter (future) */
    DAC,           /**< Digital to Analog Converter (future) */
    CUSTOM         /**< Custom IO type */
};

/**
 * @brief IO Configuration Structure
 */
struct IoConfig {
    uint8_t logical_pin;          /**< Logical pin number (0-255) */
    IoType type;                  /**< IO type */
    std::string name;             /**< Pin name */

    // Type-specific configuration
    union {
        struct {
            uint8_t mode;         /**< GPIO mode */
            uint8_t pull;         /**< Pull resistor setting */
            uint8_t drive;        /**< Drive strength */
            uint8_t initial_level;/**< Initial output level */
            bool signal_inversion;/**< Signal inversion */
        } gpio;

        struct {
            uint32_t frequency;   /**< PWM frequency */
            uint8_t duty_cycle;   /**< Initial duty cycle */
            uint8_t resolution;   /**< PWM resolution */
            uint8_t channel;      /**< Hardware channel */
        } pwm;

        struct {
            uint16_t custom_id;   /**< Custom function ID */
            uint32_t custom_data; /**< Custom function data */
        } custom;
    } config;
};

/**
 * @brief IO Factory Class
 *
 * This factory creates IO instances based on configuration.
 */
class MeshXIoFactory {
public:
    /**
     * @brief Get singleton instance
     *
     * @return MeshXIoFactory& Reference to singleton instance
     */
    static MeshXIoFactory& getInstance();

    /**
     * @brief Register IO type factory function
     *
     * @param type IO type
     * @param factory_func Factory function
     * @return true if registration successful
     * @return false if registration failed (type already registered)
     */
    bool registerType(IoType type, IoFactoryFunction factory_func);

    /**
     * @brief Create IO instance
     *
     * @param config IO configuration
     * @return std::unique_ptr<MeshXIoInterface> IO instance pointer
     */
    std::unique_ptr<MeshXIoInterface> create(const IoConfig& config);

    /**
     * @brief Create IO instance from YAML configuration
     *
     * @param yaml_config YAML configuration node
     * @return std::unique_ptr<MeshXIoInterface> IO instance pointer
     */
    std::unique_ptr<MeshXIoInterface> createFromYaml(const void* yaml_config);

    /**
     * @brief Check if IO type is supported
     *
     * @param type IO type to check
     * @return true if type is supported
     * @return false if type is not supported
     */
    bool isTypeSupported(IoType type) const;

private:
    MeshXIoFactory() = default;
    ~MeshXIoFactory() = default;

    // Delete copy constructor and assignment operator
    MeshXIoFactory(const MeshXIoFactory&) = delete;
    MeshXIoFactory& operator=(const MeshXIoFactory&) = delete;

    // Factory function registry
    std::unordered_map<IoType, IoFactoryFunction> factory_registry_;
};

} // namespace meshx

#endif /* __MESHX_IO_FACTORY_HPP */
