/**
 * Copyright (c) 2024 - 2025 MeshX
 *
 * @file meshx_rtos_timer.h
 * @brief Header file for MeshX RTOS Timer interface.
 *        Provides APIs for creating, starting, stopping, deleting,
 *        and managing RTOS timers in the MeshX framework.
 *
 * This module abstracts the RTOS timer functionalities and provides
 * a unified interface for timer management, including support for
 * auto-reload timers, callback functions, and dynamic period changes.
 *
 * @author Pranjal Chanda
 *
 */

#ifndef __MESHX_RTOS_TIMER_H
#define __MESHX_RTOS_TIMER_H

#include <stdint.h>
#include <stdbool.h>
#include "meshx_err.h"

typedef void (*meshx_rtos_timer_callback_t)(void *);

typedef struct meshx_rtos_timer
{
    /* Public */
    void *timer_arg;
    bool auto_reload;
    uint32_t timer_period;
    const char *timer_name;
    meshx_rtos_timer_callback_t timer_cb;
    /* Private */
    void *__timer_handle;
}meshx_rtos_timer_t;

/**
 * @brief Creates a new RTOS timer.
 *
 * This function initializes and creates a new RTOS timer with the specified parameters.
 *
 * @param[out] timer    Pointer to the meshx_rtos_timer_t structure to be initialized.
 * @param[in] name      Timer Name String
 * @param[in] cb        Callback function to be invoked when the timer expires.
 * @param[in] arg       Argument to be passed to the callback function.
 * @param[in] period_ms Timer period in milliseconds.
 * @param[in] reload    Flag set if timer is a auto reload type.
 *
 * @return meshx_err_t Returns MESHX_SUCCESS if the timer was created successfully,
 *                     or an error code if the creation failed.
 */
meshx_err_t meshx_rtos_timer_create(meshx_rtos_timer_t *timer, const char * name, meshx_rtos_timer_callback_t cb, void *arg, uint32_t period_ms, bool reload);
/**
 * @brief Starts the RTOS timer.
 *
 * This function starts the RTOS timer.
 *
 * @param[in] timer Pointer to the meshx_rtos_timer_t structure.
 *
 * @return meshx_err_t Returns MESHX_SUCCESS if the timer was started successfully,
 *                     or an error code if the start operation failed.
 */
meshx_err_t meshx_rtos_timer_start(meshx_rtos_timer_t *timer);

/**
 * @brief Stops the RTOS timer.
 *
 * This function stops the RTOS timer.
 *
 * @param[in] timer Pointer to the meshx_rtos_timer_t structure.
 *
 * @return meshx_err_t Returns MESHX_SUCCESS if the timer was stopped successfully,
 *                     or an error code if the stop operation failed.
 */
meshx_err_t meshx_rtos_timer_stop(meshx_rtos_timer_t *timer);

/**
 * @brief Deletes the RTOS timer.
 *
 * This function deletes the RTOS timer and frees associated resources.
 *
 * @param[in] timer Pointer to the meshx_rtos_timer_t structure.
 *
 * @return meshx_err_t Returns MESHX_SUCCESS if the timer was deleted successfully,
 *                     or an error code if the delete operation failed.
 */
meshx_err_t meshx_rtos_timer_delete(meshx_rtos_timer_t *timer);

/**
 * @brief Changes the period of the RTOS timer.
 *
 * This function changes the period of an active or dormant RTOS timer.
 *
 * @param[in] timer Pointer to the meshx_rtos_timer_t structure.
 * @param[in] new_period_ms New timer period in milliseconds.
 *
 * @return meshx_err_t Returns MESHX_SUCCESS if the timer period was changed successfully,
 *                     or an error code if the change operation failed.
 */
meshx_err_t meshx_rtos_timer_change_period(meshx_rtos_timer_t *timer, uint32_t new_period_ms);

/**
 * @brief Resets the RTOS timer.
 *
 * This function resets the RTOS timer, causing it to restart from its beginning.
 *
 * @param[in] timer Pointer to the meshx_rtos_timer_t structure.
 *
 * @return meshx_err_t Returns MESHX_SUCCESS if the timer was reset successfully,
 *                     or an error code if the reset operation failed.
 */
meshx_err_t meshx_rtos_timer_reset(meshx_rtos_timer_t *timer);

/*
 * @brief Callback function for the OS timer to control task.
 * @note This function is called internally not to be called by user.
 *
 * This function is called whenever a OS timer timeout occurs.
 *
 * @param timer_handle  Timer haandle callback param
 */
void meshx_os_timer_fire_cb(const void* timer_handle);

#endif /* __MESHX_RTOS_TIMER_H */
