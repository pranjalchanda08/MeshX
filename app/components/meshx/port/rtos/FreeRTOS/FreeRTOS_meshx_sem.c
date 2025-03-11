/**
 * @file FreeRTOS_meshx_sem.c
 * @brief MeshX Semaphore Implementation
 *
 * This file implements the MeshX Semaphore.
 *
 * @author Pranjal Chanda
 */

#include "meshx_sem.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_system.h"

/**
 * @brief Create a MeshX Semaphore
 *
 * This function creates a MeshX Semaphore.
 *
 * @param[in,out] sem_handle Semaphore Handle
 *
 * @return meshx_err_t
 */
meshx_err_t meshx_sem_create(meshx_sem_t *sem_handle)
{
    if (sem_handle == NULL)
    {
        return MESHX_INVALID_ARG;
    }

    sem_handle->__sem_handle = xSemaphoreCreateCounting(sem_handle->max_count, sem_handle->init_count);

    if (sem_handle->__sem_handle == NULL)
    {
        return MESHX_FAIL;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Delete a MeshX Semaphore
 *
 * This function deletes a MeshX Semaphore.
 *
 * @param[in] sem_handle Semaphore Handle
 * @return meshx_err_t
 */
meshx_err_t meshx_sem_delete(meshx_sem_t *sem_handle)
{
    if (sem_handle == NULL)
    {
        return MESHX_INVALID_ARG;
    }

    vSemaphoreDelete(sem_handle->__sem_handle);

    return MESHX_SUCCESS;
}

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
meshx_err_t meshx_sem_take(meshx_sem_t *sem_handle, uint32_t delay_ms)
{
    if (sem_handle == NULL)
    {
        return MESHX_INVALID_ARG;
    }

    BaseType_t err = xPortInIsrContext() ? xSemaphoreTakeFromISR(sem_handle->__sem_handle, NULL) : xSemaphoreTake(sem_handle->__sem_handle, delay_ms);
    if(err != pdPASS)
    {
        return MESHX_FAIL;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Give a MeshX Semaphore
 *
 * This function gives a MeshX Semaphore.
 *
 * @param[in] sem_handle Semaphore Handle
 *
 * @return meshx_err_t
 */
meshx_err_t meshx_sem_give(meshx_sem_t *sem_handle)
{
    if (sem_handle == NULL)
    {
        return MESHX_INVALID_ARG;
    }

    if (xSemaphoreGive(sem_handle->__sem_handle) != pdTRUE)
    {
        return MESHX_FAIL;
    }

    return MESHX_SUCCESS;
}

