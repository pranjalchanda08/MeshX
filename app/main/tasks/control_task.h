#ifndef __CONTROL_TASK_H__
#define __CONTROL_TASK_H__

#include "stdio.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#define CONFIG_CONTROL_TASK_NAME "control_task"

#ifndef CONFIG_CONTROL_TASK_PRIO
#define CONFIG_CONTROL_TASK_PRIO configTIMER_TASK_PRIORITY + 1
#endif

#ifndef CONFIG_CONTROL_TASK_STACK_SIZE
#define CONFIG_CONTROL_TASK_STACK_SIZE 2048
#endif

#ifndef CONFIG_CONTROL_TASK_QUEUE_LEN
#define CONFIG_CONTROL_TASK_QUEUE_LEN 10
#endif

typedef enum PACKED_ATTR
{
    CONTROL_TASK_MSG_CODE_TO_HAL,
    CONTROL_TASK_MSG_CODE_SYSTEM,
    CONTROL_TASK_MSG_CODE_TO_BLE,
} control_task_msg_code_t;

typedef uint32_t control_task_msg_evt_t;

enum PACKED_ATTR
{
    /* To HAL Codes */
    CONTROL_TASK_MSG_EVT_TO_HAL_SET_ON_OFF         = (1 << 0),
    CONTROL_TASK_MSG_EVT_TO_HAL_SET_TEMP           = (1 << 1),
    CONTROL_TASK_MSG_EVT_TO_HAL_SET_LIGHTNESS      = (1 << 2),
    CONTROL_TASK_MSG_EVT_TO_HAL_MAX,
};

enum PACKED_ATTR
{
    /* To BLE Codes*/
    CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF         = (1 << 0),
    CONTROL_TASK_MSG_EVT_TO_BLE_SET_TEMP           = (1 << 1),
    CONTROL_TASK_MSG_EVT_TO_BLE_SET_LIGHTNESS      = (1 << 2),
    CONTROL_TASK_MSG_EVT_TO_BLE_MAX

};

typedef void (*control_task_msg_handle_t)(control_task_msg_evt_t evt, void* params);

typedef struct control_task_msg
{
    control_task_msg_code_t msg_code;
    control_task_msg_evt_t msg_evt;
    void*  msg_evt_params;
} control_task_msg_t;

esp_err_t create_control_task(void);
esp_err_t control_task_send_msg(control_task_msg_code_t msg_code,
                                control_task_msg_evt_t msg_evt,
                                const void* msg_evt_params,
                                size_t sizeof_msg_evt_params);

#endif /* __CONTROL_TASK_H__ */
