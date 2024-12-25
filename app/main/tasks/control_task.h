#ifndef __CONTROL_TASK_H__
#define __CONTROL_TASK_H__

#include "stdio.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "app_common.h"

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
    CONTROL_TASK_MSG_CODE_TO_HAL,   /* control_task_msg_evt_to_hal_t */
    CONTROL_TASK_MSG_CODE_SYSTEM,   /* control_task_msg_evt_system_t */
    CONTROL_TASK_MSG_CODE_TO_BLE,   /* control_task_msg_evt_to_ble_t */
    CONTROL_TASK_MSG_CODE_MAX,

} control_task_msg_code_t;

typedef uint32_t control_task_msg_evt_t;

typedef enum PACKED_ATTR
{
    /* To HAL Codes */
    CONTROL_TASK_MSG_EVT_TO_HAL_SET_ON_OFF      = BIT0, /* param: esp_ble_mesh_gen_onoff_srv_t */
    CONTROL_TASK_MSG_EVT_TO_HAL_SET_CTL         = BIT1, /* param: esp_ble_mesh_light_ctl_srv_t */
    CONTROL_TASK_MSG_EVT_TO_HAL_SET_LIGHTNESS   = BIT2,
    CONTROL_TASK_MSG_EVT_TO_HAL_MAX,
} control_task_msg_evt_to_hal_t;


typedef enum PACKED_ATTR
{
    /* To BLE Codes*/
    CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF      = BIT0,
    CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL         = BIT1,
    CONTROL_TASK_MSG_EVT_TO_BLE_SET_LIGHTNESS   = BIT2,
    CONTROL_TASK_MSG_EVT_TO_BLE_MAX

} control_task_msg_evt_to_ble_t;

typedef enum PACKED_ATTR
{
    CONTROL_TASK_MSG_CODE_SYSTEM_MAX,
}control_task_msg_evt_system_t;

typedef esp_err_t (*control_task_msg_handle_t)(dev_struct_t *pdev, control_task_msg_evt_t evt, void *params);

typedef struct control_task_msg
{
    control_task_msg_code_t msg_code;
    control_task_msg_evt_t msg_evt;
    void *msg_evt_params;
} control_task_msg_t;

typedef struct control_task_evt_cb_reg
{
    uint32_t msg_evt_bmap;
    control_task_msg_handle_t cb;
    struct control_task_evt_cb_reg * next;
}control_task_evt_cb_reg_t;

esp_err_t create_control_task(dev_struct_t * pdev);
esp_err_t control_task_reg_msg_code_handler_cb(control_task_msg_code_t msg_code,
                                control_task_msg_evt_t evt_bmap,
                                control_task_msg_handle_t cb);
esp_err_t control_task_send_msg(control_task_msg_code_t msg_code,
                                control_task_msg_evt_t msg_evt,
                                const void *msg_evt_params,
                                size_t sizeof_msg_evt_params);

#endif /* __CONTROL_TASK_H__ */
