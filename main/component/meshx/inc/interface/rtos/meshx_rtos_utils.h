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
 * @brief Retrieves the current system time in milliseconds.
 *
 * This function calculates the system time in milliseconds based on the
 * FreeRTOS tick count and the configured tick rate. The result is stored
 * in the variable pointed to by the `millis` parameter.
 *
 * @param[out] millis Pointer to an unsigned integer where the system time
 *                    in milliseconds will be stored.
 *
 * @return
 * - MESHX_SUCCESS: If the system time was successfully retrieved.
 *
 * @note Ensure that the `millis` pointer is valid and not NULL before
 *       calling this function.
 */
meshx_err_t meshx_rtos_get_sys_time(unsigned int *millis);

/**
 * @brief Allocates memory dynamically in a thread-safe manner using FreeRTOS.
 *
 * This function wraps the memory allocation process to ensure compatibility
 * with the FreeRTOS environment. It allocates a block of memory of the specified
 * size and assigns the pointer to the provided pointer variable.
 *
 * @param[out] ptr Pointer to the memory location where the allocated memory address
 *                 will be stored. Must not be NULL.
 * @param[in] size The size of the memory block to allocate, in bytes.
 *
 * @return
 *     - MESHX_SUCCESS on successful memory allocation.
 *     - MESHX_ERR_NO_MEM if memory allocation fails.
 *     - Other error codes as defined in the meshx_err_t enumeration.
 */
meshx_err_t meshx_rtos_malloc(void** ptr, size_t size);

/**
 * @brief Allocates memory for an array of elements and initializes it to zero.
 *
 * This function allocates memory for an array of `num` elements, each of size `size`,
 * and initializes all bytes in the allocated memory to zero. The allocated memory
 * pointer is returned via the `ptr` parameter.
 *
 * @param[out] ptr Pointer to the allocated memory. This will be set to NULL if the allocation fails.
 * @param[in]  num Number of elements to allocate.
 * @param[in]  size Size of each element in bytes.
 *
 * @return
 *     - MESHX_SUCCESS: Memory allocation was successful.
 *     - MESHX_ERR_NO_MEM: Memory allocation failed due to insufficient memory.
 */
meshx_err_t meshx_rtos_calloc(void **ptr, size_t num, size_t size);

/**
 * @brief Frees memory allocated to a pointer and sets it to NULL.
 *
 * This function is used to safely deallocate memory that was previously
 * allocated and ensures that the pointer is set to NULL to avoid dangling
 * pointer issues.
 *
 * @param[in,out] ptr A double pointer to the memory to be freed. After the
 *                     memory is freed, the pointer is set to NULL.
 *
 * @return
 *     - MESHX_SUCCESS on successful deallocation.
 *     - MESHX_ERR_INVALID_ARG if the provided pointer is NULL or invalid.
 *     - Other error codes depending on the implementation.
 */
meshx_err_t meshx_rtos_free(void** ptr);

/**
 * @brief Retrieves the amount of free heap memory available in the system.
 *
 * This function is used to query the current amount of free heap memory
 * available in the system. It is useful for monitoring memory usage and
 * ensuring that the system has sufficient resources for dynamic memory
 * allocation.
 *
 * @return size_t The amount of free heap memory in bytes.
 */
size_t meshx_rtos_get_free_heap(void);

/**
 * @brief Retrieves the current task ID.
 *
 * This function retrieves the current task ID using FreeRTOS APIs.
 * The task ID is stored in the variable pointed to by the `task_id` parameter.
 *
 * @param[out] task_id Pointer to an unsigned integer where the task ID will be stored.
 *
 * @return
 * - MESHX_SUCCESS: If the task ID was successfully retrieved.
 * - MESHX_ERR_INVALID_ARG: If the provided pointer is NULL or invalid.
 *
 * @note Ensure that the `task_id` pointer is valid and not NULL before
 *       calling this function.
 */
meshx_err_t meshx_rtos_get_curr_task_id_prio(unsigned int *task_id);

#endif /* __MESHX_RTOS_UTILS_H__ */

