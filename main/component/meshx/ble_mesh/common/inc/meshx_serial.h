/**
 * @file meshx_serial.h
 * @brief MeshX Serial Transport Layer
 *
 * Provides the UART RX state machine, hosted-mode control, and the
 * serial context shared with the MXCP dispatch engine.
 */

#ifndef __MESHX_SERIAL_H__
#define __MESHX_SERIAL_H__

#include "meshx_common.h"
#include "meshx_api.h"
#include "meshx_mxcp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Serial context shared between the RX state machine and MXCP TX path.
 */
typedef struct {
    uint8_t state;
    mxcp_frame_t rx_frame;
    uint8_t rx_ptr;
    bool hosted_mode_enabled;
} mxsp_ctx_t;

/**
 * @brief Global serial context instance (defined in meshx_serial.c).
 */
extern mxsp_ctx_t mxsp_ctx;

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

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_SERIAL_H__ */
