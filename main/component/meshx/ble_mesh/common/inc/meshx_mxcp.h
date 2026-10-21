/**
 * @file meshx_mxcp.h
 * @brief MeshX Command Protocol (MXCP) Definitions
 *
 * Unified, single-layer, table-driven command/event protocol for
 * communication between the MeshX Engine and an external Host MCU.
 *
 * Frame Format (identical structure to legacy MXSP):
 *   [SOF][LEN][TYPE][PAYLOAD][CHK][EOF]
 *
 * TYPE byte encoding:
 *   Bit 7:     Direction (0 = CMD Host→Engine, 1 = EVT Engine→Host)
 *   Bits 6-0:  Command/Event ID
 *
 * @author Pranjal Chanda
 * @date 2026-05-19
 * @copyright Copyright 2024 - 2025 MeshX
 */

#ifndef __MESHX_MXCP_H__
#define __MESHX_MXCP_H__

#include "meshx_common.h"
#include "meshx_api.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MXCP_SOF               0xFE
#define MXCP_EOF               0xEF
#define MXCP_PAYLOAD_MAX_SIZE  255

#define MXCP_UUID_SIZE         16
#define MXCP_NETKEY_SIZE       16
#define MXCP_AUTH_DATA_MAX_SIZE 16
#define MXCP_DEV_NAME_MAX_LEN  32
#define MXCP_RPR_PDU_MAX_SIZE  64

/* Buffer sizes (including null-terminator for string buffers) */
#define MXCP_DEV_NAME_BUF_SIZE     (MXCP_DEV_NAME_MAX_LEN + 1)
#define MXCP_AUTH_STR_BUF_SIZE     (MXCP_AUTH_DATA_MAX_SIZE + 1)

/* Wire Frame Sizes */
#define MXCP_FRAME_HDR_SIZE        3  /**< Frame preamble: [SOF][LEN][TYPE] */
#define MXCP_FRAME_TRAILER_SIZE    2  /**< Frame trailer: [CHK][EOF] */
#define MXCP_FRAME_MAX_SIZE        (MXCP_FRAME_HDR_SIZE + MXCP_PAYLOAD_MAX_SIZE + MXCP_FRAME_TRAILER_SIZE)

/* Command Header Sizes */
#define MXCP_CMD_PROV_AUTH_INPUT_HDR_SIZE (sizeof(uint8_t) + sizeof(uint8_t))
#define MXCP_CMD_UVP_RPR_TUNNEL_HDR_SIZE  (sizeof(uint16_t) + MXCP_UUID_SIZE + sizeof(uint8_t))

#define MXCP_TYPE_DIR_CMD  0x00
#define MXCP_TYPE_DIR_EVT  0x80
#define MXCP_TYPE_ID_MASK  0x7F

#define MXCP_MAKE_TYPE(dir, id)  ((uint8_t)((dir) | ((id) & MXCP_TYPE_ID_MASK)))
#define MXCP_TYPE_IS_CMD(t)      (((t) & 0x80) == 0)
#define MXCP_TYPE_IS_EVT(t)      (((t) & 0x80) != 0)
#define MXCP_TYPE_ID(t)          ((t) & MXCP_TYPE_ID_MASK)

/* Local Provisioning Enums & Structs */
#define MXCP_PROV_BEARER_PB_ADV   0x01
#define MXCP_PROV_BEARER_PB_GATT  0x02
#define MXCP_PROV_BEARER_BOTH     (MXCP_PROV_BEARER_PB_ADV | MXCP_PROV_BEARER_PB_GATT)

/**
 * @brief MXCP GPIO Response Payload Sizes
 */
#define MXCP_EVT_GPIO_RSP_HDR_SIZE          (sizeof(uint8_t) + sizeof(uint8_t))
#define MXCP_EVT_GPIO_SET_LEVEL_RSP_SIZE    (MXCP_EVT_GPIO_RSP_HDR_SIZE)
#define MXCP_EVT_GPIO_GET_LEVEL_RSP_SIZE    (MXCP_EVT_GPIO_RSP_HDR_SIZE + sizeof(((mxcp_evt_gpio_rsp_t *)0)->data.get_level))
#define MXCP_EVT_GPIO_TOGGLE_RSP_SIZE       (MXCP_EVT_GPIO_RSP_HDR_SIZE + sizeof(((mxcp_evt_gpio_rsp_t *)0)->data.toggle))
#define MXCP_EVT_GPIO_SET_PWM_DUTY_RSP_SIZE (MXCP_EVT_GPIO_RSP_HDR_SIZE)
#define MXCP_EVT_GPIO_SET_PWM_FREQ_RSP_SIZE (MXCP_EVT_GPIO_RSP_HDR_SIZE)
#define MXCP_EVT_GPIO_INTR_ENABLE_RSP_SIZE  (MXCP_EVT_GPIO_RSP_HDR_SIZE)
#define MXCP_EVT_GPIO_INTR_DISABLE_RSP_SIZE (MXCP_EVT_GPIO_RSP_HDR_SIZE)
#define MXCP_EVT_GPIO_GET_CONFIG_RSP_SIZE   (MXCP_EVT_GPIO_RSP_HDR_SIZE + sizeof(((mxcp_evt_gpio_rsp_t *)0)->data.get_config))
#define MXCP_EVT_GPIO_GET_STATE_RSP_SIZE    (MXCP_EVT_GPIO_RSP_HDR_SIZE + sizeof(((mxcp_evt_gpio_rsp_t *)0)->data.get_state))

/**
 * @brief Command ID namespace (Host -> Engine, bit 7 = 0).
 */
typedef enum {
    MXCP_CMD_HOSTED_MODE_ENABLE    = 0x01,
    MXCP_CMD_NODE_RESET            = 0x02,
    MXCP_CMD_GET_COMPOSITION       = 0x03,
    MXCP_CMD_GET_ELEMENT_STATE     = 0x04,
    MXCP_CMD_SET_CONSOLE_ROUTING   = 0x05,
    MXCP_CMD_SET_COMPOSITION       = 0x06,
    MXCP_CMD_PROV_ENABLE           = 0x0B,
    MXCP_CMD_PROV_DISABLE          = 0x0C,
    MXCP_CMD_PROV_GET_STATE        = 0x0D,
    MXCP_CMD_PROV_SET_NAME         = 0x0E,
    MXCP_CMD_PROV_AUTH_INPUT       = 0x0F,

    MXCP_CMD_EL_SEND               = 0x10,

#if CONFIG_MESHX_ENABLE_GPIO_TEST_API
    MXCP_CMD_GPIO_SET_LEVEL        = 0x21,
    MXCP_CMD_GPIO_GET_LEVEL        = 0x22,
    MXCP_CMD_GPIO_TOGGLE           = 0x23,
    MXCP_CMD_GPIO_SET_PWM_DUTY     = 0x24,
    MXCP_CMD_GPIO_SET_PWM_FREQ     = 0x25,
    MXCP_CMD_GPIO_INTR_ENABLE      = 0x26,
    MXCP_CMD_GPIO_INTR_DISABLE     = 0x27,
    MXCP_CMD_GPIO_GET_CONFIG       = 0x28,
    MXCP_CMD_GPIO_GET_STATE        = 0x29,
#endif

    MXCP_CMD_UVP_RPR_SCAN_START        = 0x30,
    MXCP_CMD_UVP_RPR_TUNNEL_SEND       = 0x31,
    MXCP_CMD_UVP_KEY_ROTATE_TRIGGER    = 0x32,
    MXCP_CMD_UVP_REDUNDANCY_SET_ROLE   = 0x33,
    MXCP_CMD_UVP_FAST_PROV_START       = 0x34,

    MXCP_CMD_DISCOVER_CAPABILITIES = 0x70,
    MXCP_CMD_BIND_ELEMENTS         = 0x71,
    MXCP_CMD_UNBIND_ELEMENTS       = 0x72,
    MXCP_CMD_GET_LINK_TABLE        = 0x73,
    MXCP_CMD_GET_LINK_EPOCH        = 0x74,

    MXCP_CMD_MAX
} mxcp_cmd_id_t;

/**
 * @brief Event ID namespace (Engine -> Host, bit 7 = 1).
 */
typedef enum {
    MXCP_EVT_NONE                  = 0x00,
    MXCP_EVT_PROV_COMP             = 0x01,
    MXCP_EVT_PROV_FAILED           = 0x02,
    MXCP_EVT_PROV_START            = 0x03,
    MXCP_EVT_IDENTIFY_START        = 0x04,
    MXCP_EVT_IDENTIFY_STOP         = 0x05,
    MXCP_EVT_COMPOSITION_RSP       = 0x06,
    MXCP_EVT_ELEMENT_STATE_RSP     = 0x07,
    MXCP_EVT_NODE_RESET_IND        = 0x08,
    MXCP_EVT_HOSTED_MODE_RSP       = 0x09,
    MXCP_EVT_CONSOLE_ROUTING_RSP   = 0x0A,
    MXCP_EVT_SET_COMPOSITION_RSP   = 0x0B,

    MXCP_EVT_EL_DATA_RX_NOTIFY     = 0x10,
    MXCP_EVT_EL_DATA_TX_NOTIFY     = 0x11,

#if CONFIG_MESHX_ENABLE_GPIO_TEST_API
    MXCP_EVT_GPIO_SET_LEVEL_RSP    = 0x21,
    MXCP_EVT_GPIO_GET_LEVEL_RSP    = 0x22,
    MXCP_EVT_GPIO_TOGGLE_RSP       = 0x23,
    MXCP_EVT_GPIO_SET_PWM_DUTY_RSP = 0x24,
    MXCP_EVT_GPIO_SET_PWM_FREQ_RSP = 0x25,
    MXCP_EVT_GPIO_INTR_ENABLE_RSP  = 0x26,
    MXCP_EVT_GPIO_INTR_DISABLE_RSP = 0x27,
    MXCP_EVT_GPIO_GET_CONFIG_RSP   = 0x28,
    MXCP_EVT_GPIO_GET_STATE_RSP    = 0x29,
    MXCP_EVT_GPIO_ASYNC            = 0x3E,
    MXCP_EVT_GPIO_ERROR            = 0x3F,
#endif

    MXCP_EVT_CAPABILITY_REPORT     = 0x70,
    MXCP_EVT_BIND_STATUS           = 0x71,
    MXCP_EVT_UNBIND_STATUS         = 0x72,
    MXCP_EVT_LINK_TABLE_RSP        = 0x73,
    MXCP_EVT_LINK_EPOCH_RSP        = 0x74,

    MXCP_EVT_PROV_STATE_IND        = 0x8C,
    MXCP_EVT_PROV_LINK_OPEN        = 0x8D,
    MXCP_EVT_PROV_LINK_CLOSE       = 0x8E,
    MXCP_EVT_PROV_AUTH_REQ         = 0x8F,

    MXCP_EVT_UVP_RPR_BEACON_IND       = 0xB0,
    MXCP_EVT_UVP_RPR_TUNNEL_RECV      = 0xB1,
    MXCP_EVT_UVP_KEY_ROTATION_PHASE   = 0xB2,
    MXCP_EVT_UVP_REDUNDANCY_STATUS    = 0xB3,
    MXCP_EVT_UVP_FAST_PROV_PROGRESS   = 0xB4,

    MXCP_EVT_MAX
} mxcp_evt_id_t;

typedef enum {
    MXCP_PROV_STATE_UNPROVISIONED = 0x00, /**< Node is unprovisioned */
    MXCP_PROV_STATE_BEACONING     = 0x01, /**< Node is actively beaconing (PB-ADV/GATT) */
    MXCP_PROV_STATE_PROVISIONING  = 0x02, /**< Provisioning link/exchange in progress */
    MXCP_PROV_STATE_PROVISIONED   = 0x03, /**< Node is provisioned and operational */
} mxcp_prov_state_t;

typedef enum {
    MXCP_PROV_AUTH_TYPE_NUMERIC      = 0x01, /**< Numeric OOB input */
    MXCP_PROV_AUTH_TYPE_ALPHANUMERIC = 0x02, /**< Alphanumeric string OOB input */
} mxcp_prov_auth_type_t;

typedef enum {
    MXCP_COMP_STATUS_SUCCESS            = 0x00,
    MXCP_COMP_STATUS_ERR_COUNT_EXCEEDED = 0x01,
    MXCP_COMP_STATUS_ERR_INVALID_TYPE   = 0x02,
    MXCP_COMP_STATUS_ERR_NVS_WRITE      = 0x03,
} mxcp_comp_status_t;

#pragma pack(push, 1)

typedef struct {
    uint8_t enable;
} mxcp_cmd_hosted_mode_enable_t;

typedef struct {
    uint8_t enable;
} mxcp_cmd_set_console_routing_t;

/**
 * @brief Unify element data send command with canonical meshx_data_msg_hdr_t (REQ-001)
 */
typedef meshx_data_msg_hdr_t mxcp_cmd_el_send_t;

#if CONFIG_MESHX_ENABLE_GPIO_TEST_API
typedef struct {
    uint8_t logical_pin;
    uint8_t level;
} mxcp_cmd_gpio_set_level_t;

typedef struct {
    uint8_t logical_pin;
} mxcp_cmd_gpio_get_level_t;

typedef struct {
    uint8_t logical_pin;
} mxcp_cmd_gpio_toggle_t;

typedef struct {
    uint8_t  logical_pin;
    uint8_t  duty_cycle;
} mxcp_cmd_gpio_set_pwm_duty_t;

typedef struct {
    uint8_t  logical_pin;
    uint32_t frequency;
} mxcp_cmd_gpio_set_pwm_freq_t;

typedef struct {
    uint8_t logical_pin;
    uint8_t enable;
} mxcp_cmd_gpio_intr_enable_t;

typedef struct {
    uint8_t logical_pin;
} mxcp_cmd_gpio_intr_disable_t;

typedef struct {
    uint8_t logical_pin;
} mxcp_cmd_gpio_get_config_t;

typedef struct {
    uint8_t logical_pin;
} mxcp_cmd_gpio_get_state_t;
#endif

typedef struct {
    uint16_t net_idx;
    uint16_t addr;
    uint8_t  device_uuid[MXCP_UUID_SIZE];
} mxcp_evt_prov_comp_t;

typedef struct {
    uint8_t reason;
} mxcp_evt_prov_failed_t;

typedef struct {
    uint8_t element_count;
} mxcp_evt_composition_rsp_t;

typedef struct {
    uint8_t  element_count;
    uint16_t reboot_delay_ms;
    uint16_t element_types[0];
} mxcp_cmd_set_composition_t;

typedef struct {
    uint8_t  status;
    uint8_t  element_count;
    uint16_t reboot_delay_ms;
} mxcp_evt_set_composition_rsp_t;

typedef struct {
    uint8_t element_count;
} mxcp_evt_element_state_rsp_t;

/**
 * @brief Unify element data notify events with canonical meshx_data_msg_hdr_t (REQ-001)
 */
typedef meshx_data_msg_hdr_t mxcp_evt_el_data_rx_notify_t;
typedef meshx_data_msg_hdr_t mxcp_evt_el_data_tx_notify_t;

#if CONFIG_MESHX_ENABLE_GPIO_TEST_API
typedef struct {
    uint8_t  status;
    uint8_t  logical_pin;
    union {
        struct {
            uint8_t level;
        } get_level;
        struct {
            uint8_t level;
        } toggle;
        struct {
            uint8_t mode;
            uint8_t pull;
            uint8_t drive_strength;
            uint8_t initial_level;
            uint8_t signal_inversion;
        } get_config;
        struct {
            uint8_t current_level;
            uint8_t interrupt_registered;
            uint8_t intr_type;
        } get_state;
    } data;
} mxcp_evt_gpio_rsp_t;

typedef struct {
    uint8_t  event_type;
    uint8_t  logical_pin;
    uint8_t  value;
    uint32_t timestamp;
} mxcp_evt_gpio_async_t;
#endif

typedef struct {
    uint16_t idx;
    uint16_t variant;
    uint16_t type;
} mxcp_comp_entry_header_t;

typedef struct {
    uint16_t idx;
    uint16_t variant;
    uint16_t ctx_size;
    uint16_t telemetry_size;
} mxcp_state_entry_header_t;

typedef struct {
    uint16_t target_unicast;
    uint8_t  filter_role;
} mxcp_cmd_discover_capabilities_t;

typedef struct {
    uint16_t src_addr;
    uint8_t  src_elem;
    uint16_t dst_addr;
    uint8_t  dst_elem;
    uint16_t func_id;
} mxcp_cmd_bind_elements_t;

typedef struct {
    uint16_t src_addr;
    uint8_t  src_elem;
    uint16_t dst_addr;
    uint8_t  dst_elem;
    uint16_t func_id;
} mxcp_cmd_unbind_elements_t;

typedef struct {
    uint8_t  status;
    uint16_t group_addr;
} mxcp_evt_bind_status_t;

typedef struct {
    uint8_t status;
} mxcp_evt_unbind_status_t;

typedef struct {
    uint16_t link_epoch;
    uint16_t node_addr;
} mxcp_evt_link_epoch_rsp_t;

typedef struct {
    uint8_t  bearer_mask;       /**< 0x01 = PB-ADV, 0x02 = PB-GATT, 0x03 = Both */
    uint16_t duration_sec;      /**< Beaconing duration (0 = indefinite) */
} mxcp_cmd_prov_enable_t;

typedef struct {
    char device_name[MXCP_DEV_NAME_MAX_LEN]; /**< Friendly device advertising name */
} mxcp_cmd_prov_set_name_t;

typedef struct {
    uint8_t  auth_type;         /**< 0x01 = Number, 0x02 = Alphanumeric (mxcp_prov_auth_type_t) */
    uint8_t  auth_len;          /**< Length of input auth payload */
    uint8_t  auth_data[MXCP_AUTH_DATA_MAX_SIZE]; /**< Input authentication bytes */
} mxcp_cmd_prov_auth_input_t;

typedef struct {
    uint8_t  state;             /**< mxcp_prov_state_t (0=Unprov, 1=Beacon, 2=Proving, 3=Provd) */
    uint16_t unicast_addr;      /**< Assigned primary unicast address */
    uint16_t net_idx;           /**< Primary NetKey index */
} mxcp_evt_prov_state_ind_t;

typedef struct {
    uint8_t  bearer;            /**< 0x01 = PB-ADV, 0x02 = PB-GATT */
    uint8_t  device_uuid[MXCP_UUID_SIZE]; /**< Remote or local device UUID */
} mxcp_evt_prov_link_open_t;

typedef struct {
    uint8_t  bearer;            /**< 0x01 = PB-ADV, 0x02 = PB-GATT */
    uint8_t  reason;            /**< Disconnect/close reason */
} mxcp_evt_prov_link_close_t;

typedef struct {
    uint8_t  action;            /**< 0x01 = Output Display, 0x02 = Input Request */
    uint8_t  auth_size;         /**< Number of digits / bytes */
    uint32_t output_num;        /**< Random number to display (if action == 1) */
} mxcp_evt_prov_auth_req_t;

/* UVP Remote Provisioning (RPR) Structs */
typedef struct {
    uint16_t proxy_node_addr;   /**< 0xFFFF for all nodes, or specific relay address */
    uint8_t  duration_sec;      /**< Scan duration (seconds) */
    uint8_t  uuid_filter[MXCP_UUID_SIZE]; /**< All 0s = match any unprov node */
} mxcp_cmd_uvp_rpr_scan_t;

typedef struct {
    uint16_t proxy_node_addr;   /**< Address of relay node in range of unprov device */
    uint8_t  target_uuid[MXCP_UUID_SIZE]; /**< Target unprovisioned node UUID */
    uint8_t  pdu_len;           /**< Length of raw provisioning PDU */
    uint8_t  pdu_data[MXCP_RPR_PDU_MAX_SIZE]; /**< Provisioning PDU bytes */
} mxcp_cmd_uvp_rpr_tunnel_t;

typedef struct {
    uint16_t proxy_node_addr;   /**< Node that heard the beacon */
    uint8_t  uuid[MXCP_UUID_SIZE]; /**< Discovered device UUID */
    int8_t   rssi;              /**< Received signal strength */
    uint16_t oob_info;          /**< Advertised OOB info */
} mxcp_evt_uvp_rpr_beacon_ind_t;

typedef struct {
    uint16_t proxy_node_addr;   /**< Reporting proxy relay */
    uint8_t  target_uuid[MXCP_UUID_SIZE]; /**< Target unprov device UUID */
    uint8_t  pdu_len;           /**< Length of reply PDU */
    uint8_t  pdu_data[MXCP_RPR_PDU_MAX_SIZE]; /**< Reply PDU payload */
} mxcp_evt_uvp_rpr_tunnel_recv_t;

/* UVP Key Rotation Enums & Structs */
typedef enum {
    MXCP_KR_PHASE_IDLE        = 0, /**< No key rotation active */
    MXCP_KR_PHASE_DISTRIBUTE  = 1, /**< Phase 1: Zero-ACK push of new key to virtual address */
    MXCP_KR_PHASE_SWITCHOVER  = 2, /**< Phase 2: Switch primary TX key to new key */
    MXCP_KR_PHASE_REVOKE      = 3, /**< Phase 3: Invalidate old key and commit epoch to NVS */
} mxcp_kr_phase_t;

typedef struct {
    uint8_t  phase;             /**< mxcp_kr_phase_t (1=Distribute, 2=Switchover, 3=Revoke) */
    uint16_t net_idx;           /**< Primary NetKey index */
    uint8_t  new_key[MXCP_NETKEY_SIZE]; /**< 16-byte NetKey payload (only for Phase 1) */
} mxcp_cmd_uvp_key_rotate_t;

typedef struct {
    uint8_t  current_phase;     /**< mxcp_kr_phase_t (1, 2, or 3) */
    uint32_t rotation_epoch;    /**< Monotonically increasing epoch */
    uint8_t  status_code;       /**< 0 = Success */
} mxcp_evt_uvp_key_rot_phase_t;

/* UVP Provisioner Redundancy Structs */
typedef struct {
    uint8_t  role;              /**< 1 = Master, 2 = Standby */
    uint16_t peer_addr;         /**< Peer provisioner unicast address */
    uint16_t pool_start;        /**< Assigned address pool start */
    uint16_t pool_end;          /**< Assigned address pool end */
} mxcp_cmd_uvp_redundancy_role_t;

typedef struct {
    uint8_t  current_role;      /**< 1 = Master, 2 = Standby */
    uint8_t  peer_alive;        /**< 1 = Peer heartbeat OK, 0 = Lost */
    uint16_t next_free_addr;    /**< Next allocatable address in pool */
    uint32_t sync_epoch;        /**< Provisioner state sync epoch */
} mxcp_evt_uvp_redundancy_status_t;

/* UVP Fast Bulk Provisioning Structs */
typedef struct {
    uint8_t  uuid_mask[MXCP_UUID_SIZE]; /**< Filter mask for bulk discovery */
    uint16_t target_count;      /**< Total nodes to batch onboard */
    uint16_t base_addr;         /**< Starting unicast address block */
} mxcp_cmd_uvp_fast_prov_start_t;

typedef struct {
    uint16_t onboarded_count;   /**< Nodes successfully provisioned */
    uint16_t total_target;      /**< Total nodes requested */
    uint8_t  last_uuid[MXCP_UUID_SIZE]; /**< UUID of most recent provisioned node */
    uint16_t last_addr;         /**< Assigned unicast address */
    uint8_t  progress_percent;  /**< Overall progress (0-100) */
} mxcp_evt_uvp_fast_prov_progress_t;

#pragma pack(pop)

/**
 * @brief MXCP wire frame for RX-side parsing.
 *
 * Layout: [SOF][LEN][TYPE][PAYLOAD...][CHK][EOF]
 * NOTE: Used for RX-side parsing only. TX uses flat-buffer
 *       serialisation for variable-length wire efficiency.
 */
typedef struct {
    uint8_t len;
    uint8_t type;
    uint8_t payload[MXCP_PAYLOAD_MAX_SIZE];
    uint8_t checksum;
} mxcp_frame_t;

/**
 * @brief MXCP command handler function signature.
 *
 * @param payload  Pointer to payload bytes from the frame.
 * @param len      Payload length in bytes.
 *
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
typedef meshx_err_t (*mxcp_cmd_handler_t)(const uint8_t *payload, uint8_t len);

typedef struct {
    uint8_t              cmd_id;
    mxcp_cmd_handler_t   handler;
    uint8_t              payload_size;
    uint8_t              sync_evt_id;
    uint8_t              async_evt_id;
} mxcp_cmd_entry_t;

typedef struct {
    uint8_t  evt_id;
    uint8_t  src_cmd_id;
    uint8_t  payload_size;
} mxcp_evt_entry_t;

#define MXCP_CMD_ENTRY(id, fn, psz, sync_evt, async_evt) \
    { (id), (fn), (psz), (sync_evt), (async_evt) }

/**
 * @brief Dispatch a received MXCP frame to the matching command handler.
 *
 * @param[in] type    TYPE byte from the frame (direction + ID).
 * @param[in] payload Pointer to the payload bytes.
 * @param[in] len     Payload length in bytes.
 *
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
meshx_err_t mxcp_dispatch_frame(uint8_t type, const uint8_t *payload, uint8_t len);

/**
 * @brief Build and transmit an MXCP frame over the serial interface.
 *
 * Frames are only sent when hosted mode is enabled.
 *
 * @param[in] type    TYPE byte (direction + ID).
 * @param[in] payload Pointer to payload bytes (may be NULL if len == 0).
 * @param[in] len     Payload length in bytes.
 *
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t mxcp_send_frame(uint8_t type, const uint8_t *payload, uint8_t len);

/**
 * @brief Send an MXCP event (Engine -> Host).
 *
 * @param[in] evt_id  Event ID from @ref mxcp_evt_id_t.
 * @param[in] payload Pointer to the typed event payload.
 * @param[in] len     Payload length in bytes.
 *
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t mxcp_send_event(mxcp_evt_id_t evt_id, const uint8_t *payload, uint8_t len);

/**
 * @brief Send an MXCP command (Host -> Engine).
 *
 * @param[in] cmd_id  Command ID from @ref mxcp_cmd_id_t.
 * @param[in] payload Pointer to the typed command payload.
 * @param[in] len     Payload length in bytes.
 *
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t mxcp_send_cmd(mxcp_cmd_id_t cmd_id, const uint8_t *payload, uint8_t len);

/**
 * @brief Send a composition or element-state response to the host.
 *
 * Queries the composition builder for the requested data and sends it
 * as an MXCP event.
 *
 * @param[in] evt_id MXCP_EVT_COMPOSITION_RSP or MXCP_EVT_ELEMENT_STATE_RSP.
 */
void mxcp_send_element_info_response(mxcp_evt_id_t evt_id);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_MXCP_H__ */
