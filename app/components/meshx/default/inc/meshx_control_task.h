/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_control_task.h
 * @brief Header file for the control task in the BLE mesh node application.
 *
 * This file contains the definitions and function prototypes for the control task,
 * which handles various control messages and events in the BLE mesh node application.
 *
 * @author Pranjal Chanda
 */

#ifndef __MESHX_CONTROL_TASK__
#define __MESHX_CONTROL_TASK__

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
#define CONFIG_CONTROL_TASK_NAME        "meshx_control_task"

/**
 * @brief Control task priority configuration.
 */
#ifndef CONFIG_CONTROL_TASK_PRIO
#define CONFIG_CONTROL_TASK_PRIO        configTIMER_TASK_PRIORITY + 1
#endif

/**
 * @brief Control task stack size configuration.
 */
#ifndef CONFIG_CONTROL_TASK_STACK_SIZE
#define CONFIG_CONTROL_TASK_STACK_SIZE  4096
#endif

/**
 * @brief Control task queue length configuration.
 */
#ifndef CONFIG_CONTROL_TASK_QUEUE_LEN
#define CONFIG_CONTROL_TASK_QUEUE_LEN   10
#endif

/**
 * @brief Enumeration for control task message codes.
 */
typedef enum control_task_msg_code
{
    CONTROL_TASK_MSG_CODE_EL_STATE_CH,  /**< Message code for Element state change */
    CONTROL_TASK_MSG_CODE_SYSTEM,       /**< Message code for system events. */
    CONTROL_TASK_MSG_CODE_TO_BLE,       /**< Message code to BLE layer. */
    CONTROL_TASK_MSG_CODE_FRM_BLE,      /**< Message code from BLE layer. */
    CONTROL_TASK_MSG_CODE_PROVISION,    /**< Message code for provisioning events. */
    CONTROL_TASK_MSG_CODE_TO_APP,       /**< Message code for application events. */
    CONTROL_TASK_MSG_CODE_TO_MESHX,     /**< Message code for meshX events from app */
    CONTROL_TASK_MSG_CODE_MAX,          /**< Maximum message code value. */
} control_task_msg_code_t;

/**
 * @brief Type definition for control task message event.
 */
typedef uint32_t control_task_msg_evt_t;

/**
 * @brief Enumeration for control task message events to application.
 */
typedef enum control_task_msg_evt_to_app_meshx
{
    CONTROL_TASK_MSG_EVT_DATA = BIT0,   /**< Data message */
    CONTROL_TASK_MSG_EVT_CTRL = BIT1,   /**< Control message */
    CONTROL_TASK_MSG_EVT_MAX,           /**< Maximum event value */
} control_task_msg_evt_to_app_meshx_t;

/**
 * @brief Enumeration for control task message events to HAL.
 */
typedef enum control_task_msg_evt_el_state_ch
{
    CONTROL_TASK_MSG_EVT_EL_STATE_CH_SET_ON_OFF      = BIT0, /**< Event to set on/off state. */
    CONTROL_TASK_MSG_EVT_EL_STATE_CH_SET_CTL         = BIT1, /**< Event to set CTL state. */
    CONTROL_TASK_MSG_EVT_EL_STATE_CH_MAX,                    /**< Maximum HAL event value. */
} control_task_msg_evt_el_state_ch_t;

/**
 * @brief Enumeration for control task message events to BLE.
 */
typedef enum control_task_msg_evt_to_ble
{
    CONTROL_TASK_MSG_EVT_TO_BLE_SET_ON_OFF      = BIT0, /**< Event to set on/off state. */
    CONTROL_TASK_MSG_EVT_TO_BLE_SET_CTL         = BIT1, /**< Event to set CTL state. */
    CONTROL_TASK_MSG_EVT_TO_BLE_SET_LIGHTNESS   = BIT2, /**< Event to set lightness state. */
    CONTROL_TASK_MSG_EVT_TO_BLE_MAX                     /**< Maximum BLE event value. */
} control_task_msg_evt_to_ble_t;

/**
 * @brief Enumeration for control task system events.
 */
typedef enum control_task_msg_evt_system
{
    CONTROL_TASK_MSG_EVT_SYSTEM_RESTART      = BIT0,    /**< Event to restart the system. */
    CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_ARM    = BIT1,    /**< Event to arm an OS Timer */
    CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_REARM  = BIT2,    /**< Event to re-arm an OS Timer */
    CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_DISARM = BIT3,    /**< Event to stop an OS Timer */
    CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_FIRE   = BIT4,    /**< Event to fire timedout OS Timer */
    CONTROL_TASK_MSG_EVT_SYSTEM_TIMER_PERIOD = BIT5,    /**< Event to set timedout OS Timer */
    CONTROL_TASK_MSG_EVT_SYSTEM_FRESH_BOOT   = BIT6,    /**< Event to indicate fresh boot */
    CONTROL_TASK_MSG_EVT_SYSTEM_MAX,                    /**< Maximum system event value. */
} control_task_msg_evt_system_t;

/**
 * @brief Enumeration for control task provisioning events.
 */
typedef enum control_task_msg_evt_provision
{
    CONTROL_TASK_MSG_EVT_PROVISION_STOP         = BIT1, /**< ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT */
    CONTROL_TASK_MSG_EVT_IDENTIFY_START         = BIT2, /**< EESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT */
    CONTROL_TASK_MSG_EVT_IDENTIFY_STOP          = BIT3, /**< ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT */
    CONTROL_TASK_MSG_EVT_NODE_RESET             = BIT4, /**< CONTROL_TASK_MSG_EVT_NODE_RESET */
    CONTROL_TASK_MSG_EVT_PROXY_CONNECT          = BIT5, /**< ESP_BLE_MESH_PROXY_SERVER_CONNECTED_EVT */
    CONTROL_TASK_MSG_EVT_PROXY_DISCONN          = BIT6, /**< ESP_BLE_MESH_PROXY_SERVER_DISCONNECTED_EVT */
    CONTROL_TASK_MSG_EVT_EN_NODE_PROV           = BIT7, /**< ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT */
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
 * This function creates a FreeRTOS task to handle control events.
 *
 * @param[in] pdev Pointer to the device structure (dev_struct_t).
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t create_control_task(dev_struct_t * pdev);

/**
 * @brief Subscribe to a control task message.
 *
 * This function allows you to subscribe to a specific control task message
 * identified by the given message code. When the message is received, the
 * specified callback function will be invoked.
 *
 * @param[in] msg_code  The message code to subscribe to.
 * @param[in] evt_bmap  The event bitmap associated with the message.
 * @param[in] callback  The callback function to be called when the message is received.
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid argument
 *     - ESP_FAIL: Other failures
 */
esp_err_t control_task_msg_subscribe(control_task_msg_code_t msg_code, control_task_msg_evt_t evt_bmap, control_task_msg_handle_t cbcallback);

/**
 * @brief Deregister a callback for a specific message code and event bitmap.
 *
 * This function allows deregistering a callback handler for a specific message code and event type.
 *
 * @param[in] msg_code  The message code to deregister the handler for.
 * @param[in] evt_bmap  Bitmap of events to deregister for.
 * @param[in] callback        Callback function to deregister.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t control_task_msg_unsubscribe(control_task_msg_code_t msg_code, control_task_msg_evt_t evt_bmap, control_task_msg_handle_t callback);

/**
 * @brief Publish a control task message.
 *
 * This function allows you to publish a control task message with the given
 * message code, event, and event parameters.
 * The message will be sent to the control task for processing.
 *
 * @param[in] msg_code              The message code to publish.
 * @param[in] msg_evt               The event associated with the message.
 * @param[in] msg_evt_params        Pointer to the event parameters.
 * @param[in] sizeof_msg_evt_params Size of the event parameters.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t control_task_msg_publish(control_task_msg_code_t msg_code, control_task_msg_evt_t msg_evt, const void *msg_evt_params, size_t sizeof_msg_evt_params);

#endif /* __MESHX_CONTROL_TASK__ */
