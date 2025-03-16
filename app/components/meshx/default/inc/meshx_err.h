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

#define MESHX_UNUSED(x) ((void)(x))

#define MESHX_DO_NOTHING    do {} while(0)

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
    MESHX_ERR_MAX        /**< Maximum Error */
} meshx_err_t;

#endif /* __MESHX_ERR_H__ */
