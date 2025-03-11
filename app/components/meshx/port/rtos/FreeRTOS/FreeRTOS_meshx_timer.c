#include "meshx_rtos_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

static void timer_callback(TimerHandle_t xTimer)
{
    meshx_rtos_timer_t *timer = (meshx_rtos_timer_t *)pvTimerGetTimerID(xTimer);
    if (timer->timer_cb != NULL) {
        timer->timer_cb(timer->timer_arg);
    }
}

/**
 * @brief Creates a new RTOS timer.
 *
 * This function initializes and creates a new RTOS timer with the specified parameters.
 *
 * @param[out] timer Pointer to the meshx_rtos_timer_t structure to be initialized.
 * @param[in] name  Timer Name String
 * @param[in] cb Callback function to be invoked when the timer expires.
 * @param[in] arg Argument to be passed to the callback function.
 * @param[in] period_ms Timer period in milliseconds.
 *
 * @return meshx_err_t Returns MESHX_SUCCESS if the timer was created successfully,
 *                     or an error code if the creation failed.
 */
meshx_err_t meshx_rtos_timer_create(meshx_rtos_timer_t *timer, const char * name, meshx_rtos_timer_callback_t cb, void *arg, uint32_t period_ms)
{
    if (timer == NULL || cb == NULL) {
        return MESHX_INVALID_ARG;
    }

    timer->timer_name = name;
    timer->timer_cb = cb;
    timer->timer_arg = arg;
    timer->timer_period = period_ms;

    timer->__timer_handle = xTimerCreate(
        timer->timer_name,
        pdMS_TO_TICKS(period_ms),
        pdTRUE,  // Auto reload
        (void *)timer,
        timer_callback
    );

    if (timer->__timer_handle == NULL) {
        return MESHX_NO_MEM;
    }

    return MESHX_SUCCESS;
}

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
meshx_err_t meshx_rtos_timer_start(meshx_rtos_timer_t *timer)
{
    if (timer == NULL || timer->__timer_handle == NULL) {
        return MESHX_INVALID_ARG;
    }

    if (xTimerStart(timer->__timer_handle, 0) != pdPASS) {
        return MESHX_FAIL;
    }

    return MESHX_SUCCESS;
}

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
meshx_err_t meshx_rtos_timer_stop(meshx_rtos_timer_t *timer)
{
    if (timer == NULL || timer->__timer_handle == NULL) {
        return MESHX_INVALID_ARG;
    }

    if (xTimerStop(timer->__timer_handle, 0) != pdPASS) {
        return MESHX_FAIL;
    }

    return MESHX_SUCCESS;
}

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
meshx_err_t meshx_rtos_timer_delete(meshx_rtos_timer_t *timer)
{
    if (timer == NULL || timer->__timer_handle == NULL) {
        return MESHX_INVALID_ARG;
    }

    if (xTimerDelete(timer->__timer_handle, 0) != pdPASS) {
        return MESHX_FAIL;
    }

    timer->__timer_handle = NULL;
    return MESHX_SUCCESS;
}

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
meshx_err_t meshx_rtos_timer_change_period(meshx_rtos_timer_t *timer, uint32_t new_period_ms)
{
    if (timer == NULL || timer->__timer_handle == NULL) {
        return MESHX_INVALID_ARG;
    }

    if (xTimerChangePeriod(timer->__timer_handle, pdMS_TO_TICKS(new_period_ms), 0) != pdPASS) {
        return MESHX_FAIL;
    }

    timer->timer_period = new_period_ms;
    return MESHX_SUCCESS;
}

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
meshx_err_t meshx_rtos_timer_reset(meshx_rtos_timer_t *timer)
{
    if (timer == NULL || timer->__timer_handle == NULL) {
        return MESHX_INVALID_ARG;
    }

    if (xTimerReset(timer->__timer_handle, 0) != pdPASS) {
        return MESHX_FAIL;
    }

    return MESHX_SUCCESS;
}
