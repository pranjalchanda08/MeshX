/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_os_timer.h
 * @brief Header file for OS timer utilities.
 *
 * This file contains the definitions and includes necessary for
 * working with OS timers in the ESP32 BLE mesh node application.
 *
 * @author Pranjal Chanda
 */

#ifndef __OS_TIMER_H__
#define __OS_TIMER_H__

#include <stdint.h>
#include "sys/queue.h"
#include "meshx_control_task.h"
#include "interface/rtos/meshx_rtos_timer.h"

/**
 * @brief return meshx_os_timer_t size
 */
#define OS_TIMER_SIZE sizeof(meshx_os_timer_t)

/**
 * @brief return timer registered name pointer
 */
#define OS_TMER_GET_TIMER_NAME(timer) (timer->timer_handle.timer_name)

/**
 * @typedef meshx_os_timer_handle_t
 * @brief Alias for the meshx_rtos_timer_t type.
 *
 * This typedef provides a more convenient name for the FreeRTOS
 * timer handle type, used for creating and managing timers.
 */
typedef meshx_rtos_timer_t meshx_os_timer_handle_t;

/**
 * @typedef meshx_os_timer_t
 * @brief Alias for the meshx_os_timer structure.
 *
 * This typedef provides a more convenient name for the meshx_os_timer
 * structure, which holds the parameters for the OS timer control task message.
 */
typedef struct meshx_os_timer meshx_os_timer_t;
/**
 * @brief Timer callback function prototype.
 *
 * This function is called when the timer expires.
 *
 * @param[in] p_timer The timer handle.
 */
typedef void (*meshx_os_timer_cb_t)(const meshx_os_timer_t* p_timer);

/**
 * @brief Structure to hold parameters for the OS timer control task message.
 *
 * This structure contains the parameters required to configure and control
 * an OS timer. It includes options for setting the timer to reload, specifying
 * the timer period, providing a name for the timer, and assigning a callback
 * function to be executed when the timer expires.
 */
struct meshx_os_timer
{
    uint16_t init;
    uint32_t period;
    meshx_os_timer_cb_t cb;
    meshx_os_timer_handle_t timer_handle;
    SLIST_ENTRY(meshx_os_timer) next;
};

/**
 * @brief Initialize the OS timer module.
 *
 * This function initializes the OS timer module.
 *
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
meshx_err_t meshx_os_timer_init(void);

/**
 * @brief Create a timer.
 *
 * This function creates a timer with the given period and callback function.
 *
 * @param[in] name          The name of the timer.
 * @param[in] period        The period of the timer in milliseconds.
 * @param[in] reload        If true, the timer will automatically reload after expiring.
 * @param[in] cb            The callback function to be called when the timer expires.
 * @param[inout] timer_handle  The timer handle.
 *
 * Example:
 * ```c
 *  meshx_os_timer_t * meshx_os_timer_inst;
 *  meshx_err_t err = meshx_os_timer_create("Example_Timer", 1000, 1, &example_os_timer_cb, &os_timer_inst);
 * ```
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
meshx_err_t meshx_os_timer_create(const char *name, uint32_t period, bool reload, meshx_os_timer_cb_t cb, meshx_os_timer_t **timer_handle);

/**
 * @brief Start a timer.
 *
 * This function starts the given timer.
 *
 * @param timer_handle The timer handle.
 *
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
meshx_err_t meshx_os_timer_start(const meshx_os_timer_t *timer_handle);

/**
 * @brief Restart a timer.
 *
 * This function re-starts the given timer.
 *
 * @param timer_handle The timer handle.
 *
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
meshx_err_t meshx_os_timer_restart(const meshx_os_timer_t *timer_handle);

/**
 * @brief Set period on an initialised timer.
 *
 * This function reset period of initialised timer.
 *
 * @param timer_handle  The timer handle.
 * @param period_ms     New period in ms
 *
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
meshx_err_t meshx_os_timer_set_period(meshx_os_timer_t *timer_handle, const uint32_t period_ms);

/**
 * @brief Stop a timer.
 *
 * This function stops the given timer.
 *
 * @param timer_handle The timer handle.
 *
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
meshx_err_t meshx_os_timer_stop(const meshx_os_timer_t *timer_handle);

/**
 * @brief Delete a timer.
 *
 * This function deletes the given timer.
 *
 * @param timer_handle The timer handle.
 *
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */

meshx_err_t meshx_os_timer_delete(meshx_os_timer_t **timer_handle);

#endif /* __OS_TIMER_H__ */
