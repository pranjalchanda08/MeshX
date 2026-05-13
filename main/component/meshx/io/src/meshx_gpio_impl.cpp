/**
 * @file meshx_gpio_impl.cpp
 * @brief MeshX GPIO Concrete Implementation
 *
 * This file implements the concrete GPIO class that inherits from
 * MeshXIoInterface and provides GPIO operations.
 *
 * @author MeshX Team
 * @date 2024
 */

#include "../inc/meshx_io_interface.hpp"
#include "../inc/meshx_io_types.hpp"
#include "../../interface/gpio/meshx_gpio.h"
#include "../../interface/gpio/meshx_gpio_types.h"
#include "../../inc/meshx_err.h"
#include <cstring>
#include <vector>

namespace meshx {

/**
 * @brief Static interrupt callback wrapper for C API
 *
 * @param logical_pin Logical pin number
 * @param user_data User data (pointer to MeshXGpioImpl instance)
 */
static void meshx_gpio_intr_cb_wrapper(uint8_t logical_pin, void *user_data);

/**
 * @brief Concrete GPIO Implementation Class
 */
class MeshXGpioImpl : public MeshXIoInterface {
    // Allow the static wrapper to access private members
    friend void meshx_gpio_intr_cb_wrapper(uint8_t, void*);

private:
    uint8_t m_logical_pin;                 /**< Logical pin number */
    char m_name[32];                       /**< Pin name */
    GpioMode m_mode;                       /**< GPIO mode */
    bool m_initialized;                    /**< Initialization flag */
    void (*m_intr_callback)(uint8_t, void*); /**< Interrupt callback */
    void* m_intr_user_data;                /**< Interrupt user data */

public:
    /**
     * @brief Constructor
     *
     * @param logical_pin Logical pin number
     * @param name Pin name
     * @param mode GPIO mode
     */
    MeshXGpioImpl(uint8_t logical_pin, const char* name, GpioMode mode)
        : m_logical_pin(logical_pin), m_mode(mode), m_initialized(false),
          m_intr_callback(nullptr), m_intr_user_data(nullptr) {
        strncpy(m_name, name, sizeof(m_name) - 1);
        m_name[sizeof(m_name) - 1] = '\0';
    }

    /**
     * @brief Destructor
     */
    virtual ~MeshXGpioImpl() {
        // Cleanup if needed
    }

    /**
     * @brief Get logical pin number
     *
     * @return uint8_t Logical pin number
     */
    virtual uint8_t getLogicalPin() const override {
        return m_logical_pin;
    }

    /**
     * @brief Get pin name
     *
     * @return const char* Pin name
     */
    virtual const char* getName() const override {
        return m_name;
    }

    /**
     * @brief Check if function is supported
     *
     * @param function IO function to check
     * @return true if function is supported
     * @return false if function is not supported
     */
    virtual bool isFunctionSupported(IoFunction function) const override {
        switch (function) {
            case IoFunction::SET_LEVEL:
            case IoFunction::GET_LEVEL:
            case IoFunction::TOGGLE:
                return (m_mode == GpioMode::OUTPUT ||
                        m_mode == GpioMode::INPUT_OUTPUT ||
                        m_mode == GpioMode::OPEN_DRAIN ||
                        m_mode == GpioMode::OPEN_DRAIN_INPUT_OUTPUT);

            case IoFunction::REGISTER_INTERRUPT:
            case IoFunction::UNREGISTER_INTERRUPT:
            case IoFunction::ENABLE_INTERRUPT:
                return (m_mode == GpioMode::INPUT ||
                        m_mode == GpioMode::INPUT_OUTPUT ||
                        m_mode == GpioMode::OPEN_DRAIN_INPUT_OUTPUT);

            case IoFunction::SET_PWM_DUTY:
            case IoFunction::SET_PWM_FREQUENCY:
                return (m_mode == GpioMode::PWM_OUTPUT);

            case IoFunction::CUSTOM_FUNCTION:
                // Custom functions are always supported for extensibility
                return true;

            default:
                return false;
        }
    }

    /**
     * @brief Execute IO function
     *
     * @param function IO function to execute
     * @param args Function arguments vector
     * @return int Error code (0 = success, negative = error)
     */
    virtual int execute(IoFunction function, const std::vector<uint32_t>& args) override {
        if (!m_initialized) {
            // Initialize GPIO if not already initialized
            meshx_err_t err = meshx_gpio_init();
            if (err != MESHX_SUCCESS) {
                return static_cast<int>(IoError::NOT_INITIALIZED);
            }
            m_initialized = true;
        }

        switch (function) {
            case IoFunction::SET_LEVEL: {
                if (args.size() < 1) {
                    return static_cast<int>(IoError::INVALID_LEVEL);
                }
                uint8_t level = static_cast<uint8_t>(args[0]);
                if (level > 1) {
                    return static_cast<int>(IoError::INVALID_LEVEL);
                }
                meshx_err_t err = meshx_gpio_set_level(m_logical_pin, level);
                return (err == MESHX_SUCCESS) ? 0 : static_cast<int>(IoError::INVALID_MODE);
            }

            case IoFunction::GET_LEVEL: {
                if (args.size() < 1) {
                    return static_cast<int>(IoError::INVALID_ARG);
                }
                uint8_t* level_ptr = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(args[0]));
                if (level_ptr == nullptr) {
                    return static_cast<int>(IoError::INVALID_ARG);
                }
                meshx_err_t err = meshx_gpio_get_level(m_logical_pin, level_ptr);
                return (err == MESHX_SUCCESS) ? 0 : static_cast<int>(IoError::INVALID_MODE);
            }

            case IoFunction::TOGGLE: {
                meshx_err_t err = meshx_gpio_toggle(m_logical_pin);
                return (err == MESHX_SUCCESS) ? 0 : static_cast<int>(IoError::INVALID_MODE);
            }

            case IoFunction::SET_PWM_DUTY: {
                if (args.size() < 1) {
                    return static_cast<int>(IoError::PWM_INVALID_PARAM);
                }
                // PWM functions should use meshx_pwm API or meshx_gpio_execute_function
                meshx_err_t err = meshx_gpio_execute_function(m_logical_pin, MESHX_IO_FUNCTION_SET_PWM_DUTY, &args[0], 1);
                return (err == MESHX_SUCCESS) ? 0 : static_cast<int>(IoError::PWM_NOT_SUPPORTED);
            }

            case IoFunction::SET_PWM_FREQUENCY: {
                if (args.size() < 1) {
                    return static_cast<int>(IoError::PWM_INVALID_PARAM);
                }
                meshx_err_t err = meshx_gpio_execute_function(m_logical_pin, MESHX_IO_FUNCTION_SET_PWM_FREQUENCY, &args[0], 1);
                return (err == MESHX_SUCCESS) ? 0 : static_cast<int>(IoError::PWM_NOT_SUPPORTED);
            }

            case IoFunction::REGISTER_INTERRUPT: {
                if (args.size() < 3) {
                    return static_cast<int>(IoError::INVALID_ARG);
                }
                void (*callback)(uint8_t, void*) = reinterpret_cast<void (*)(uint8_t, void*)>(static_cast<uintptr_t>(args[0]));
                void* user_data = reinterpret_cast<void*>(static_cast<uintptr_t>(args[1]));
                uint8_t trigger_type = static_cast<uint8_t>(args[2]);
                return registerInterrupt(callback, user_data, trigger_type);
            }

            case IoFunction::UNREGISTER_INTERRUPT: {
                meshx_err_t err = meshx_gpio_unregister_intr(m_logical_pin);
                return (err == MESHX_SUCCESS) ? 0 : static_cast<int>(IoError::INTR_NOT_SUPPORTED);
            }

            case IoFunction::ENABLE_INTERRUPT: {
                if (args.size() < 1) {
                    return static_cast<int>(IoError::INVALID_ARG);
                }
                bool enable = static_cast<bool>(args[0]);
                meshx_err_t err = meshx_gpio_intr_enable(m_logical_pin, enable);
                return (err == MESHX_SUCCESS) ? 0 : static_cast<int>(IoError::INTR_NOT_SUPPORTED);
            }

            case IoFunction::CUSTOM_FUNCTION: {
                // Custom functions not implemented in this basic version
                return static_cast<int>(IoError::NOT_SUPPORTED);
            }

            default:
                return static_cast<int>(IoError::INVALID_MODE);
        }
    }

    /**
     * @brief Set pin level (convenience method)
     *
     * @param level Pin level (0 = low, 1 = high)
     * @return int Error code (0 = success, negative = error)
     */
    virtual int setLevel(uint8_t level) override {
        std::vector<uint32_t> args = {static_cast<uint32_t>(level)};
        return execute(IoFunction::SET_LEVEL, args);
    }

    /**
     * @brief Get pin level (convenience method)
     *
     * @param[out] level Pointer to store pin level
     * @return int Error code (0 = success, negative = error)
     */
    virtual int getLevel(uint8_t* level) override {
        if (level == nullptr) {
            return static_cast<int>(IoError::INVALID_ARG);
        }
        // Note: This would need to call meshx_gpio_get_level directly
        // since execute() can't return the level value
        meshx_err_t err = meshx_gpio_get_level(m_logical_pin, level);
        return (err == MESHX_SUCCESS) ? 0 : static_cast<int>(IoError::INVALID_MODE);
    }

    /**
     * @brief Toggle pin level (convenience method)
     *
     * @return int Error code (0 = success, negative = error)
     */
    virtual int toggle() override {
        std::vector<uint32_t> args;  // No arguments needed for toggle
        return execute(IoFunction::TOGGLE, args);
    }

    /**
     * @brief Set PWM duty cycle (convenience method)
     *
     * @param duty_cycle Duty cycle (0-100%)
     * @return int Error code (0 = success, negative = error)
     */
    virtual int setPwmDutyCycle(uint8_t duty_cycle) override {
        std::vector<uint32_t> args = {static_cast<uint32_t>(duty_cycle)};
        return execute(IoFunction::SET_PWM_DUTY, args);
    }

    /**
     * @brief Set PWM frequency (convenience method)
     *
     * @param frequency Frequency in Hz
     * @return int Error code (0 = success, negative = error)
     */
    virtual int setPwmFrequency(uint32_t frequency) override {
        std::vector<uint32_t> args = {frequency};
        return execute(IoFunction::SET_PWM_FREQUENCY, args);
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
                                  uint8_t trigger_type) override {
        // Convert trigger type to meshx_gpio_intr_type_t
        meshx_gpio_intr_type_t intr_type;
        switch (trigger_type) {
            case 1: intr_type = MESHX_GPIO_INTR_POSITIVE_EDGE; break;
            case 2: intr_type = MESHX_GPIO_INTR_NEGATIVE_EDGE; break;
            case 3: intr_type = MESHX_GPIO_INTR_ANY_EDGE; break;
            case 4: intr_type = MESHX_GPIO_INTR_LOW_LEVEL; break;
            case 5: intr_type = MESHX_GPIO_INTR_HIGH_LEVEL; break;
            default: return static_cast<int>(IoError::INVALID_ARG);
        }

        // Store the callback and user data for the wrapper
        m_intr_callback = callback;
        m_intr_user_data = user_data;

        // Use a static wrapper function that calls back with the logical pin
        meshx_err_t err = meshx_gpio_register_intr(m_logical_pin, intr_type,
                                                   meshx_gpio_intr_cb_wrapper, this);
        return (err == MESHX_SUCCESS) ? 0 : static_cast<int>(IoError::INTR_NOT_SUPPORTED);
    }

    /**
     * @brief Unregister interrupt (convenience method)
     *
     * @return int Error code (0 = success, negative = error)
     */
    virtual int unregisterInterrupt() override {
        std::vector<uint32_t> args;  // No arguments needed
        return execute(IoFunction::UNREGISTER_INTERRUPT, args);
    }

    /**
     * @brief Enable/disable interrupt (convenience method)
     *
     * @param enable true to enable, false to disable
     * @return int Error code (0 = success, negative = error)
     */
    virtual int enableInterrupt(bool enable) override {
        std::vector<uint32_t> args = {static_cast<uint32_t>(enable)};
        return execute(IoFunction::ENABLE_INTERRUPT, args);
    }
};

/**
 * @brief Static interrupt callback wrapper for C API
 *
 * @param logical_pin Logical pin number
 * @param user_data User data (pointer to MeshXGpioImpl instance)
 */
static void meshx_gpio_intr_cb_wrapper(uint8_t logical_pin, void *user_data)
{
    MeshXGpioImpl* instance = static_cast<MeshXGpioImpl*>(user_data);
    if (instance != nullptr && instance->m_intr_callback != nullptr) {
        instance->m_intr_callback(logical_pin, instance->m_intr_user_data);
    }
}

/**
 * @brief Factory function for creating GPIO instances
 *
 * @param logical_pin Logical pin number
 * @param name Pin name
 * @return std::unique_ptr<MeshXIoInterface> IO interface instance
 */
std::unique_ptr<MeshXIoInterface> createGpioInstance(uint8_t logical_pin, const char* name) {
    // Default to output mode for GPIO
    return std::make_unique<MeshXGpioImpl>(logical_pin, name, GpioMode::OUTPUT);
}

} // namespace meshx
