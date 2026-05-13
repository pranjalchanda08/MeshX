/**
 * @file meshx_io_interface.hpp
 * @brief MeshX Abstract IO Interface
 *
 * This file defines the abstract IO interface for unified IO operations.
 * The interface provides a function-based API for extensible GPIO, PWM, and custom IO.
 *
 * @author MeshX Team
 * @date 2024
 */

#ifndef __MESHX_IO_INTERFACE_HPP
#define __MESHX_IO_INTERFACE_HPP

#include <cstdint>
#include <vector>
#include <memory>
#include "meshx_c_header.h"  // Follow C/C++ boundary pattern

namespace meshx {

/**
 * @brief IO Function Types for function-based API
 */
enum class IoFunction {
    SET_LEVEL = 0,          /**< Set pin level function */
    GET_LEVEL,              /**< Get pin level function */
    TOGGLE,                 /**< Toggle pin function */
    SET_PWM_DUTY,           /**< Set PWM duty cycle function */
    SET_PWM_FREQUENCY,      /**< Set PWM frequency function */
    REGISTER_INTERRUPT,     /**< Register interrupt function */
    UNREGISTER_INTERRUPT,   /**< Unregister interrupt function */
    ENABLE_INTERRUPT,       /**< Enable/disable interrupt function */
    CUSTOM_FUNCTION,        /**< Custom function (for extensibility) */
    FUNCTION_MAX            /**< Maximum function type */
};

/**
 * @brief Abstract IO Interface
 *
 * This interface provides a unified API for all IO operations using
 * a function-based approach for extensibility.
 */
class MeshXIoInterface {
public:
    virtual ~MeshXIoInterface() = default;

    /**
     * @brief Execute IO function
     *
     * This is the core function-based API for all IO operations.
     *
     * @param function IO function to execute
     * @param args Function arguments vector
     * @return int Error code (0 = success, negative = error)
     */
    virtual int execute(IoFunction function, const std::vector<uint32_t>& args) = 0;

    /**
     * @brief Get logical pin number
     *
     * @return uint8_t Logical pin number (0-255)
     */
    virtual uint8_t getLogicalPin() const = 0;

    /**
     * @brief Get pin name
     *
     * @return const char* Pin name string
     */
    virtual const char* getName() const = 0;

    /**
     * @brief Check if function is supported
     *
     * @param function IO function to check
     * @return true if function is supported
     * @return false if function is not supported
     */
    virtual bool isFunctionSupported(IoFunction function) const = 0;

    // Convenience methods (implemented in terms of execute())

    /**
     * @brief Set pin level (convenience method)
     *
     * @param level Pin level (0 = low, 1 = high)
     * @return int Error code (0 = success, negative = error)
     */
    virtual int setLevel(uint8_t level) {
        return execute(IoFunction::SET_LEVEL, {static_cast<uint32_t>(level)});
    }

    /**
     * @brief Get pin level (convenience method)
     *
     * @param[out] level Pointer to store pin level
     * @return int Error code (0 = success, negative = error)
     */
    virtual int getLevel(uint8_t* level) {
        if (!level) return -1;
        std::vector<uint32_t> args = {static_cast<uint32_t>(reinterpret_cast<uintptr_t>(level))};
        return execute(IoFunction::GET_LEVEL, args);
    }

    /**
     * @brief Toggle pin level (convenience method)
     *
     * @return int Error code (0 = success, negative = error)
     */
    virtual int toggle() {
        return execute(IoFunction::TOGGLE, {});
    }

    /**
     * @brief Set PWM duty cycle (convenience method)
     *
     * @param duty_cycle Duty cycle (0-100%)
     * @return int Error code (0 = success, negative = error)
     */
    virtual int setPwmDutyCycle(uint8_t duty_cycle) {
        return execute(IoFunction::SET_PWM_DUTY, {static_cast<uint32_t>(duty_cycle)});
    }

    /**
     * @brief Set PWM frequency (convenience method)
     *
     * @param frequency Frequency in Hz
     * @return int Error code (0 = success, negative = error)
     */
    virtual int setPwmFrequency(uint32_t frequency) {
        return execute(IoFunction::SET_PWM_FREQUENCY, {frequency});
    }

    /**
     * @brief Register interrupt (convenience method)
     *
     * @param callback Function pointer to callback
     * @param user_data User data passed to callback
     * @param trigger_type Interrupt trigger type
     * @return int Error code (0 = success, negative = error)
     */
    virtual int registerInterrupt(void (*callback)(uint8_t, void*),
                                  void* user_data,
                                  uint8_t trigger_type) {
        return execute(IoFunction::REGISTER_INTERRUPT, {
            reinterpret_cast<uintptr_t>(callback),
            reinterpret_cast<uintptr_t>(user_data),
            static_cast<uint32_t>(trigger_type)
        });
    }

    /**
     * @brief Unregister interrupt (convenience method)
     *
     * @return int Error code (0 = success, negative = error)
     */
    virtual int unregisterInterrupt() {
        return execute(IoFunction::UNREGISTER_INTERRUPT, {});
    }

    /**
     * @brief Enable/disable interrupt (convenience method)
     *
     * @param enable true to enable, false to disable
     * @return int Error code (0 = success, negative = error)
     */
    virtual int enableInterrupt(bool enable) {
        return execute(IoFunction::ENABLE_INTERRUPT, {static_cast<uint32_t>(enable ? 1 : 0)});
    }
};

/**
 * @brief IO Factory function type
 */
using IoFactoryFunction = std::unique_ptr<MeshXIoInterface>(*)(uint8_t logical_pin, const char* name);

} // namespace meshx

#endif /* __MESHX_IO_INTERFACE_HPP */
