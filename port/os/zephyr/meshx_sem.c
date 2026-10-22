/**
 * Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_sem.c
 * @brief Implementation of MeshX Semaphore APIs using Zephyr RTOS k_sem.
 *        This file provides functions to create, delete, take, and give semaphores
 *        for synchronization in the MeshX framework on Zephyr.
 *
 * @author Pranjal Chanda
 */

#include "interface/rtos/meshx_sem.h"
#include <zephyr/kernel.h>
#include <stdlib.h>

/**
 * @brief Create a MeshX Semaphore on Zephyr
 *
 * @param[in,out] sem_handle Semaphore Handle
 * @return meshx_err_t
 */
meshx_err_t meshx_sem_create(meshx_sem_t *sem_handle)
{
    if (sem_handle == NULL || sem_handle->max_count <= 0 || sem_handle->init_count < 0) {
        return MESHX_INVALID_ARG;
    }

    struct k_sem *sem = (struct k_sem *)k_malloc(sizeof(struct k_sem));
    if (sem == NULL) {
        return MESHX_NO_MEM;
    }

    int ret = k_sem_init(sem, (unsigned int)sem_handle->init_count, (unsigned int)sem_handle->max_count);
    if (ret != 0) {
        k_free(sem);
        return MESHX_FAIL;
    }

    sem_handle->__sem_handle = (void *)sem;
    return MESHX_SUCCESS;
}

/**
 * @brief Delete a MeshX Semaphore
 *
 * @param[in] sem_handle Semaphore Handle
 * @return meshx_err_t
 */
meshx_err_t meshx_sem_delete(meshx_sem_t *sem_handle)
{
    if (sem_handle == NULL || sem_handle->__sem_handle == NULL) {
        return MESHX_INVALID_ARG;
    }

    struct k_sem *sem = (struct k_sem *)sem_handle->__sem_handle;
    k_free(sem);
    sem_handle->__sem_handle = NULL;

    return MESHX_SUCCESS;
}

/**
 * @brief Take a MeshX Semaphore
 *
 * @param[in] sem_handle Semaphore Handle
 * @param[in] delay_ms Delay in milliseconds
 * @return meshx_err_t
 */
meshx_err_t meshx_sem_take(meshx_sem_t *sem_handle, uint32_t delay_ms)
{
    if (sem_handle == NULL || sem_handle->__sem_handle == NULL) {
        return MESHX_INVALID_ARG;
    }

    struct k_sem *sem = (struct k_sem *)sem_handle->__sem_handle;
    k_timeout_t timeout = k_is_in_isr() ? K_NO_WAIT : ((delay_ms == UINT32_MAX) ? K_FOREVER : K_MSEC(delay_ms));

    int ret = k_sem_take(sem, timeout);
    if (ret != 0) {
        return MESHX_FAIL;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Give a MeshX Semaphore
 *
 * @param[in] sem_handle Semaphore Handle
 * @return meshx_err_t
 */
meshx_err_t meshx_sem_give(meshx_sem_t *sem_handle)
{
    if (sem_handle == NULL || sem_handle->__sem_handle == NULL) {
        return MESHX_INVALID_ARG;
    }

    struct k_sem *sem = (struct k_sem *)sem_handle->__sem_handle;
    k_sem_give(sem);

    return MESHX_SUCCESS;
}
