/**
 * @file meshx_task.h
 * @brief MeshX Task Interface
 *
 * This file contains the MeshX Task Interface.
 *
 * @note This file should not be modified.
 *
 * @version 1.0
 *
 * @author Pranjal Chanda
 */

#ifndef __MESHX_TASK_H
#define __MESHX_TASK_H

#include <stddef.h>
#include "meshx_err.h"

/**
 * @brief MeshX Task Function
 * @param[in] arg Task Argument
 *
 * @return None
 */
typedef void (*meshx_task_cb_t)(void *arg);

/**
 * @brief MeshX Task Structure
 */
typedef struct meshx_task
{
    /* Public */
    const char *task_name;   /**< Task Name */
    void *arg;               /**< Task Argument */
    size_t stack_size;       /**< Task Stack Size */
    int priority;            /**< Task Priority */
    meshx_task_cb_t task_cb; /**< Task Callback */
    /* Private */
    void *__task_handle; /**< Task Handle */
} meshx_task_t;

/**
 * @brief Create a MeshX Task
 *
 * This function creates a MeshX Task.
 *
 * @param[in,out] task_handle Task Handle
 *
 * @return meshx_err_t
 */
meshx_err_t meshx_task_create(meshx_task_t *task_handle);

/**
 * @brief Delete a MeshX Task
 *
 * This function deletes a MeshX Task.
 *
 * @param[in] task_handle Task Handle
 */
meshx_err_t meshx_task_delete(meshx_task_t *task_handle);

/**
 * @brief Suspend a MeshX Task
 *
 * This function suspends a MeshX Task.
 *
 * @param[in] task_handle Task Handle
 */

meshx_err_t meshx_task_suspend(meshx_task_t *task_handle);

/**
 * @brief Resume a MeshX Task
 *
 * This function resumes a MeshX Task.
 *
 * @param[in] task_handle Task Handle
 */
meshx_err_t meshx_task_resume(meshx_task_t *task_handle);

/**
 * @brief Get Task Handle
 *
 * This function gets the Task Handle.
 *
 * @param[in] task_handle Task Handle
 *
 * @return meshx_err_t
 */
meshx_err_t meshx_task_get_handle(meshx_task_t *task_handle);

/**
 * @brief Delay a MeshX Task
 *
 * This function delays a MeshX Task.
 *
 * @param[in] delay_ms Delay in milliseconds
 * @return meshx_err_t
 */
meshx_err_t meshx_task_delay(uint32_t delay_ms);

#endif /* __MESHX_TASK_H */
