/**
 * Copyright © 2024 - 2025 MeshX
 *
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

#include "unit_test.h"
#include <meshx_common.h>
#include <meshx_control_task.h>
/* NOTE: meshx_ble_mesh_sensor_srv.h decommissioned in TASK-007 (UVP migration) */

#ifdef __cplusplus
extern "C" {
#endif

#define MESHX_APP_API_MSG_MAX_SIZE  sizeof(meshx_data_payload_t)

/* MeshX Function ID Relay Server */
#define MESHX_ELEMENT_FUNC_ID_RELAY_SERVER_ONN_OFF          0x00

/* MeshX Function ID Light CWWW Server */
#define MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_SERVER_ONN_OFF     0x00
#define MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_SERVER_CTL         0x01

/* MeshX Function ID Light HSL Server */
#define MESHX_ELEMENT_FUNC_ID_LIGHT_HSL_SERVER_ONN_OFF      0x00
#define MESHX_ELEMENT_FUNC_ID_LIGHT_HSL_SERVER_HSL          0x01

/* MeshX Function ID Light CWWW Client */
#define MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_CLIENT_ONN_OFF     0x00
#define MESHX_ELEMENT_FUNC_ID_LIGHT_CWWW_CLIENT_CTL         0x01

/* MeshX Function ID Sensor Server */
#define MESHX_ELEMENT_FUNC_ID_SENSOR_SERVER_DATA            0x00

/* MeshX Function ID Sensor Client */
#define MESHX_ELEMENT_FUNC_ID_SENSOR_CLIENT_DATA            0x00

/* MeshX Function ID Light HSL Client */
#define MESHX_ELEMENT_FUNC_ID_LIGHT_HSL_CLIENT_ONN_OFF      0x00
#define MESHX_ELEMENT_FUNC_ID_LIGHT_HSL_CLIENT_HSL          0x01

/**
 * @brief Enumeration of BLE Mesh application API message types.
 */
typedef enum meshx_api_type
{
    MESHX_API_TYPE_DATA = CONTROL_TASK_MSG_EVT_DATA,        /**< Data message : All msg rekated to BLE mesh elements */
    MESHX_API_TYPE_CTRL = CONTROL_TASK_MSG_EVT_CTRL,        /**< Control message : All msg related to System control */
} meshx_api_type_t;

/**
 * @brief Enumeration of Client Logical Model error codes.
 */
typedef enum meshx_api_client_err
{
    MESHX_CLIENT_ERR_SUCCESS = 0x00,          /**< Operation successful */
    MESHX_CLIENT_ERR_TIMEOUT = 0x01,          /**< TXCM timeout */
    MESHX_CLIENT_ERR_UNCONFIGURED = 0x02,     /**< No pub_addr configured */
} meshx_api_client_err_t;


/**
 * @brief Structure defines the payload for MESHX_ELEMENT_TYPE_RELAY_SERVER
 */
typedef struct meshx_api_relay_server_evt
{
   uint8_t on_off;
}meshx_api_relay_server_evt_t;

/**
 * @brief Structure defines the payload for MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER
 */
typedef struct meshx_api_light_cwww_server_evt
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
}meshx_api_light_cwww_server_evt_t;

/**
 * @brief Structure defines the payload for MESHX_ELEMENT_TYPE_LIGHT_HSL_SERVER
 */
typedef struct meshx_api_light_hsl_server_evt
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
            uint16_t hue;
            uint16_t saturation;
        }hsl;
    }state_change;
}meshx_api_light_hsl_server_evt_t;

/**
 * @brief Structure defines the payload for MESHX_ELEMENT_TYPE_SENSOR_SERVER
 */
typedef struct meshx_api_sensor_server_evt
{
    union
    {
        struct
        {
            uint16_t value;
        }data;
    }state_change;
}meshx_api_sensor_server_evt_t;

/**
 * @brief Structure defines the payload for MESHX_ELEMENT_TYPE_LIGHT_HSL_CLIENT
 */
typedef struct meshx_api_light_hsl_client_evt
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
            uint16_t hue;
            uint16_t saturation;
        }hsl;
    }state_change;
}meshx_api_light_hsl_client_evt_t;

/**
 * @brief Structure defines the payload for MESHX_ELEMENT_TYPE_SENSOR_CLIENT
 */
typedef struct meshx_api_sensor_client_evt
{
   uint8_t err_code;
    union
    {
        struct
        {
            uint16_t value;
        }data;
    }state_change;
}meshx_api_sensor_client_evt_t;


/**
 * @brief Structure defines the payload for MESHX_ELEMENT_TYPE_RELAY_CLIENT
 */
typedef struct meshx_api_relay_client_state
{
   uint8_t err_code;
   uint8_t on_off;
}meshx_api_relay_client_evt_t;

/**
 * @brief Structure defines the payload for MESHX_ELEMENT_TYPE_LIGHT_CWWW_CLIENT
 */
typedef struct meshx_api_light_cwww_client_evt
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
}meshx_api_light_cwww_client_evt_t;
/**
 * @brief Structure for the BLE Mesh application control message.
 *
 * This structure defines the BLE Mesh application control payloads.
 *
 */
typedef union meshx_data_payload
{
    meshx_api_relay_client_evt_t relay_client_evt;
    meshx_api_relay_server_evt_t relay_server_evt;
    meshx_api_light_cwww_client_evt_t light_cwww_client_evt;
    meshx_api_light_cwww_server_evt_t light_cwww_server_evt;
    meshx_api_light_hsl_server_evt_t light_hsl_server_evt;
    meshx_api_light_hsl_client_evt_t light_hsl_client_evt;
    meshx_api_sensor_server_evt_t sensor_server_evt;
    meshx_api_sensor_client_evt_t sensor_client_evt;
} meshx_data_payload_t;

/**
 * @brief Structure defines the payload for meshx_ctrl_payload
 */
typedef union meshx_ctrl_payload
{
    struct {
        uint16_t net_idx;
        uint16_t addr;
        uint8_t device_uuid[16];
    } prov_comp;
    struct {
        uint8_t reason;
    } prov_failed;
    uint32_t reserved;
} meshx_ctrl_payload_t;


#pragma pack(push, 1)

/**
 * @brief Canonical 8-byte Element Data Message Header.
 *
 * Single source of truth for both internal element messages and MXCP wire payloads
 * (MXCP_CMD_EL_SEND 0x10, MXCP_EVT_EL_DATA_RX_NOTIFY 0x10, MXCP_EVT_EL_DATA_TX_NOTIFY 0x11).
 */
typedef struct meshx_data_msg_hdr
{
    uint16_t element_id;               /**< Element index (0 .. N-1) */
    uint16_t element_type;             /**< Element type (meshx_element_type_t) */
    uint16_t func_id;                  /**< Functionality / Opcode ID */
    uint16_t msg_len;                  /**< Length of payload following header in bytes */
} meshx_data_msg_hdr_t;

/**
 * @brief Canonical Control Message Header.
 */
typedef struct meshx_ctrl_msg_hdr
{
    uint16_t evt_id;                   /**< Control Event ID (meshx_ctrl_evt_t) */
    uint16_t status;                   /**< Status / Result code (0 = SUCCESS) */
    uint16_t msg_len;                  /**< Length of control payload in bytes */
} meshx_ctrl_msg_hdr_t;

#pragma pack(pop)

/**
 * @brief Backward-compatible typedef alias for legacy code.
 */
typedef meshx_data_msg_hdr_t meshx_app_element_msg_header_t;

/**
 * @brief Structure for the BLE Mesh application control message header (legacy).
 */
typedef struct meshx_ctrl_msg_header
{
    uint16_t evt_id;                /* Event */
    uint16_t reserved;              /* Reserved */
} meshx_ctrl_msg_header_t;

/**
 * @brief Structure for the BLE Mesh application API message.
 *
 * Internal message wrapper exchanged over the control task message queue.
 */
typedef struct meshx_app_api_msg
{
    union
    {
        meshx_ctrl_msg_header_t ctrl_msg;
        meshx_data_msg_hdr_t    element_msg;
    } msg_type_u;

    uint8_t data[MESHX_APP_API_MSG_MAX_SIZE];
} meshx_app_api_msg_t;

/**
 * @brief BLE Mesh application callback function for element data.
 *
 * @param[in] msg_hdr   Pointer to the BLE Mesh application data message header.
 * @param[in] msg       Pointer to the data payload union.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
typedef meshx_err_t (*meshx_app_data_cb_t)(const meshx_app_element_msg_header_t *msg_hdr, const meshx_data_payload_t *msg);

/**
 * @brief BLE Mesh application control callback function.
 *
 * @param[in] msg_hdr   Pointer to the BLE Mesh application control message header.
 * @param[in] msg       Pointer to the control payload union.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
typedef meshx_err_t (*meshx_app_ctrl_cb_t)(const meshx_ctrl_msg_header_t *msg_hdr, const meshx_ctrl_payload_t *msg);

/* ====================================================================
 * Unified Data & Control Path Public APIs (REQ-001, REQ-002, REQ-003)
 * ==================================================================== */

/**
 * @brief Send element data upwards to App and Host MCU (Data Path).
 *
 * Emits MXCP_EVT_EL_DATA_RX_NOTIFY if MXCP active and dispatches to local registered app_data_cb.
 *
 * @param[in] hdr       Pointer to unified data header.
 * @param[in] payload   Pointer to element payload data.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_data_send_to_app(const meshx_data_msg_hdr_t *hdr, const void *payload);

/**
 * @brief Send element command downwards from App or Host MCU into the MeshX Element Engine.
 *
 * @param[in] hdr       Pointer to unified data header.
 * @param[in] payload   Pointer to element payload data.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_data_send_to_element(const meshx_data_msg_hdr_t *hdr, const void *payload);

/**
 * @brief Dispatch a system control event to the registered app control callback and MXCP (Control Path).
 *
 * @param[in] evt_id        Control event ID (meshx_ctrl_evt_t).
 * @param[in] status        Result or status code.
 * @param[in] payload       Pointer to control payload data (optional).
 * @param[in] payload_len   Length of control payload.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_ctrl_send_event(meshx_ctrl_evt_t evt_id, uint16_t status, const void *payload, uint16_t payload_len);

/**
 * @brief Register callback for element data messages (Data Path).
 *
 * @param[in] cb Pointer to the data callback.
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_app_reg_data_callback(meshx_app_data_cb_t cb);

/**
 * @brief Register callback for system control messages (Control Path).
 *
 * @param[in] cb Pointer to the control callback.
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_app_reg_ctrl_callback(meshx_app_ctrl_cb_t cb);

/* ====================================================================
 * Legacy API Prototypes (Maintained for full backward compatibility)
 * ==================================================================== */

/**
 * @brief Sends a message to the BLE Mesh application.
 *
 * @param[in] element_id    The element ID.
 * @param[in] element_type  The element type.
 * @param[in] func_id       The function ID.
 * @param[in] msg_len       The message length.
 * @param[in] msg           Pointer to the message.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_send_msg_to_app(uint16_t element_id, uint16_t element_type, uint16_t func_id, uint16_t msg_len, const void *msg);

/**
 * @brief Sends a control message to the BLE Mesh application.
 *
 * @param[in] evt       The event ID.
 * @param[in] msg_len   The message length.
 * @param[in] msg       Pointer to the message.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_send_ctrl_msg_to_app(meshx_ctrl_evt_t evt, uint16_t msg_len, const void *msg);

/**
 * @brief Sends a message to the element.
 *
 * @param[in] element_id    The element ID.
 * @param[in] element_type  The element type.
 * @param[in] func_id       The function ID.
 * @param[in] msg_len       The message length.
 * @param[in] msg           Pointer to the message.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_send_msg_to_element(uint16_t element_id, uint16_t element_type, uint16_t func_id, uint16_t msg_len, const void *msg);

/**
 * @brief Registers the BLE Mesh application callback (Data Path).
 *
 * @param[in] cb Pointer to the application callback.
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_app_reg_element_callback(meshx_app_data_cb_t cb);

/**
 * @brief Registers the BLE Mesh application control callback (Control Path).
 *
 * @param[in] cb Pointer to the control callback.
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshx_app_reg_system_events_callback(meshx_app_ctrl_cb_t cb);

/**
 * @brief Get the network key identifier.
 * @return uint16_t The network key identifier.
 */
uint16_t meshx_get_net_key_id(void);

/**
 * @brief Get the node's primary unicast address.
 * @return uint16_t The node's primary unicast address, or 0x0000 if unprovisioned.
 */
uint16_t meshx_get_node_addr(void);

/**
 * @brief Get the total number of elements on this node.
 * @return size_t The element count.
 */
size_t meshx_get_element_count(void);

/**
 * @brief Get the active Company ID (CID).
 * @return uint16_t The company ID.
 */
uint16_t meshx_get_cid(void);

/**
 * @brief Get the active Product ID (PID).
 * @return uint16_t The product ID.
 */
uint16_t meshx_get_pid(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __MESHX_API_H__ */
