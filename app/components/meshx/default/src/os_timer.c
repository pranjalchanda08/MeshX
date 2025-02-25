/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file os_timer.c
 * @brief Implementation of OS timer functionalities for BLE mesh node.
 *
 * This file contains the implementation of the OS timer functionalities used in the BLE mesh node application.
 * It includes the necessary includes, definitions, and initialization of the timer registration table.
 */

#include "os_timer.h"

#define OS_TIMER_INIT_MAGIC 0x3892
 /**
 * @def OS_TIMER_CONTROL_TASK_EVT_MASK
 * @brief Mask for OS timer control task events.
 *
 * This mask includes the following events:
 * - CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_ARM
 * - CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_DISARM
 * - CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_FIRE
 */
#define OS_TIMER_CONTROL_TASK_EVT_MASK (CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_DISARM | \
                                        CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_ARM |    \
                                        CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_REARM |  \
                                        CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_PERIOD | \
                                        CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_FIRE)

 /**
 * @struct os_timer_reg_head
 * @brief Head of the singly linked list for OS timer control task message parameters.
 */
SLIST_HEAD(os_timer_reg_head, os_timer);

 /**
 * @var os_timer_reg_table_head
 * @brief Head of the OS timer registration table.
 *
 * This is initialized using the SLIST_HEAD_INITIALIZER macro.
 */
static struct os_timer_reg_head os_timer_reg_table_head = SLIST_HEAD_INITIALIZER(os_timer_reg_table_head);

#if CONFIG_ENABLE_UNIT_TEST

typedef enum
{
    OS_TIMER_CLI_CMD_CREATE,
    OS_TIMER_CLI_CMD_ARM,
    OS_TIMER_CLI_CMD_REARM,
    OS_TIMER_CLI_CMD_DISARM,
    OS_TIMER_CLI_CMD_DELETE,
    OS_TIMER_CLI_CMD_PERIOD_SET,
    OS_TIMER_CLI_CMD_MAX
} os_timer_cli_cmd_t;

/**
 * @brief OS Timer Unit Test Callback handler
 * @param[in] p_timer   Callback params
 */
static void os_timer_ut_cb_handler(const os_timer_t* p_timer)
{
    ESP_LOGI(TAG, "%s|%ld|%d", p_timer->name, p_timer->period, p_timer->reload);
}

/**
 * @brief Callback handler for OS Timer unit test command.
 *
 * This function handles OS Timer unit test command by processing the
 * provided command ID and arguments.
 *
 * @param[in] cmd_id    The command ID to be processed.
 * @param[in] argc      The number of arguments provided.
 * @param[in] argv      The array of arguments.
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - Other error codes depending on the implementation
 */
static esp_err_t os_timer_unit_test_cb_handler(int cmd_id, int argc, char **argv)
{
    esp_err_t err = ESP_OK;
    os_timer_cli_cmd_t cmd = (os_timer_cli_cmd_t) cmd_id;

    uint32_t ut_period = 0;
    bool ut_reload = false;
    static os_timer_t* ut_os_timer;

    ESP_LOGI(TAG, "argc|cmd_id: %d|%d", argc, cmd_id);
    if (cmd_id >= OS_TIMER_CLI_CMD_MAX)
    {
        ESP_LOGE(TAG, "Invalid number of arguments");
        return ESP_ERR_INVALID_ARG;
    }

    switch(cmd)
    {
        case OS_TIMER_CLI_CMD_CREATE:
            /* ut 2 0 2 [period_ms] [reload]*/
            ut_period = UT_GET_ARG(0, uint32_t, argv);
            ut_reload = UT_GET_ARG(1, uint32_t, argv) == 0 ? false : true;
            err = os_timer_create("OS_TIMER_UT", ut_period, ut_reload, os_timer_ut_cb_handler, &ut_os_timer);
            break;
        case OS_TIMER_CLI_CMD_ARM:
            /* ut 2 1 0 */
            err = os_timer_start(ut_os_timer);
            break;
        case OS_TIMER_CLI_CMD_REARM:
            /* ut 2 2 0 */
            err = os_timer_restart(ut_os_timer);
            break;
        case OS_TIMER_CLI_CMD_DISARM:
            /* ut 2 3 0 */
            err = os_timer_stop(ut_os_timer);
            break;
        case OS_TIMER_CLI_CMD_DELETE:
            /* ut 2 4 0 */
            err = os_timer_delete(&ut_os_timer);
            break;
        case OS_TIMER_CLI_CMD_PERIOD_SET:
            /* ut 2 5 1 [new period ms] */
            ut_period =  UT_GET_ARG(0, uint32_t, argv);
            err = os_timer_set_period(ut_os_timer, ut_period);
            break;
        default:
            break;
    }
    if(err)
        ESP_LOGE(TAG, "err: 0x%x", err);

    return err;
}

#endif /* CONFIG_ENABLE_UNIT_TEST */

/*
 * @brief Callback function for the OS timer to control task.
 *
 * This function is called whenever a OS timer timeout occurs.
 *
 * @param timer_handle  Timer haandle callback param
 */
static void os_timer_fire_cb(const os_timer_handle_t timer_handle)
{
    os_timer_t *msg_params;
    SLIST_FOREACH(msg_params, &os_timer_reg_table_head, next)
    {
        if (msg_params->timer_handle == timer_handle)
        {
            control_task_msg_publish(
                CONTROL_TASK_MSG_CODE_SYSTEM,
                CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_FIRE,
                msg_params,
                OS_TIMER_SIZE);
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
    os_timer_t *msg_params = (os_timer_t *)params;

    switch (evt)
    {
        case CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_ARM:
            ESP_LOGD(TAG, "Starting timer %s", OS_TMER_GET_TIMER_NAME(msg_params));
            if (xTimerStart(msg_params->timer_handle, 0) != pdPASS)
            {
                return ESP_FAIL;
            }
            break;
        case CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_REARM:
            ESP_LOGD(TAG, "Rearming timer %s", OS_TMER_GET_TIMER_NAME(msg_params));
            if (xTimerReset(msg_params->timer_handle, 0) != pdPASS)
            {
                return ESP_FAIL;
            }
            break;
        case CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_DISARM:
            ESP_LOGD(TAG, "Stopping timer %s", OS_TMER_GET_TIMER_NAME(msg_params));
            if (xTimerStop(msg_params->timer_handle, 0) != pdPASS)
            {
                return ESP_FAIL;
            }
            break;
        case CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_PERIOD:
            ESP_LOGD(TAG, "Timer %s period set: %ld", OS_TMER_GET_TIMER_NAME(msg_params), msg_params->period);
            if (xTimerChangePeriod(msg_params->timer_handle, pdMS_TO_TICKS(msg_params->period), 0) != pdPASS)
            {
                return ESP_FAIL;
            }
            break;
        case CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_FIRE:
            ESP_LOGD(TAG, "Timer %s fire", OS_TMER_GET_TIMER_NAME(msg_params));
            /* call respective callback */
            if (msg_params->cb)
                msg_params->cb(msg_params);
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
    esp_err_t err;
#if CONFIG_ENABLE_UNIT_TEST
    err = register_unit_test(MODULE_ID_COMPONENT_OS_TIMER, &os_timer_unit_test_cb_handler);
    if (err)
    {
        ESP_LOGE(TAG, "unit_test reg failed: (%d)", err);
        return err;
    }
#endif /* CONFIG_ENABLE_UNIT_TEST */
    err = control_task_msg_subscribe(
        CONTROL_TASK_MSG_CODE_SYSTEM,
        OS_TIMER_CONTROL_TASK_EVT_MASK,
        (control_task_msg_handle_t)&os_timer_control_task_cb);

    return err;
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
 * @param timer_handle  Timer handle return
 *
 * @return The timer handle on success, or NULL on failure.
 */
esp_err_t os_timer_create(
    const char *name,
    uint32_t period,
    bool reload,
    os_timer_cb_t cb,
    os_timer_t **timer_handle)
{
    if (timer_handle == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if((*timer_handle) != NULL && (*timer_handle)->init == OS_TIMER_INIT_MAGIC)
        return ESP_ERR_INVALID_STATE;

    esp_err_t err = ESP_OK;

    *timer_handle = (os_timer_t *) malloc(OS_TIMER_SIZE);
    if (*timer_handle == NULL)
        return ESP_ERR_NO_MEM;

    (*timer_handle)->cb = cb;
    (*timer_handle)->name = name;
    (*timer_handle)->period = period;
    (*timer_handle)->reload = reload;

    (*timer_handle)->timer_handle = xTimerCreate(
        (*timer_handle)->name,
        pdMS_TO_TICKS((*timer_handle)->period),
        (*timer_handle)->reload,
        NULL,
        os_timer_fire_cb
    );
    if (NULL == (*timer_handle)->timer_handle)
    {
        free(*timer_handle);
        return ESP_ERR_NO_MEM;
    }

    SLIST_INSERT_HEAD(&os_timer_reg_table_head, (*timer_handle), next);
    (*timer_handle)->init = OS_TIMER_INIT_MAGIC;

    return err;
}

/**
 * @brief Start a timer.
 *
 * This function starts the given timer.
 *
 * @param timer_handle The timer handle.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t os_timer_start(const os_timer_t *timer_handle)
{
    if (timer_handle == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }
    if (timer_handle->init != OS_TIMER_INIT_MAGIC)
        return ESP_ERR_INVALID_STATE;

    return control_task_msg_publish(
        CONTROL_TASK_MSG_CODE_SYSTEM,
        CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_ARM,
        timer_handle,
        OS_TIMER_SIZE
    );
}

/**
 * @brief Restart a timer.
 *
 * This function re-starts the given timer.
 *
 * @param timer_handle The timer handle.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t os_timer_restart(const os_timer_t *timer_handle)
{
    if (timer_handle == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }
    if (timer_handle->init != OS_TIMER_INIT_MAGIC)
        return ESP_ERR_INVALID_STATE;

    return control_task_msg_publish(
        CONTROL_TASK_MSG_CODE_SYSTEM,
        CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_REARM,
        timer_handle,
        OS_TIMER_SIZE
    );
}

/**
 * @brief Set period on an initialised timer.
 *
 * This function reset period of initialised timer.
 *
 * @param timer_handle  The timer handle.
 * @param period_ms     New period in ms
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t os_timer_set_period(os_timer_t *timer_handle, const uint32_t period_ms)
{
    if (timer_handle == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }
    if (timer_handle->init != OS_TIMER_INIT_MAGIC)
        return ESP_ERR_INVALID_STATE;

    timer_handle->period = period_ms;

    return control_task_msg_publish(
        CONTROL_TASK_MSG_CODE_SYSTEM,
        CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_PERIOD,
        timer_handle,
        OS_TIMER_SIZE
    );
}

/**
 * @brief Stop a timer.
 *
 * This function stops the given timer.
 *
 * @param timer_handle The timer handle.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t os_timer_stop(const os_timer_t *timer_handle)
{
    if (timer_handle == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }
    if (timer_handle->init != OS_TIMER_INIT_MAGIC)
        return ESP_ERR_INVALID_STATE;

    return control_task_msg_publish(
        CONTROL_TASK_MSG_CODE_SYSTEM,
        CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_DISARM,
        timer_handle,
        OS_TIMER_SIZE
    );
}

/**
 * @brief Delete a timer.
 *
 * This function deletes the given timer.
 *
 * @param timer_handle The timer handle.
 *
 * @return ESP_OK on success, or an error code on failure.
 */

esp_err_t os_timer_delete(os_timer_t **timer_handle)
{
    if (timer_handle == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (*timer_handle == NULL || (*timer_handle)->init != OS_TIMER_INIT_MAGIC)
        return ESP_ERR_INVALID_STATE;

    ESP_LOGI(TAG, "Deleting timer %s", OS_TMER_GET_TIMER_NAME((*timer_handle)));
    if (xTimerDelete((*timer_handle)->timer_handle, 0) != pdPASS)
    {
        return ESP_FAIL;
    }
    (*timer_handle)->init = 0;

    SLIST_REMOVE(&os_timer_reg_table_head, *timer_handle, os_timer, next);
    free(*timer_handle);

    return ESP_OK;
}
