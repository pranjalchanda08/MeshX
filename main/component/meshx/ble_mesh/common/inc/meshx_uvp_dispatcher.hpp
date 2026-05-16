/**
 * @file meshx_uvp_dispatcher.hpp
 * @brief Unified Dispatcher for MeshX Unified Vendor Protocol (UVP).
 * 
 * This class handles the routing of incoming UVP messages from the BLE Mesh
 * stack to the targeted element instances using the element registry.
 * 
 * @author Pranjal Chanda
 * @date 2024-2025
 */

#ifndef __MESHX_UVP_DISPATCHER_HPP__
#define __MESHX_UVP_DISPATCHER_HPP__

#include <meshx_common.h>
#include <meshx_control_task.h>
#include <meshx_uvp.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the Unified Vendor Protocol Dispatcher.
 * @return MESHX_SUCCESS on success, or error code.
 */
meshx_err_t meshx_uvp_dispatcher_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_UVP_DISPATCHER_HPP__ */
