/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_host.h
 * @brief Host SDK initialization and process interfaces.
 */

#ifndef __MESHX_HOST_H__
#define __MESHX_HOST_H__

#include <stdint.h>
#include <stddef.h>
#include "meshx_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Compile-time heap choice for the Host SDK.
 *
 * By default, the Host SDK uses a statically allocated buffer for message
 * packing. If MALLOC is enabled, it will use malloc() and free()
 * instead.
 */
#ifndef CONFIG_MESHX_HOST_USE_MALLOC
#define CONFIG_MESHX_HOST_USE_MALLOC 1
#endif

#if CONFIG_MESHX_HOST_USE_MALLOC
#include <stdlib.h>
#ifndef MESHX_HOST_MALLOC
#define MESHX_HOST_MALLOC(size) malloc(size)
#endif
#ifndef MESHX_HOST_FREE
#define MESHX_HOST_FREE(ptr) free(ptr)
#endif
#else /* CONFIG_MESHX_HOST_USE_MALLOC */
/* Use a static buffer instead of malloc() */
#ifndef MESHX_HOST_MALLOC
#define MESHX_HOST_MALLOC(size) ((void*)0)
#endif /* MESHX_HOST_MALLOC */
#ifndef MESHX_HOST_FREE
#define MESHX_HOST_FREE(ptr) ((void)0)
#endif /* MESHX_HOST_FREE */
#endif /* CONFIG_MESHX_HOST_USE_MALLOC */

/**
 * @brief Initialize the MeshX Host SDK variables.

 * @param rx_data_cb Callback pointer for received data path events/messages.
 * @param rx_ctrl_cb Callback pointer for received control path events/messages.
 */
void meshx_host_init(meshx_api_data_cb_t rx_data_cb, meshx_api_ctrl_cb_t rx_ctrl_cb);

/**
 * @brief Decode an incoming MXCP frame and fire appropriate callbacks.
 *        This should be called by the platform port when data is received.
 * @param frame Pointer to the raw frame payload.
 * @param len Length of the frame payload.
 */
void meshx_host_process_rx(const uint8_t *frame, size_t len);

/**
 * @brief User-provided porting function to send a raw frame over the transport.
 * @param frame Pointer to the raw frame payload.
 * @param len Length of the frame payload.
 * @return 0 on success, else error code.
 */
int meshx_port_tx(const uint8_t *frame, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_HOST_H__ */
