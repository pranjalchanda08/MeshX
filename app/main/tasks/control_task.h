/**
 * @file control_task.h
 * @brief Header file for the control task in the BLE mesh node application.
 *
 * This file contains the definitions and function prototypes for the control task,
 * which handles various control messages and events in the BLE mesh node application.
 *
 * @author Pranjal Chanda
 */

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

/**
 * @brief Control task name configuration.
 */
#define CONFIG_CONTROL_TASK_NAME "control_task"

/**
 * @brief Control task priority configuration.
 */
#ifndef CONFIG_CONTROL_TASK_PRIO
#define CONFIG_CONTROL_TASK_PRIO configTIMER_TASK_PRIORITY + 1
#endif

/**
 * @brief Control task stack size configuration.
 */
#ifndef CONFIG_CONTROL_TASK_STACK_SIZE
#define CONFIG_CONTROL_TASK_STACK_SIZE 2048
#endif

/**
 * @brief Control task queue length configuration.
 */
#ifndef CONFIG_CONTROL_TASK_QUEUE_LEN
#define CONFIG_CONTROL_TASK_QUEUE_LEN 10
#endif

/**
 * @brief Enumeration for control task message codes.
 */
typedef enum PACKED_ATTR
{
    CONTROL_TASK_MSG_CODE_TO_HAL,       /**< Message code for HAL events. */
    CONTROL_TASK_MSG_CODE_SYSTEM,       /**< Message code for system events. */
    CONTROL_TASK_MSG_CODE_TO_BLE,       /**< Message code for BLE events. */
    CONTROL_TASK_MSG_CODE_PROVISION,    /**< Message code for provisioning events. */
    CONTROL_TASK_MSG_CODE_MAX,          /**< Maximum message code value. */
} control_task_msg_code_t;

/**
 * @brief Type definition for control task message event.
 */
typedef uint32_t control_task_msg_evt_t;

/**
 * @brief Enumeration for control task message events to HAL.
 */
typedef enum PACKED_ATTR
{
    CONTROL_TASK_MSG_EVT_TO_HAL_SET_ON_OFF      = BIT0, /**< Event to set on/off state. */
    CONTROL_TASK_MSG_EVT_TO_HAL_SET_CTL         = BIT1, /**< Event to set CTL state. */
    CONTROL_TASK_MSG_EVT_TO_HAL_SET_LIGHTNESS   = BIT2, /**< Event to set lightness state. */
    CONTROL_TASK_MSG_EVT_TO_HAL_MAX,                    /**< Maximum HAL event value. */
} control_task_msg_evt_to_hal_t;

/**
 * @brief Enumeration for control task message events to BLE.
 */
typedef enum PACKED_ATTR
{
    CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF      = BIT0, /**< Event to set on/off state. */
    CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL         = BIT1, /**< Event to set CTL state. */
    CONTROL_TASK_MSG_EVT_TO_BLE_SET_LIGHTNESS   = BIT2, /**< Event to set lightness state. */
    CONTROL_TASK_MSG_EVT_TO_BLE_MAX                     /**< Maximum BLE event value. */
} control_task_msg_evt_to_ble_t;

/**
 * @brief Enumeration for control task system events.
 */
typedef enum PACKED_ATTR
{
    CONTROL_TASK_MSG_CODE_SYSTEM_MAX, /**< Maximum system event value. */
} control_task_msg_evt_system_t;

/**
 * @brief Enumeration for control task provisioning events.
 */
typedef enum PACKED_ATTR
{
    CONTROL_TASK_MSG_EVT_PROVISION_STOP         = BIT1, /**< ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT */
    CONTROL_TASK_MSG_EVT_IDENTIFY_START         = BIT2, /**< EESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT */
    CONTROL_TASK_MSG_EVT_IDENTIFY_STOP          = BIT3, /**< ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT */
    CONTROL_TASK_MSG_EVT_NODE_RESET             = BIT4, /**< CONTROL_TASK_MSG_EVT_NODE_RESET */
    CONTROL_TASK_MSG_EVT_PROXY_CONNECT          = BIT5, /**< ESP_BLE_MESH_PROXY_SERVER_CONNECTED_EVT */
    CONTROL_TASK_MSG_EVT_PROXY_DISCONN          = BIT6, /**< ESP_BLE_MESH_PROXY_SERVER_DISCONNECTED_EVT */
    CONTROL_TASK_MSG_EVT_PROVISION_ALL          = 0xFF, /**< Maximum provisioning event value. */
} control_task_msg_evt_provision_t;

/**
 * @brief Function pointer type for control task message handler.
 *
 * @param pdev      Pointer to the device structure.
 * @param evt       Event code.
 * @param params    Pointer to the event parameters.
 * @return ESP_OK on success, or an error code on failure.
 */
typedef esp_err_t (*control_task_msg_handle_t)(dev_struct_t *pdev, control_task_msg_evt_t evt, void *params);

/**
 * @brief Structure for control task message.
 */
typedef struct control_task_msg
{
    control_task_msg_code_t msg_code; /**< Message code. */
    control_task_msg_evt_t msg_evt;   /**< Message event. */
    void *msg_evt_params;             /**< Pointer to message event parameters. */
} control_task_msg_t;

/**
 * @brief Structure for control task event callback registration.
 */
typedef struct control_task_evt_cb_reg
{
    uint32_t msg_evt_bmap;                     /**< Bitmap of message events. */
    control_task_msg_handle_t cb;              /**< Callback function pointer. */
    struct control_task_evt_cb_reg * next;     /**< Pointer to the next callback registration. */
} control_task_evt_cb_reg_t;

/**
 * @brief Create the control task.
 *
 * @param pdev  Pointer to the device structure.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t create_control_task(dev_struct_t * pdev);

/**
 * @brief Register a message code handler callback for the control task.
 *
 * @param msg_code  Message code.
 * @param evt_bmap  Bitmap of events.
 * @param cb        Callback function pointer.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t control_task_reg_msg_code_handler_cb(control_task_msg_code_t msg_code,
                                control_task_msg_evt_t evt_bmap,
                                control_task_msg_handle_t cb);

/**
 * @brief Send a message to the control task.
 *
 * @param msg_code                  Message code.
 * @param msg_evt                   Message event.
 * @param msg_evt_params            Pointer to message event parameters.
 * @param sizeof_msg_evt_params     Size of message event parameters.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t control_task_send_msg(control_task_msg_code_t msg_code,
                                control_task_msg_evt_t msg_evt,
                                const void *msg_evt_params,
                                size_t sizeof_msg_evt_params);

#endif /* __CONTROL_TASK_H__ */
