/**
 * Copyright (c) 2024 - 2026 MeshX
 *
 * @file meshx_timer.c
 * @brief Implementation of RTOS timer abstraction for MeshX using Zephyr RTOS k_timer.
 *        This file provides functions to create, start, stop, delete, reset,
 *        and modify RTOS timers in the MeshX framework on Zephyr targets.
 *
 * @author Pranjal Chanda
 */

#include "interface/rtos/meshx_rtos_timer.h"
#include <zephyr/kernel.h>
#include <stdlib.h>

/**
 * @brief Zephyr timer expiry handler
 *
 * @param timer_id Pointer to the Zephyr k_timer instance
 */
static void zephyr_timer_expiry(struct k_timer *timer_id)
{
    meshx_os_timer_fire_cb((const void *)timer_id);
}

/**
 * @brief Creates a new RTOS timer on Zephyr.
 *
 * @param[out] timer    Pointer to the meshx_rtos_timer_t structure to be initialized.
 * @param[in] name      Timer Name String
 * @param[in] cb        Callback function to be invoked when the timer expires.
 * @param[in] arg       Argument to be passed to the callback function.
 * @param[in] period_ms Timer period in milliseconds.
 * @param[in] reload    Flag set if timer is an auto-reload type.
 * @return meshx_err_t Returns MESHX_SUCCESS if created, error code otherwise.
 */
meshx_err_t meshx_rtos_timer_create(meshx_rtos_timer_t *timer, const char *name, meshx_rtos_timer_callback_t cb, void *arg, uint32_t period_ms, bool reload)
{
    if (timer == NULL || cb == NULL || period_ms == 0) {
        return MESHX_INVALID_ARG;
    }

    struct k_timer *ktimer = (struct k_timer *)k_malloc(sizeof(struct k_timer));
    if (ktimer == NULL) {
        return MESHX_NO_MEM;
    }

    timer->timer_name = name;
    timer->timer_cb = cb;
    timer->timer_arg = arg;
    timer->timer_period = period_ms;
    timer->auto_reload = reload;

    k_timer_init(ktimer, zephyr_timer_expiry, NULL);
    k_timer_user_data_set(ktimer, (void *)timer);

    timer->__timer_handle = (void *)ktimer;
    return MESHX_SUCCESS;
}

/**
 * @brief Starts the RTOS timer.
 *
 * @param[in] timer Pointer to the meshx_rtos_timer_t structure.
 * @return meshx_err_t
 */
meshx_err_t meshx_rtos_timer_start(meshx_rtos_timer_t *timer)
{
    if (timer == NULL || timer->__timer_handle == NULL) {
        return MESHX_INVALID_ARG;
    }

    struct k_timer *ktimer = (struct k_timer *)timer->__timer_handle;
    k_timeout_t period = timer->auto_reload ? K_MSEC(timer->timer_period) : K_NO_WAIT;

    k_timer_start(ktimer, K_MSEC(timer->timer_period), period);
    return MESHX_SUCCESS;
}

/**
 * @brief Stops the RTOS timer.
 *
 * @param[in] timer Pointer to the meshx_rtos_timer_t structure.
 * @return meshx_err_t
 */
meshx_err_t meshx_rtos_timer_stop(meshx_rtos_timer_t *timer)
{
    if (timer == NULL || timer->__timer_handle == NULL) {
        return MESHX_INVALID_ARG;
    }

    struct k_timer *ktimer = (struct k_timer *)timer->__timer_handle;
    k_timer_stop(ktimer);
    return MESHX_SUCCESS;
}

/**
 * @brief Deletes the RTOS timer.
 *
 * @param[in] timer Pointer to the meshx_rtos_timer_t structure.
 * @return meshx_err_t
 */
meshx_err_t meshx_rtos_timer_delete(meshx_rtos_timer_t *timer)
{
    if (timer == NULL || timer->__timer_handle == NULL) {
        return MESHX_INVALID_ARG;
    }

    struct k_timer *ktimer = (struct k_timer *)timer->__timer_handle;
    k_timer_stop(ktimer);
    k_free(ktimer);
    timer->__timer_handle = NULL;

    return MESHX_SUCCESS;
}

/**
 * @brief Changes the period of the RTOS timer.
 *
 * @param[in] timer Pointer to the meshx_rtos_timer_t structure.
 * @param[in] new_period_ms New timer period in milliseconds.
 * @return meshx_err_t
 */
meshx_err_t meshx_rtos_timer_change_period(meshx_rtos_timer_t *timer, uint32_t new_period_ms)
{
    if (timer == NULL || timer->__timer_handle == NULL || new_period_ms == 0) {
        return MESHX_INVALID_ARG;
    }

    struct k_timer *ktimer = (struct k_timer *)timer->__timer_handle;
    timer->timer_period = new_period_ms;
    k_timeout_t period = timer->auto_reload ? K_MSEC(new_period_ms) : K_NO_WAIT;

    k_timer_start(ktimer, K_MSEC(new_period_ms), period);
    return MESHX_SUCCESS;
}

/**
 * @brief Resets the RTOS timer.
 *
 * @param[in] timer Pointer to the meshx_rtos_timer_t structure.
 * @return meshx_err_t
 */
meshx_err_t meshx_rtos_timer_reset(meshx_rtos_timer_t *timer)
{
    if (timer == NULL || timer->__timer_handle == NULL) {
        return MESHX_INVALID_ARG;
    }

    return meshx_rtos_timer_start(timer);
}
