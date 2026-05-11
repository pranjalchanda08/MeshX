/**
 * @file meshx_relay_msg_defs.h
 * @brief Shared message definitions for Relay elements.
 */

#ifndef __MESHX_RELAY_MSG_DEFS_H__
#define __MESHX_RELAY_MSG_DEFS_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Standard Relay Client Message structure (shared between C element and C++ model)
 * @details This structure must match the layout expected by the control task dispatch.
 */
typedef struct meshx_gen_on_off_cli_msg {
    uint8_t ack;
    uint8_t set_get;
    uint8_t on_off;
    uint8_t reserved;
    uint16_t element_id;
} meshx_gen_on_off_cli_msg_t;

#define MESHX_GEN_ON_OFF_CLI_MSG_SET    0
#define MESHX_GEN_ON_OFF_CLI_MSG_GET    1
#define MESHX_GEN_ON_OFF_CLI_MSG_ACK    1
#define MESHX_GEN_ON_OFF_CLI_MSG_NO_ACK 0

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_RELAY_MSG_DEFS_H__ */
