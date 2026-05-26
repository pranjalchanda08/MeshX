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

#define MXCP_TYPE_DIR_CMD  MESHX_MSG_DIR_CMD
#define MXCP_TYPE_DIR_EVT  MESHX_MSG_DIR_EVT
#define MXCP_TYPE_ID_MASK  0x7FFF

#define MXCP_MAKE_TYPE(dir, id)  ((uint16_t)((dir) | ((id) & MXCP_TYPE_ID_MASK)))
#define MXCP_TYPE_IS_CMD(t)      (((t) & 0x8000) == 0)
#define MXCP_TYPE_IS_EVT(t)      (((t) & 0x8000) != 0)
#define MXCP_TYPE_ID(t)          ((t) & MXCP_TYPE_ID_MASK)

#pragma pack(push, 1)

/**
 * @brief MXCP wire frame for RX-side parsing.
 *
 * Layout: [SOF][LEN][TYPE][PAYLOAD...][CHK][EOF]
 * NOTE: Used for RX-side parsing only. TX uses flat-buffer
 *       serialisation for variable-length wire efficiency.
 */
typedef struct {
    uint8_t len;
    uint16_t type;
    uint8_t payload[MXCP_PAYLOAD_MAX_SIZE];
    uint8_t checksum;
} mxcp_frame_t;

#pragma pack(pop)

/**
 * @brief MXCP command handler function signature.
 *
 * @param payload  Pointer to payload bytes from the frame.
 * @param len      Payload length in bytes.
 */
typedef void (*mxcp_cmd_handler_t)(const uint8_t *payload, uint8_t len);

typedef struct {
    uint16_t             cmd_id;
    mxcp_cmd_handler_t   handler;
    uint8_t              payload_size;
    uint16_t             sync_evt_id;
    uint16_t             async_evt_id;
} mxcp_cmd_entry_t;

typedef struct {
    uint16_t  evt_id;
    uint16_t  src_cmd_id;
    uint8_t   payload_size;
} mxcp_evt_entry_t;

#define MXCP_CMD_ENTRY(id, fn, psz, sync_evt, async_evt) \
    { (id), (fn), (psz), (sync_evt), (async_evt) }

/**
 * @brief Dispatch a received MXCP frame to the matching command handler.
 *
 * @param type    TYPE byte from the frame (direction + ID).
 * @param payload Pointer to the payload bytes.
 * @param len     Payload length in bytes.
 */
void mxcp_dispatch_frame(uint16_t type, const uint8_t *payload, uint8_t len);

/**
 * @brief Build and transmit an MXCP frame over the serial interface.
 *
 * Frames are only sent when hosted mode is enabled.
 *
 * @param type    TYPE byte (direction + ID).
 * @param payload Pointer to payload bytes (may be NULL if len == 0).
 * @param len     Payload length in bytes.
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t mxcp_send_frame(uint16_t type, const uint8_t *payload, uint8_t len);

/**
 * @brief Send an MXCP event (Engine -> Host).
 *
 * @param evt_id  Event ID from @ref meshx_msg_ctrl_evt_t or meshx_msg_data_id_t (must have EVT bit set).
 * @param payload Pointer to the typed event payload.
 * @param len     Payload length in bytes.
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t mxcp_send_event(uint16_t evt_id, const uint8_t *payload, uint8_t len);

/**
 * @brief Send an MXCP command (Host -> Engine).
 *
 * @param cmd_id  Command ID from @ref meshx_msg_ctrl_cmd_t or meshx_msg_data_id_t (must have CMD bit set).
 * @param payload Pointer to the typed command payload.
 * @param len     Payload length in bytes.
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t mxcp_send_cmd(uint16_t cmd_id, const uint8_t *payload, uint8_t len);

/**
 * @brief Send a composition or element-state response to the host.
 *
 * Queries the composition builder for the requested data and sends it
 * as an MXCP event.
 *
 * @param evt_id  MESHX_MSG_CTRL_EVT_COMPOSITION_RSP or MESHX_MSG_CTRL_EVT_ELEMENT_STATE_RSP.
 */
void mxcp_send_element_info_response(uint16_t evt_id);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_MXCP_H__ */
