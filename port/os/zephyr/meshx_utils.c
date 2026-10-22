/**
 * Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_utils.c
 * @brief Utility functions for integrating MeshX with Zephyr RTOS.
 *        This file provides implementations for memory management,
 *        system time retrieval, and task query using Zephyr APIs.
 *
 * @author Pranjal Chanda
 */

#include "interface/rtos/meshx_rtos_utils.h"
#include <zephyr/kernel.h>
#include <string.h>

/**
 * @brief Retrieves the current system time in milliseconds.
 *
 * @param[out] millis Pointer to unsigned int where system time (ms) is stored.
 * @return meshx_err_t
 */
meshx_err_t meshx_rtos_get_sys_time(unsigned int *millis)
{
    if (millis == NULL) {
        return MESHX_INVALID_ARG;
    }

    *millis = (unsigned int)k_uptime_get_32();
    return MESHX_SUCCESS;
}

/**
 * @brief Allocates memory dynamically using Zephyr k_malloc.
 *
 * @param[out] ptr Pointer to allocated memory destination pointer.
 * @param[in] size Size in bytes.
 * @return meshx_err_t
 */
meshx_err_t meshx_rtos_malloc(void **ptr, size_t size)
{
    if (!ptr || size == 0) {
        return MESHX_INVALID_ARG;
    }

    *ptr = k_malloc(size);
    if (*ptr == NULL) {
        return MESHX_NO_MEM;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Allocates zero-initialized memory using Zephyr k_calloc.
 *
 * @param[out] ptr Pointer to allocated memory destination.
 * @param[in] num Number of elements.
 * @param[in] size Size of each element.
 * @return meshx_err_t
 */
meshx_err_t meshx_rtos_calloc(void **ptr, size_t num, size_t size)
{
    if (!ptr || num == 0 || size == 0) {
        return MESHX_INVALID_ARG;
    }

    *ptr = k_calloc(num, size);
    if (*ptr == NULL) {
        return MESHX_NO_MEM;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Frees memory and sets pointer to NULL.
 *
 * @param[in,out] ptr Pointer to memory pointer to free.
 * @return meshx_err_t
 */
meshx_err_t meshx_rtos_free(void **ptr)
{
    if (ptr && *ptr) {
        k_free(*ptr);
        *ptr = NULL;
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Retrieves estimated free heap memory.
 *
 * @return size_t Free heap size in bytes.
 */
size_t meshx_rtos_get_free_heap(void)
{
#if defined(CONFIG_HEAP_MEM_POOL_SIZE)
    return (size_t)CONFIG_HEAP_MEM_POOL_SIZE;
#else
    return 65536;
#endif
}

/**
 * @brief Retrieves current task ID.
 *
 * @param[out] task_id Pointer to unsigned int where task ID is stored.
 * @return meshx_err_t
 */
meshx_err_t meshx_rtos_get_curr_task_id_prio(unsigned int *task_id)
{
    if (task_id == NULL) {
        return MESHX_INVALID_ARG;
    }

    *task_id = (unsigned int)(uintptr_t)k_current_get();
    return MESHX_SUCCESS;
}
