/**
 * @file meshx_uvp_registry.hpp
 * @brief Dynamic UVP Opcode Handler Registration Interface.
 *
 * Provides registration and dispatch tables for MeshX Unified Vendor Protocol
 * (UVP) management opcodes across discovery, pairing, remote provisioning,
 * key rotation, redundancy, and re-healing.
 *
 * @author Pranjal Chanda
 * @date 2024-2026
 */

#ifndef __MESHX_UVP_REGISTRY_HPP__
#define __MESHX_UVP_REGISTRY_HPP__

#include <meshx_c_header.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Handler function signature for UVP system management messages.
 *
 * @param[in] p_meta      Pointer to UVP message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to payload bytes (pointing at or after opcode/msg_type).
 * @param[in] payload_len Length of payload bytes in bytes.
 * @return MESHX_SUCCESS on success, or an error code on failure.
 */
typedef meshx_err_t (*uvp_sys_msg_handler_t)(const control_task_uvp_meta_t *p_meta,
                                             const meshx_uvp_ctx_t *p_ctx,
                                             const uint8_t *payload,
                                             uint16_t payload_len);

/**
 * @brief Register an opcode handler for a specific UVP function ID.
 *
 * @param[in] func_id Function ID category (e.g., MESHX_FUNC_ID_SYS_DISCOVERY, MESHX_FUNC_ID_SYS_PAIRING).
 * @param[in] opcode  Opcode / message type byte identifier.
 * @param[in] handler Callback function to handle the message.
 * @return MESHX_SUCCESS on successful registration, or an error code.
 */
meshx_err_t meshx_uvp_register_handler(uint16_t func_id, uint8_t opcode, uvp_sys_msg_handler_t handler);

/**
 * @brief Dispatch an incoming UVP system management message using the registered handler table.
 *
 * @param[in] func_id     UVP function ID.
 * @param[in] opcode      Opcode / message type byte.
 * @param[in] p_meta      Pointer to UVP message metadata header.
 * @param[in] p_ctx       Pointer to UVP routing context.
 * @param[in] payload     Pointer to payload bytes.
 * @param[in] payload_len Length of payload in bytes.
 * @return MESHX_SUCCESS on success, MESHX_NOT_FOUND if no handler registered, or handler error code.
 */
meshx_err_t meshx_uvp_dispatch_handler(uint16_t func_id,
                                       uint8_t opcode,
                                       const control_task_uvp_meta_t *p_meta,
                                       const meshx_uvp_ctx_t *p_ctx,
                                       const uint8_t *payload,
                                       uint16_t payload_len);

/* --- Module Registration Functions --- */

/**
 * @brief Register In-Mesh Discovery (0xFFFE) opcode handlers.
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t meshx_uvp_register_discovery_handlers(void);

/**
 * @brief Register In-Mesh Pairing (0xFFFF) opcode handlers.
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t meshx_uvp_register_pairing_handlers(void);

/**
 * @brief Register Remote Provisioning (0xC0..0xC3) opcode handlers.
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t meshx_uvp_register_rpr_handlers(void);

/**
 * @brief Register Key Refresh & Rotation (0xC4..0xC7) opcode handlers.
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t meshx_uvp_register_kr_handlers(void);

/**
 * @brief Register Redundancy & Fast Infill Provisioning (0xC8..0xCD, 0xD0) opcode handlers.
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t meshx_uvp_register_redundancy_handlers(void);

/**
 * @brief Register Re-Healing & Courier Forwarding (0xCE..0xCF) opcode handlers.
 * @return MESHX_SUCCESS on success.
 */
meshx_err_t meshx_uvp_register_reheal_handlers(void);

#ifdef __cplusplus
}
#endif

#endif /* __MESHX_UVP_REGISTRY_HPP__ */
