/**
 * @file meshx_pwm_impl.cpp
 * @brief MeshX PWM Concrete Implementation
 *
 * This file implements the concrete PWM class that inherits from
 * MeshXIoInterface and provides PWM operations.
 *
 * @author MeshX Team
 * @date 2024
 */

#include "../inc/meshx_io_interface.hpp"
#include "../inc/meshx_io_types.hpp"
#include "../../interface/gpio/meshx_pwm.h"
#include "../../interface/gpio/meshx_gpio_types.h"
#include "../../inc/meshx_err.h"
#include <cstring>
#include <vector>

namespace meshx {

/**
 * @brief Concrete PWM Implementation Class
 */
class MeshXPwmImpl : public MeshXIoInterface {
private:
    uint8_t m_logical_pin;                 /**< Logical pin number */
    char m_name[32];                       /**< Pin name */
    bool m_initialized;                    /**< Initialization flag */
    bool m_started;                        /**< PWM started flag */
    uint32_t m_frequency;                  /**< Current frequency */
    uint8_t m_duty_cycle;                  /**< Current duty cycle */

public:
    /**
     * @brief Constructor
     *
     * @param logical_pin Logical pin number
     * @param name Pin name
     */
    MeshXPwmImpl(uint8_t logical_pin, const char* name)
        : m_logical_pin(logical_pin), m_initialized(false),
          m_started(false), m_frequency(1000), m_duty_cycle(50) {
        strncpy(m_name, name, sizeof(m_name) - 1);
        m_name[sizeof(m_name) - 1] = '\0';
    }

    /**
     * @brief Destructor
     */
    virtual ~MeshXPwmImpl() {
        if (m_started) {
            // Stop PWM if it's running
            meshx_pwm_stop(m_logical_pin);
        }
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
                // PWM pins don't support direct level control
                return false;

            case IoFunction::SET_PWM_DUTY:
            case IoFunction::SET_PWM_FREQUENCY:
                // PWM-specific functions
                return true;

            case IoFunction::REGISTER_INTERRUPT:
            case IoFunction::UNREGISTER_INTERRUPT:
            case IoFunction::ENABLE_INTERRUPT:
                // PWM pins don't support interrupts
                return false;

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
            // Initialize PWM if not already initialized
            meshx_err_t err = meshx_pwm_init();
            if (err != MESHX_SUCCESS) {
                return static_cast<int>(IoError::NOT_INITIALIZED);
            }
            m_initialized = true;
        }

        switch (function) {
            case IoFunction::SET_PWM_DUTY: {
                if (args.size() < 1) {
                    return static_cast<int>(IoError::PWM_INVALID_PARAM);
                }
                uint8_t duty_cycle = static_cast<uint8_t>(args[0]);
                if (duty_cycle > 100) {
                    return static_cast<int>(IoError::PWM_INVALID_PARAM);
                }

                meshx_err_t err = meshx_pwm_set_duty_cycle(m_logical_pin, duty_cycle);
                if (err == MESHX_SUCCESS) {
                    m_duty_cycle = duty_cycle;
                    return 0;
                }
                return static_cast<int>(IoError::PWM_NOT_SUPPORTED);
            }

            case IoFunction::SET_PWM_FREQUENCY: {
                if (args.size() < 1) {
                    return static_cast<int>(IoError::PWM_INVALID_PARAM);
                }
                uint32_t frequency = args[0];
                if (frequency == 0 || frequency > 1000000) { // 1MHz max
                    return static_cast<int>(IoError::PWM_INVALID_PARAM);
                }

                meshx_err_t err = meshx_pwm_set_frequency(m_logical_pin, frequency);
                if (err == MESHX_SUCCESS) {
                    m_frequency = frequency;
                    return 0;
                }
                return static_cast<int>(IoError::PWM_NOT_SUPPORTED);
            }

            case IoFunction::SET_LEVEL:
            case IoFunction::GET_LEVEL:
            case IoFunction::TOGGLE:
                // PWM pins don't support direct level control
                return static_cast<int>(IoError::INVALID_MODE);

            case IoFunction::REGISTER_INTERRUPT:
            case IoFunction::UNREGISTER_INTERRUPT:
            case IoFunction::ENABLE_INTERRUPT:
                // PWM pins don't support interrupts
                return static_cast<int>(IoError::INTR_NOT_SUPPORTED);

            case IoFunction::CUSTOM_FUNCTION: {
                // Custom functions not implemented in this basic version
                return static_cast<int>(IoError::NOT_SUPPORTED);
            }

            default:
                return static_cast<int>(IoError::INVALID_MODE);
        }
    }

    // Note: setLevel, getLevel, toggle are still overridden to return INVALID_MODE
    // and avoid the overhead of execute() for non-supported basic IO on PWM pins.

    /**
     * @brief Set pin level (convenience method)
     *
     * @param level Pin level (0 = low, 1 = high)
     * @return int Error code (0 = success, negative = error)
     */
    virtual int setLevel(uint8_t level) override {
        // PWM pins don't support direct level control
        return static_cast<int>(IoError::INVALID_MODE);
    }

    /**
     * @brief Get pin level (convenience method)
     *
     * @param[out] level Pointer to store pin level
     * @return int Error code (0 = success, negative = error)
     */
    virtual int getLevel(uint8_t* level) override {
        // PWM pins don't support direct level control
        return static_cast<int>(IoError::INVALID_MODE);
    }

    /**
     * @brief Toggle pin level (convenience method)
     *
     * @return int Error code (0 = success, negative = error)
     */
    virtual int toggle() override {
        // PWM pins don't support direct level control
        return static_cast<int>(IoError::INVALID_MODE);
    }


    /**
     * @brief Start PWM output
     *
     * @return int Error code (0 = success, negative = error)
     */
    int start() {
        if (!m_initialized) {
            meshx_err_t err = meshx_pwm_init();
            if (err != MESHX_SUCCESS) {
                return static_cast<int>(IoError::NOT_INITIALIZED);
            }
            m_initialized = true;
        }

        meshx_err_t err = meshx_pwm_start(m_logical_pin);
        if (err == MESHX_SUCCESS) {
            m_started = true;
            return 0;
        }
        return static_cast<int>(IoError::PWM_NOT_SUPPORTED);
    }

    /**
     * @brief Stop PWM output
     *
     * @return int Error code (0 = success, negative = error)
     */
    int stop() {
        if (!m_started) {
            return 0;  // Not an error if not started
        }

        meshx_err_t err = meshx_pwm_stop(m_logical_pin);
        if (err == MESHX_SUCCESS) {
            m_started = false;
            return 0;
        }
        return static_cast<int>(IoError::PWM_NOT_SUPPORTED);
    }

    /**
     * @brief Check if PWM is started
     *
     * @return true if PWM is started
     * @return false if PWM is not started
     */
    bool isStarted() const {
        return m_started;
    }

    /**
     * @brief Get current frequency
     *
     * @return uint32_t Current frequency in Hz
     */
    uint32_t getFrequency() const {
        return m_frequency;
    }

    /**
     * @brief Get current duty cycle
     *
     * @return uint8_t Current duty cycle (0-100%)
     */
    uint8_t getDutyCycle() const {
        return m_duty_cycle;
    }
};

/**
 * @brief Factory function for creating PWM instances
 *
 * @param logical_pin Logical pin number
 * @param name Pin name
 * @return std::unique_ptr<MeshXIoInterface> IO interface instance
 */
std::unique_ptr<MeshXIoInterface> createPwmInstance(uint8_t logical_pin, const char* name) {
    return std::make_unique<MeshXPwmImpl>(logical_pin, name);
}

} // namespace meshx
