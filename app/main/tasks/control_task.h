#ifndef __CONTROL_TASK_H__
#define __CONTROL_TASK_H__

#include "stdio.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#define CONFIG_CONTROL_TASK_NAME    "control_task"

#ifndef CONFIG_CONTROL_TASK_PRIO
#define CONFIG_CONTROL_TASK_PRIO    5
#endif

#ifndef CONFIG_CONTROL_TASK_STACK_SIZE
#define CONFIG_CONTROL_TASK_STACK_SIZE  2048
#endif

#ifndef CONFIG_CONTROL_TASK_QUEUE_LEN
#define CONFIG_CONTROL_TASK_QUEUE_LEN   10
#endif

typedef enum
{
    CONTROL_TASK_MSG_CODE_TO_HAL,
    CONTROL_TASK_MSG_CODE_SYSTEM,
    CONTROL_TASK_MSG_CODE_TO_BLE,
}control_task_msg_code_t;

typedef enum
{
    /* To HW Codes */
    CONTROL_TASK_MSG_EVT_SET_ON_OFF       = 0x0100,
    CONTROL_TASK_MSG_EVT_SET_LIGHTNESS    = 0x0101,

}control_task_msg_evt_t;

typedef void* control_task_evt_params_t;
typedef void (*control_task_msg_handle_t)(control_task_msg_evt_t evt, control_task_evt_params_t params);

typedef struct control_task_msg
{
    control_task_msg_code_t msg_code;
    control_task_msg_evt_t msg_evt;
    control_task_evt_params_t msg_evt_params;
}control_task_msg_t;

esp_err_t create_control_task(void);
esp_err_t control_task_send_msg(control_task_msg_code_t msg_code,
                                control_task_msg_evt_t msg_evt,
                                const control_task_evt_params_t msg_evt_params,
                                size_t sizeof_msg_evt_params);

#endif /* __CONTROL_TASK_H__ */
