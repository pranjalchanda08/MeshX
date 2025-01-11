/**
 * @file os_timer.h
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
#include "control_task.h"
#include "freertos/timers.h"
#include "sys/queue.h"

/**
 * @brief return timer registered name pointer
 */
#define OS_TMER_GET_TIMER_NAME(timer) (timer->name)

/**
 * @typedef os_timer_t
 * @brief Alias for the FreeRTOS TimerHandle_t type.
 *
 * This typedef provides a more convenient name for the FreeRTOS
 * timer handle type, used for creating and managing timers.
 */
typedef TimerHandle_t os_timer_t;

/**
 * @brief Timer callback function prototype.
 *
 * This function is called when the timer expires.
 *
 * @param timer_id The timer handle.
 */
typedef void (*os_timer_cb_t)(os_timer_t timer_id);

/**
 * @brief Structure to hold parameters for the OS timer control task message.
 *
 * This structure contains the parameters required to configure and control
 * an OS timer. It includes options for setting the timer to reload, specifying
 * the timer period, providing a name for the timer, and assigning a callback
 * function to be executed when the timer expires.
 */
typedef struct os_timer_reg_list
{
    bool reload;
    uint32_t period;
    os_timer_cb_t cb;
    const char *name;
    os_timer_t timer_handle;
    SLIST_ENTRY(os_timer_reg_list) next;
} os_timer_reg_list_t;

/**
 * @brief Initialize the OS timer module.
 *
 * This function initializes the OS timer module.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t os_timer_init(void);

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
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t os_timer_create(const char *name, uint32_t period, bool reload, os_timer_cb_t cb, os_timer_t *timer_handle);

/**
 * @brief Start a timer.
 *
 * This function starts the given timer.
 *
 * @param timer_id The timer handle.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t os_timer_start(os_timer_t timer_id);

/**
 * @brief Stop a timer.
 *
 * This function stops the given timer.
 *
 * @param timer_id The timer handle.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t os_timer_stop(os_timer_t timer_id);

/**
 * @brief Delete a timer.
 *
 * This function deletes the given timer.
 *
 * @param timer_id The timer handle.
 *
 * @return ESP_OK on success, or an error code on failure.
 */

esp_err_t os_timer_delete(os_timer_t timer_id);

#endif /* __OS_TIMER_H__ */
