/**
 * Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_task.c
 * @brief Implementation of MeshX task management functions using Zephyr RTOS.
 *        This file provides APIs to create, delete, suspend, resume, and delay tasks
 *        in the MeshX framework for Zephyr targets.
 *
 * @author Pranjal Chanda
 */

#include "interface/rtos/meshx_task.h"
#include <zephyr/kernel.h>
#include <stdlib.h>

/**
 * @brief Internal wrapper structure for dynamically allocated Zephyr thread
 */
typedef struct {
    struct k_thread thread_data;
    k_thread_stack_t *stack;
    size_t stack_size;
    meshx_task_cb_t task_cb;
    void *arg;
} zephyr_task_control_block_t;

/**
 * @brief Zephyr thread entry point adaptor
 */
static void zephyr_thread_wrapper(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    zephyr_task_control_block_t *tcb = (zephyr_task_control_block_t *)p1;
    if (tcb != NULL && tcb->task_cb != NULL) {
        tcb->task_cb(tcb->arg);
    }
}

/**
 * @brief Create a MeshX Task on Zephyr
 *
 * @param[in,out] task_handle Task Handle structure
 * @return meshx_err_t MESHX_SUCCESS on success, error code otherwise
 */
meshx_err_t meshx_task_create(meshx_task_t *task_handle)
{
    if (task_handle == NULL || task_handle->task_cb == NULL || task_handle->stack_size == 0) {
        return MESHX_INVALID_ARG;
    }

    zephyr_task_control_block_t *tcb = (zephyr_task_control_block_t *)k_malloc(sizeof(zephyr_task_control_block_t));
    if (tcb == NULL) {
        return MESHX_NO_MEM;
    }

    tcb->task_cb = task_handle->task_cb;
    tcb->arg = task_handle->arg;

    /* Allocate aligned thread stack buffer */
    tcb->stack_size = task_handle->stack_size;
    tcb->stack = (k_thread_stack_t *)k_aligned_alloc(ARCH_STACK_PTR_ALIGN, tcb->stack_size);
    if (tcb->stack == NULL) {
        k_free(tcb);
        return MESHX_NO_MEM;
    }

    /* Map FreeRTOS/MeshX priority (higher value = higher priority)
     * to Zephyr preemptible priority (lower value = higher priority).
     * Zephyr defines K_LOWEST_APPLICATION_THREAD_PRIO = CONFIG_NUM_PREEMPT_PRIORITIES - 1.
     */
    int zephyr_prio = K_LOWEST_APPLICATION_THREAD_PRIO - task_handle->priority;
    if (zephyr_prio < 0) {
        zephyr_prio = 0; /* Highest preemptible priority */
    } else if (zephyr_prio > K_LOWEST_APPLICATION_THREAD_PRIO) {
        zephyr_prio = K_LOWEST_APPLICATION_THREAD_PRIO; /* Lowest preemptible priority */
    }

    k_tid_t tid = k_thread_create(
        &tcb->thread_data,
        tcb->stack,
        tcb->stack_size,
        zephyr_thread_wrapper,
        tcb,
        NULL,
        NULL,
        zephyr_prio,
        0,
        K_NO_WAIT
    );

    if (tid == NULL) {
        k_free(tcb->stack);
        k_free(tcb);
        return MESHX_FAIL;
    }

    if (task_handle->task_name != NULL) {
        k_thread_name_set(tid, task_handle->task_name);
    }

    task_handle->__task_handle = (void *)tcb;
    return MESHX_SUCCESS;
}

/**
 * @brief Delete a MeshX Task
 *
 * @param[in] task_handle Task Handle
 * @return meshx_err_t
 */
meshx_err_t meshx_task_delete(meshx_task_t *task_handle)
{
    if (task_handle == NULL || task_handle->__task_handle == NULL) {
        return MESHX_INVALID_ARG;
    }

    zephyr_task_control_block_t *tcb = (zephyr_task_control_block_t *)task_handle->__task_handle;
    k_thread_abort(&tcb->thread_data);

    if (tcb->stack != NULL) {
        k_free(tcb->stack);
        tcb->stack = NULL;
    }
    k_free(tcb);
    task_handle->__task_handle = NULL;

    return MESHX_SUCCESS;
}

/**
 * @brief Suspend a MeshX Task
 *
 * @param[in] task_handle Task Handle
 * @return meshx_err_t
 */
meshx_err_t meshx_task_suspend(meshx_task_t *task_handle)
{
    if (task_handle == NULL || task_handle->__task_handle == NULL) {
        return MESHX_INVALID_ARG;
    }

    zephyr_task_control_block_t *tcb = (zephyr_task_control_block_t *)task_handle->__task_handle;
    k_thread_suspend(&tcb->thread_data);

    return MESHX_SUCCESS;
}

/**
 * @brief Resume a MeshX Task
 *
 * @param[in] task_handle Task Handle
 * @return meshx_err_t
 */
meshx_err_t meshx_task_resume(meshx_task_t *task_handle)
{
    if (task_handle == NULL || task_handle->__task_handle == NULL) {
        return MESHX_INVALID_ARG;
    }

    zephyr_task_control_block_t *tcb = (zephyr_task_control_block_t *)task_handle->__task_handle;
    k_thread_resume(&tcb->thread_data);

    return MESHX_SUCCESS;
}

/**
 * @brief Get Task Handle
 *
 * @param[in] task_handle Task Handle
 * @return meshx_err_t
 */
meshx_err_t meshx_task_get_handle(meshx_task_t *task_handle)
{
    if (task_handle == NULL) {
        return MESHX_INVALID_ARG;
    }
    return MESHX_SUCCESS;
}

/**
 * @brief Delay a MeshX Task
 *
 * @param[in] delay_ms Delay in milliseconds
 * @return meshx_err_t
 */
meshx_err_t meshx_task_delay(uint32_t delay_ms)
{
    if (delay_ms == 0) {
        return MESHX_INVALID_ARG;
    }

    k_msleep(delay_ms);
    return MESHX_SUCCESS;
}
