/**
 * @file meshx_io_types.hpp
 * @brief MeshX IO Type Definitions
 *
 * This file defines data structures and types for IO configuration.
 *
 * @author MeshX Team
 * @date 2024
 */

#ifndef __MESHX_IO_TYPES_HPP
#define __MESHX_IO_TYPES_HPP

#include <cstdint>
#include <cstddef>

namespace meshx {

/**
 * @brief GPIO Mode enumeration
 */
enum class GpioMode {
    INPUT = 0,                     /**< Input only mode */
    OUTPUT,                        /**< Output only mode */
    INPUT_OUTPUT,                  /**< Input and output mode */
    OPEN_DRAIN,                    /**< Open drain output mode */
    OPEN_DRAIN_INPUT_OUTPUT,       /**< Open drain input/output mode */
    PWM_OUTPUT,                    /**< PWM output mode */
    MODE_MAX                       /**< Maximum mode value */
};

/**
 * @brief GPIO Pull Resistor enumeration
 */
enum class GpioPull {
    NONE = 0,                      /**< No pull resistor */
    PULL_UP,                       /**< Pull-up resistor */
    PULL_DOWN,                     /**< Pull-down resistor */
    PULL_UP_DOWN,                  /**< Both pull-up and pull-down */
    PULL_MAX                       /**< Maximum pull value */
};

/**
 * @brief GPIO Drive Strength enumeration
 */
enum class GpioDrive {
    WEAK = 0,                      /**< Weak drive strength */
    MEDIUM,                        /**< Medium drive strength */
    STRONG,                        /**< Strong drive strength */
    MAX_STRONG,                    /**< Maximum strong drive strength */
    DRIVE_MAX                      /**< Maximum drive value */
};

/**
 * @brief GPIO Interrupt Trigger enumeration
 */
enum class GpioIntrTrigger {
    DISABLED = 0,                  /**< Interrupt disabled */
    POSITIVE_EDGE,                 /**< Positive edge trigger */
    NEGATIVE_EDGE,                 /**< Negative edge trigger */
    ANY_EDGE,                      /**< Any edge trigger */
    LOW_LEVEL,                     /**< Low level trigger */
    HIGH_LEVEL,                    /**< High level trigger */
    INTR_MAX                       /**< Maximum interrupt type */
};

/**
 * @brief Hosted Mode enumeration
 */
enum class HostedMode {
    NON_HOSTED = 0,                /**< Non-hosted mode (direct IO access) */
    HOSTED,                        /**< Hosted mode (UART transport) */
    TRANSITIONING                  /**< Mode transition in progress */
};

/**
 * @brief IO Error Codes
 */
enum class IoError {
    SUCCESS = 0,                   /**< Success */
    INVALID_PIN,                   /**< Invalid pin number */
    INVALID_MODE,                  /**< Invalid mode for operation */
    INVALID_LEVEL,                 /**< Invalid level value */
    INVALID_ARG,                   /**< Invalid argument */
    INTR_NOT_SUPPORTED,            /**< Interrupt not supported */
    INTR_ALREADY_REGISTERED,       /**< Interrupt already registered */
    PWM_NOT_SUPPORTED,             /**< PWM not supported */
    PWM_INVALID_PARAM,             /**< Invalid PWM parameter */
    NOT_SUPPORTED,                 /**< Operation not supported */
    NOT_INITIALIZED,               /**< IO subsystem not initialized */
    CONFIG_INVALID,                /**< Invalid configuration */
    HOSTED_MODE,                   /**< Operation not allowed in hosted mode */
    KV_STORAGE,                    /**< KV Engine storage error */
    SERIALIZATION,                 /**< Configuration serialization error */
    ERROR_MAX                      /**< Maximum error value */
};

/**
 * @brief Convert IO error to string
 *
 * @param error IO error code
 * @return const char* Error string
 */
const char* ioErrorToString(IoError error);

/**
 * @brief Convert GPIO mode to string
 *
 * @param mode GPIO mode
 * @return const char* Mode string
 */
const char* gpioModeToString(GpioMode mode);

/**
 * @brief Convert string to GPIO mode
 *
 * @param str Mode string
 * @return GpioMode GPIO mode
 */
GpioMode stringToGpioMode(const char* str);

/**
 * @brief Convert GPIO pull to string
 *
 * @param pull GPIO pull setting
 * @return const char* Pull string
 */
const char* gpioPullToString(GpioPull pull);

/**
 * @brief Convert string to GPIO pull
 *
 * @param str Pull string
 * @return GpioPull GPIO pull setting
 */
GpioPull stringToGpioPull(const char* str);

/**
 * @brief Convert GPIO drive to string
 *
 * @param drive GPIO drive strength
 * @return const char* Drive string
 */
const char* gpioDriveToString(GpioDrive drive);

/**
 * @brief Convert string to GPIO drive
 *
 * @param str Drive string
 * @return GpioDrive GPIO drive strength
 */
GpioDrive stringToGpioDrive(const char* str);

/**
 * @brief Convert interrupt trigger to string
 *
 * @param trigger Interrupt trigger type
 * @return const char* Trigger string
 */
const char* intrTriggerToString(GpioIntrTrigger trigger);

/**
 * @brief Convert string to interrupt trigger
 *
 * @param str Trigger string
 * @return GpioIntrTrigger Interrupt trigger type
 */
GpioIntrTrigger stringToIntrTrigger(const char* str);

} // namespace meshx

#endif /* __MESHX_IO_TYPES_HPP */
