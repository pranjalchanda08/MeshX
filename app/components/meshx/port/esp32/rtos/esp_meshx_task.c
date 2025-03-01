/**
 * @file esp_meshx_task.c
 * @brief MeshX Task Implementation
 *
 * This file implements the MeshX Task.
 *
 * @author Pranjal Chanda
 */

#include "meshx_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "esp_system.h"

/**
 * @brief Create a MeshX Task
 *
 * This function creates a MeshX Task.
 *
 * @param[in,out] task_handle Task Handle
 *
 * @return meshx_err_t
 */
meshx_err_t meshx_task_create(meshx_task_t *task_handle)
{
    if (task_handle == NULL)
    {
        return MESHX_INVALID_ARG;
    }

    TaskHandle_t task_handle_temp = NULL;
    BaseType_t err = xTaskCreate(
        task_handle->task_cb,
        task_handle->task_name,
        task_handle->stack_size,
        task_handle->arg,
        task_handle->priority,
        &task_handle_temp);

    if (err != pdPASS)
    {
        return MESHX_FAIL;
    }

    task_handle->__task_handle = task_handle_temp;

    return MESHX_SUCCESS;
}

/**
 * @brief Delete a MeshX Task
 *
 * This function deletes a MeshX Task.
 *
 * @param[in] task_handle Task Handle
 */
meshx_err_t meshx_task_delete(meshx_task_t *task_handle)
{
    if (task_handle == NULL)
    {
        return MESHX_INVALID_ARG;
    }

    vTaskDelete(task_handle->__task_handle);

    return MESHX_SUCCESS;
}

/**
 * @brief Suspend a MeshX Task
 *
 * This function suspends a MeshX Task.
 *
 * @param[in] task_handle Task Handle
 */

meshx_err_t meshx_task_suspend(meshx_task_t *task_handle)
{
    if (task_handle == NULL)
    {
        return MESHX_INVALID_ARG;
    }

    vTaskSuspend(task_handle->__task_handle);

    return MESHX_SUCCESS;
}

/**
 * @brief Resume a MeshX Task
 *
 * This function resumes a MeshX Task.
 *
 * @param[in] task_handle Task Handle
 */
meshx_err_t meshx_task_resume(meshx_task_t *task_handle)
{
    if (task_handle == NULL)
    {
        return MESHX_INVALID_ARG;
    }

    vTaskResume(task_handle->__task_handle);

    return MESHX_SUCCESS;
}

/**
 * @brief Get Task Handle
 *
 * This function gets the Task Handle.
 *
 * @param[in] task_handle Task Handle
 *
 * @return meshx_err_t
 */
meshx_err_t meshx_task_get_handle(meshx_task_t *task_handle)
{
    if (task_handle == NULL)
    {
        return MESHX_INVALID_ARG;
    }

    return MESHX_SUCCESS;
}

/**
 * @brief Delay a MeshX Task
 *
 * This function delays a MeshX Task.
 *
 * @param[in] delay_ms Delay in milliseconds
 * @return meshx_err_t
 */
meshx_err_t meshx_task_delay(uint32_t delay_ms)
{
    if (delay_ms == 0)
    {
        return MESHX_INVALID_ARG;
    }

    vTaskDelay(pdMS_TO_TICKS(delay_ms));

    return MESHX_SUCCESS;
}
