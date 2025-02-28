/**
 * @file meshx_api.h
 * @brief This file contains the API definitions for the MeshX application.
 *
 * The MeshX API provides functions and structures to interact with the BLE Mesh application.
 * It includes message types, payload structures, and callback function definitions.
 *
 * @author Pranjal Chanda
 */

#ifndef __MESHX_API_H__
#define __MESHX_API_H__

#include <app_common.h>
#include <meshx_control_task.h>
#include <meshx_elements.h>

#define MESHX_APP_API_MSG_MAX_SIZE  sizeof(meshx_data_payload_t)

/* MeshX Function ID Relay Server */
#define MESHX_ELEMENT_FUNC_ID_RELAY_SERVER_ONN_OFF          0x00

/* MeshX Function ID Light CWWW Server */
#define MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_SERVER_ONN_OFF     0x00
#define MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_SERVER_CTL         0x01

/* MeshX Function ID Light CWWW Server */
#define MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_CLIENT_ONN_OFF     0x00
#define MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_CLIENT_CTL         0x01
/**
 * @brief Enumeration of BLE Mesh application API message types.
 */
typedef enum meshx_api_type
{
    MESHX_API_TYPE_DATA = CONTROL_TASK_MSG_EVT_DATA,        /**< Data message : All msg rekated to BLE mesh elements */
    MESHX_API_TYPE_CTRL = CONTROL_TASK_MSG_EVT_CTRL,        /**< Control message : All msg related to System control */
} meshx_api_type_t;

/**
 * @brief Enumeration of BLE Mesh application API message types.
 */
typedef enum meshx_element_type
{
    MESHX_ELEMENT_TYPE_RELAY_SERVER,
    MESHX_ELEMENT_TYPE_RELAY_CLIENT,
    MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER,
    MESHX_ELEMENT_TYPE_LIGHT_CWWW_CLIENT,
    MESHX_ELEMENT_TYPE_MAX
}meshx_element_type_t;

/**
 * @brief Structure defines the payload for MESHX_ELEMENT_TYPE_RELAY_SERVER
 */
typedef struct meshx_el_relay_server_evt
{
   uint8_t on_off;
}meshx_el_relay_server_evt_t;

/**
 * @brief Structure defines the payload for MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER
 */
typedef struct meshx_el_light_cwww_server_evt
{
    union
    {
        struct
        {
            uint8_t state;
        }on_off;
        struct
        {
            uint16_t lightness;
            uint16_t temperature;
            uint16_t delta_uv;
            uint16_t temp_range_min;
            uint16_t temp_range_max;
        }ctl;
    }state_change;
}meshx_el_light_cwww_server_evt_t;

/**
 * @brief Structure defines the payload for MESHX_ELEMENT_TYPE_RELAY_CLIENT
 */
typedef struct meshx_el_relay_client_state
{
   uint8_t err_code;
   uint8_t on_off;
}meshx_el_relay_client_evt_t;

/**
 * @brief Structure defines the payload for MESHX_ELEMENT_TYPE_LIGHT_CWWW_CLIENT
 */
typedef struct meshx_el_light_cwww_client_evt
{
   uint8_t err_code;
   union
    {
        struct
        {
            uint8_t state;
        }on_off;
        struct
        {
            uint16_t lightness;
            uint16_t temperature;
            uint16_t delta_uv;
            uint16_t temp_range_min;
            uint16_t temp_range_max;
        }ctl;
    }state_change;
}meshx_el_light_cwww_client_evt_t;
/**
 * @brief Structure for the BLE Mesh application control message.
 *
 * This structure defines the BLE Mesh application control payloads.
 *
 */
typedef union meshx_data_payload
{
    meshx_el_relay_client_evt_t relay_client_evt;
    meshx_el_relay_server_evt_t relay_server_evt;
    meshx_el_light_cwww_client_evt_t light_cwww_client_evt;
    meshx_el_light_cwww_server_evt_t light_cwww_server_evt;
} meshx_data_payload_t;

/**
 * @brief Structure defines the payload for meshx_ctrl_payload
 */
typedef union meshx_ctrl_payload
{
    uint32_t reserved;
} meshx_ctrl_payload_t;

/**
 * @brief Structure for the BLE Mesh application element message header.
 *
 * This structure defines the header for BLE Mesh application element messages.
 *
 */
typedef struct meshx_app_element_msg_header
{
    uint16_t element_id;               /* Element ID */
    uint16_t element_type;             /* meshx_element_type_t */
    uint16_t func_id;                  /* Functionality number */
    uint16_t msg_len;                  /* Length of the message */
}meshx_app_element_msg_header_t;

/**
 * @brief Structure for the BLE Mesh application control message header.
 *
 * This structure defines the header for BLE Mesh application control messages.
 *
 */
typedef struct meshx_ctrl_msg_header
{
    uint16_t evt;                   /* Event */
    uint16_t reserved;              /* Reserved */
}meshx_ctrl_msg_header_t;

/**
 * @brief Structure for the BLE Mesh application API message.
 *
 * This structure defines the BLE Mesh application API message.
 *
 */
typedef struct meshx_app_api_msg
{
    union
    {
        meshx_ctrl_msg_header_t ctrl_msg;
        meshx_app_element_msg_header_t element_msg;
    }msg_type_u;

    uint8_t data[MESHX_APP_API_MSG_MAX_SIZE];
}meshx_app_api_msg_t;

/**
 * @brief BLE Mesh application callback function.
 *
 * This function is the BLE Mesh application callback function.
 *
 * @param[in] msg_hdr   Pointer to the BLE Mesh application message header.
 * @param[in] msg       Pointer to the message.
 *
 * @return ESP_OK on success, error code otherwise.
 */
typedef esp_err_t (*meshx_app_data_cb_t)(const meshx_app_element_msg_header_t *msg_hdr, const meshx_data_payload_t *msg);

/**
 * @brief BLE Mesh application control callback function.
 *
 * This function is the BLE Mesh application control callback function.
 *
 * @param[in] msg_hdr   Pointer to the BLE Mesh application control message header.
 * @param[in] msg       Pointer to the message.
 *
 * @return ESP_OK on success, error code otherwise.
 */
typedef esp_err_t (*meshx_app_ctrl_cb_t)(const meshx_ctrl_msg_header_t *msg_hdr, const meshx_ctrl_payload_t *msg);

/**
 * @brief Sends a message to the BLE Mesh application.
 *
 * This function sends a message to the BLE Mesh application.
 *
 * @param[in] element_id    The element ID.
 * @param[in] element_type  The element type.
 * @param[in] func_id       The function ID.
 * @param[in] msg_len       The message length.
 * @param[in] msg           Pointer to the message.
 *
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t meshx_send_msg_to_app(uint16_t element_id, uint16_t element_type, uint16_t func_id, uint16_t msg_len, const void *msg);

/**
 * @brief Sends a message to the element
 *
 * This function sends a message to the element from BLE mesh Application
 *
 * @param[in] element_id    The element ID.
 * @param[in] element_type  The element type.
 * @param[in] func_id       The function ID.
 * @param[in] msg_len       The message length.
 * @param[in] msg           Pointer to the message.
 *
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t meshx_send_msg_to_element(uint16_t element_id, uint16_t element_type, uint16_t func_id, uint16_t msg_len, const void *msg);

/**
 * @brief Registers the BLE Mesh application callback.
 *
 * This function registers the BLE Mesh application data path callback.
 *
 * @param[in] cb Pointer to the application callback.
 *
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t meshx_app_reg_element_callback(meshx_app_data_cb_t cb);

/**
 * @brief Registers the BLE Mesh application control callback.
 *
 * This function registers the BLE Mesh application control callback.
 *
 * @param[in] cb Pointer to the control callback.
 *
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t meshx_app_reg_system_events_callback(meshx_app_ctrl_cb_t cb);

#endif /* __MESHX_API_H__ */
