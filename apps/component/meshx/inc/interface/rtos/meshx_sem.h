/**
 * @file meshx_sem.h
 * @brief MeshX Semaphore Interface
 *
 * This file contains the MeshX Semaphore Interface.
 *
 * @author Pranjal Chanda
 * @version 1.0
 */

#ifndef __MESHX_SEM_H
#define __MESHX_SEM_H

#include <stdint.h>
#include "meshx_err.h"

/**
 * @brief MeshX Semaphore Structure
 */
typedef struct meshx_sem
{
    /* Public */
    const char *sem_name; /**< Semaphore Name */
    int max_count;        /**< Maximum Count */
    int init_count;       /**< Initial Count */
    /* Private */
    void *__sem_handle; /**< Semaphore Handle */
} meshx_sem_t;

/**
 * @brief Create a MeshX Semaphore
 *
 * This function creates a MeshX Semaphore.
 *
 * @param[in,out] sem_handle Semaphore Handle
 *
 * @return meshx_err_t
 */
meshx_err_t meshx_sem_create(meshx_sem_t *sem_handle);

/**
 * @brief Delete a MeshX Semaphore
 *
 * This function deletes a MeshX Semaphore.
 *
 * @param[in] sem_handle Semaphore Handle
 * @return meshx_err_t
 */
meshx_err_t meshx_sem_delete(meshx_sem_t *sem_handle);

/**
 * @brief Take a MeshX Semaphore
 *
 * This function takes a MeshX Semaphore.
 *
 * @param[in] sem_handle Semaphore Handle
 * @param[in] delay_ms Delay in milliseconds
 *
 * @return meshx_err_t
 */
meshx_err_t meshx_sem_take(meshx_sem_t *sem_handle, uint32_t delay_ms);

/**
 * @brief Give a MeshX Semaphore
 *
 * This function gives a MeshX Semaphore.
 *
 * @param[in] sem_handle Semaphore Handle
 *
 * @return meshx_err_t 
 */
meshx_err_t meshx_sem_give(meshx_sem_t *sem_handle);

#endif /* __MESHX_SEM_H */
