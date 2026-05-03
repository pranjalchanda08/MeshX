/**
 * @file meshx_serial.h
 * @brief MeshX Serial Protocol (MXSP) Definitions
 *
 * This file defines the frame structure and APIs for the MeshX Serial Protocol,
 * used for communication between the Mesh engine and an external Host MCU.
 */

#ifndef __MESHX_SERIAL_H__
#define __MESHX_SERIAL_H__

#include "meshx_common.h"
#include "meshx_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MXSP_SOF 0xFE
#define MXSP_EOF 0xEF

#define MXSP_PAYLOAD_MAX_SIZE 255

typedef enum {
    MXSP_MSG_TYPE_DATA        = 0x01,
    MXSP_MSG_TYPE_CTRL        = 0x02,
    MXSP_MSG_TYPE_HOSTED_MODE = 0x03,
} mxsp_msg_type_t;

/**
 * @brief Set the hosted mode state
 *
 * @param enabled True to enable, false to disable
 */
void meshx_serial_set_hosted_mode(bool enabled);

/**
 * @brief Check if hosted mode is enabled
 *
 * @return true if enabled, false otherwise
 */
bool meshx_serial_is_hosted_mode_enabled(void);

/**
 * @brief MXSP Frame structure
 */
typedef struct {
    uint8_t sof;
    uint8_t len;
    uint8_t type;
    uint8_t payload[MXSP_PAYLOAD_MAX_SIZE];
    uint8_t checksum;
    uint8_t eof;
} mxsp_frame_t;

/**
 * @brief Initialize the MeshX Serial Protocol handler
 *
 * @return meshx_err_t MESHX_SUCCESS on success
 */
meshx_err_t meshx_serial_init(void);

/**
 * @brief Parse incoming byte stream for MXSP frames
 *
 * @param data Byte to parse
 */
void meshx_serial_parse_byte(uint8_t data);

/**
 * @brief Send a control event via MXSP
 *
 * @param evt_header Control message header
 * @param payload Control message payload
 * @return meshx_err_t MESHX_SUCCESS on success
 */
meshx_err_t mxsp_send_ctrl_event(const meshx_ctrl_msg_header_t *evt_header, const meshx_ctrl_payload_t *payload);

/**
 * @brief Send data event via MXSP
 *
 * @param msg_hdr Data message header
 * @param payload Data message payload
 * @return meshx_err_t MESHX_SUCCESS on success
 */
meshx_err_t mxsp_send_data_event(const meshx_app_element_msg_header_t *msg_hdr, const meshx_data_payload_t *payload);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_SERIAL_H__ */
