/**
 * @file os_timer.c
 * @brief Implementation of OS timer functionalities for BLE mesh node.
 *
 * This file contains the implementation of the OS timer functionalities used in the BLE mesh node application.
 * It includes the necessary includes, definitions, and initialization of the timer registration table.
 *
 * @author Pranjal Chanda
 */

#include "os_timer.h"

 /**
 * @def OS_TIMER_CONTROL_TASK_EVT_MASK
 * @brief Mask for OS timer control task events.
 *
 * This mask includes the following events:
 * - CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_ARM
 * - CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_DISARM
 * - CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_DELETE
 * - CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_FIRE
 */
#define OS_TIMER_CONTROL_TASK_EVT_MASK (CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_DISARM | \
                                        CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_DELETE | \
                                        CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_ARM |    \
                                        CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_FIRE)

 /**
 * @struct os_timer_reg_head
 * @brief Head of the singly linked list for OS timer control task message parameters.
 */
SLIST_HEAD(os_timer_reg_head, os_timer_reg_list);

 /**
 * @var os_timer_reg_table_head
 * @brief Head of the OS timer registration table.
 *
 * This is initialized using the SLIST_HEAD_INITIALIZER macro.
 */
static struct os_timer_reg_head os_timer_reg_table_head = SLIST_HEAD_INITIALIZER(os_timer_reg_table_head);

/*
 * @brief Callback function for the OS timer to control task.
 *
 * This function is called whenever a OS timer timeout occurs.
 *
 * @param pdev      Pointer to the device structure.
 * @param evt       Event code.
 * @param params    Pointer to the event parameters.
 * @return ESP_OK on success, or an error code on failure.
 */
static void os_timer_fire_cb(const os_timer_t timer_handle)
{
    os_timer_reg_list_t *msg_params;
    SLIST_FOREACH(msg_params, &os_timer_reg_table_head, next)
    {
        if (msg_params->timer_handle == timer_handle)
        {
            control_task_publish(
                CONTROL_TASK_MSG_CODE_SYSTEM,
                CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_FIRE,
                msg_params,
                sizeof(os_timer_reg_list_t));
            break;
        }
    }
}
/*
 * @brief Callback function for the OS timer control task.
 *
 * This function is called whenever a control task message is received.
 *
 * @param pdev      Pointer to the device structure.
 * @param evt       Event code.
 * @param params    Pointer to the event parameters.
 * @return ESP_OK on success, or an error code on failure.
 */
static esp_err_t os_timer_control_task_cb(const dev_struct_t *pdev, control_task_msg_evt_t evt, void *params)
{
    os_timer_reg_list_t *msg_params = (os_timer_reg_list_t *)params;

    switch (evt)
    {
    case CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_ARM:
        ESP_LOGI(TAG, "Starting timer %s", OS_TMER_GET_TIMER_NAME(msg_params));
        if (xTimerStart(msg_params->timer_handle, 0) != pdPASS)
        {
            return ESP_FAIL;
        }
        break;
    case CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_DISARM:
        ESP_LOGI(TAG, "Stopping timer %s", OS_TMER_GET_TIMER_NAME(msg_params));
        if (xTimerStop(msg_params->timer_handle, 0) != pdPASS)
        {
            return ESP_FAIL;
        }
        break;
    case CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_DELETE:
        ESP_LOGI(TAG, "Deleting timer %s", OS_TMER_GET_TIMER_NAME(msg_params));
        if (xTimerDelete(msg_params->timer_handle, 0) != pdPASS)
        {
            return ESP_FAIL;
        }
        SLIST_REMOVE(&os_timer_reg_table_head, msg_params, os_timer_reg_list, next);
        free(msg_params);
        break;
    case CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_FIRE:
        ESP_LOGI(TAG, "Timer %s fire", OS_TMER_GET_TIMER_NAME(msg_params));
        /* call respective callback */
        if (msg_params->cb)
            msg_params->cb(msg_params->timer_handle);
        break;
    default:
        break;
    }

    ESP_UNUSED(pdev);

    return ESP_OK;
}

/**
 * @brief Initialize the OS timer module.
 *
 * This function initializes the OS timer module.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t os_timer_init(void)
{
    return control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_SYSTEM,
        OS_TIMER_CONTROL_TASK_EVT_MASK,
        (control_task_msg_handle_t)&os_timer_control_task_cb);
}

/**
 * @brief Create a timer.
 *
 * This function creates a timer with the given period and callback function.
 *
 * @param name      The name of the timer.
 * @param period    The period of the timer in milliseconds.
 * @param reload    If true, the timer will automatically reload after expiring.
 * @param cb        The callback function to be called when the timer expires.
 *
 * @return The timer handle on success, or NULL on failure.
 */
esp_err_t os_timer_create(const char *name, uint32_t period, bool reload, os_timer_cb_t cb, os_timer_t *timer_handle)
{
    esp_err_t err = ESP_OK;
    os_timer_reg_list_t params = {
        .cb     = cb,
        .name   = name,
        .reload = reload,
        .period = period,
    };

    params.timer_handle = xTimerCreate(params.name, pdMS_TO_TICKS(params.period), params.reload, NULL, os_timer_fire_cb);
    if (NULL == params.timer_handle)
        return ESP_ERR_NO_MEM;

    os_timer_reg_list_t *new_node = (os_timer_reg_list_t *) malloc(sizeof(os_timer_reg_list_t));
    if (new_node == NULL)
        return ESP_ERR_NO_MEM;

    *new_node = params;
    SLIST_INSERT_HEAD(&os_timer_reg_table_head, new_node, next);

    *timer_handle = params.timer_handle;
    return err;
}

/**
 * @brief Start a timer.
 *
 * This function starts the given timer.
 *
 * @param timer_id The timer handle.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t os_timer_start(os_timer_t timer_id)
{
    return control_task_publish(
        CONTROL_TASK_MSG_CODE_SYSTEM,
        CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_ARM,
        &timer_id,
        sizeof(timer_id)
    );
}

/**
 * @brief Stop a timer.
 *
 * This function stops the given timer.
 *
 * @param timer_id The timer handle.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t os_timer_stop(os_timer_t timer_id)
{
    return control_task_publish(
        CONTROL_TASK_MSG_CODE_SYSTEM,
        CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_DISARM,
        &timer_id,
        sizeof(timer_id)
    );
}

/**
 * @brief Delete a timer.
 *
 * This function deletes the given timer.
 *
 * @param timer_id The timer handle.
 *
 * @return ESP_OK on success, or an error code on failure.
 */

esp_err_t os_timer_delete(os_timer_t timer_id)
{
    return control_task_publish(
        CONTROL_TASK_MSG_CODE_SYSTEM,
        CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_DELETE,
        &timer_id,
        sizeof(timer_id)
    );
}
