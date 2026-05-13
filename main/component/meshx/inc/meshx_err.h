/**
 * @file meshx_err.h
 * @brief MeshX Error Codes
 *
 * This file contains the MeshX Error Codes.
 *
 * @author Pranjal Chanda
 */

#ifndef __MESHX_ERR_H__
#define __MESHX_ERR_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MESHX_UNUSED(x) ((void)(x))

#define MESHX_DO_NOTHING    do {} while(0)

#define MESHX_WEEK __attribute__((weak))

/* Compiler based definations */

#ifndef MESHX_MALLOC
#define MESHX_MALLOC    malloc
#endif

#ifndef MESHX_CALOC
#define MESHX_CALOC     calloc
#endif

#ifndef MESHX_FREE
#define MESHX_FREE      free
#endif

/**
 * @brief MeshX Error Codes
 */
typedef enum
{
    MESHX_SUCCESS = 0,   /**< Success */
    MESHX_FAIL,          /**< Failure */
    MESHX_INVALID_ARG,   /**< Invalid Argument */
    MESHX_ERR_PLAT,      /**< Platform Error */
    MESHX_NO_MEM,        /**< No Memory */
    MESHX_INVALID_STATE, /**< Invalid State */
    MESHX_NOT_FOUND,     /**< Not Found */
    MESHX_NOT_SUPPORTED, /**< Not Supported */
    MESHX_TIMEOUT,       /**< Timeout */
    MESHX_ERR_NOT_INIT,  /**< Not Initialized */

    /* GPIO Error Codes (Base: 0x5000) */
    MESHX_ERR_GPIO_BASE = 0x5000,          /**< GPIO error base */
    MESHX_ERR_GPIO_INVALID_PIN,            /**< Invalid GPIO pin number */
    MESHX_ERR_GPIO_INVALID_MODE,           /**< Invalid GPIO mode for operation */
    MESHX_ERR_GPIO_INVALID_LEVEL,          /**< Invalid GPIO level value */
    MESHX_ERR_GPIO_INTR_NOT_SUPPORTED,     /**< Interrupt not supported on pin */
    MESHX_ERR_GPIO_INTR_ALREADY_REGISTERED,/**< Interrupt already registered */
    MESHX_ERR_GPIO_PWM_NOT_SUPPORTED,      /**< PWM not supported on pin */
    MESHX_ERR_GPIO_PWM_INVALID_PARAM,      /**< Invalid PWM parameter */
    MESHX_ERR_GPIO_NOT_INITIALIZED,        /**< GPIO subsystem not initialized */
    MESHX_ERR_GPIO_CONFIG_INVALID,         /**< Invalid GPIO configuration */
    MESHX_ERR_GPIO_HOSTED_MODE,            /**< Operation not allowed in hosted mode */
    MESHX_ERR_GPIO_KV_STORAGE,             /**< KV Engine storage error */
    MESHX_ERR_GPIO_SERIALIZATION,          /**< Configuration serialization error */

    MESHX_ERR_MAX        /**< Maximum Error */
} meshx_err_t;

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_ERR_H__ */
