/**
 * @file meshx_rtos_utils.h
 * @brief Utility functions for RTOS operations in the MeshX framework.
 *
 * This header file provides a set of utility functions for interacting with
 * the RTOS in the MeshX framework. These utilities include functions for
 * retrieving system time, memory allocation and deallocation, and querying
 * the amount of free heap memory.
 *
 * The functions in this file are designed to simplify common RTOS operations
 * and ensure consistent error handling across the MeshX framework.
 *
 * @author Pranjal Chanda
 */

#ifndef __MESHX_RTOS_UTILS_H__
#define __MESHX_RTOS_UTILS_H__

#include "meshx_err.h"
#include "stddef.h"

/**
 * @brief Get the system time in milliseconds.
 *
 * This function retrieves the current system time in milliseconds since
 *
 * @param[out] millis Pointer to store the current system time in milliseconds.
 *
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
meshx_err_t meshx_rtos_get_sys_time(unsigned int *millis);

/**
 * @brief Allocate memory using FreeRTOS.
 *
 * This function allocates memory of the specified size using FreeRTOS.
 *
 * @param[out] ptr Pointer to the allocated memory.
 * @param[in] size Size of memory to allocate.
 *
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
meshx_err_t meshx_rtos_malloc(void** ptr, size_t size);

/**
 * @brief Allocate memory using FreeRTOS and initialize it to zero.
 *
 * @param[out] ptr Pointer to the allocated memory.
 * @param[in] num Number of elements to allocate.
 * @param[in] size Size of each element.
 *
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
meshx_err_t meshx_rtos_calloc(void **ptr, size_t num, size_t size);

/**
 * @brief Free memory allocated by FreeRTOS.
 *
 * This function frees the memory allocated by FreeRTOS and sets the pointer to NULL.
 *
 * @param[in,out] ptr Pointer to the memory to free.
 *
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
meshx_err_t meshx_rtos_free(void** ptr);

/**
 * @brief Get the amount of free heap memory.
 *
 * This function retrieves the amount of free heap memory available in the system.
 *
 * @return Size of free heap memory in bytes.
 */
size_t meshx_rtos_get_free_heap(void);

#endif /* __MESHX_RTOS_UTILS_H__ */

