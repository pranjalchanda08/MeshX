/**
 * @file meshx_uvp_registry.cpp
 * @brief Dynamic UVP Opcode Handler Registration Implementation.
 *
 * Implements a lightweight opcode-to-handler lookup registry for UVP
 * system management functions.
 *
 * @author Pranjal Chanda
 * @date 2024-2026
 */

#include "meshx_uvp_registry.hpp"
#include <map>

/* =========================================================================
 * Registry Data Structures
 * ========================================================================= */

/**
 * @brief Compound key for opcode registration: (func_id << 8) | opcode.
 */
static inline uint32_t make_registry_key(uint16_t func_id, uint8_t opcode)
{
    return (static_cast<uint32_t>(func_id) << 8u) | static_cast<uint32_t>(opcode);
}

/**
 * @brief Map storing registered opcode handlers.
 */
static std::map<uint32_t, uvp_sys_msg_handler_t> s_handler_registry;

/* =========================================================================
 * Registration and Dispatch APIs
 * ========================================================================= */

/**
 * @brief Registers a handler for a specific UVP function ID and opcode.
 *
 * @param[in] func_id   UVP function identifier.
 * @param[in] opcode    UVP opcode.
 * @param[in] handler   Pointer to the handler function.
 * @return MESHX_SUCCESS on successful registration, or MESHX_INVALID_ARG if the handler is NULL.
 */
meshx_err_t meshx_uvp_register_handler(uint16_t func_id, uint8_t opcode, uvp_sys_msg_handler_t handler)
{
    if (!handler)
    {
        MESHX_LOGE(MODULE_ID_COMMON, "UVP Registry: Attempted to register NULL handler for func 0x%04X op 0x%02X",
                   func_id, opcode);
        return MESHX_INVALID_ARG;
    }

    uint32_t key = make_registry_key(func_id, opcode);
    s_handler_registry[key] = handler;
    MESHX_LOGD(MODULE_ID_COMMON, "UVP Registry: Registered handler for func 0x%04X, opcode 0x%02X",
               func_id, opcode);
    return MESHX_SUCCESS;
}

/**
 * @brief Dispatches a UVP message to its registered handler.
 * @param[in] func_id   UVP function identifier.
 * @param[in] opcode    UVP opcode.
 * @param[in] p_meta    Pointer to message metadata.
 * @param[in] p_ctx     Pointer to UVP context.
 * @param[in] payload   Pointer to payload bytes.
 * @param[in] payload_len Length of payload bytes.
 * @return MESHX_SUCCESS on successful dispatch, or MESHX_NOT_FOUND if no handler is registered.
 */
meshx_err_t meshx_uvp_dispatch_handler(uint16_t func_id,
                                       uint8_t opcode,
                                       const control_task_uvp_meta_t *p_meta,
                                       const meshx_uvp_ctx_t *p_ctx,
                                       const uint8_t *payload,
                                       uint16_t payload_len)
{
    if (!p_meta || !p_ctx || !payload)
    {
        return MESHX_INVALID_ARG;
    }

    uint32_t key = make_registry_key(func_id, opcode);
    auto it = s_handler_registry.find(key);
    if (it != s_handler_registry.end() && it->second != nullptr)
    {
        return it->second(p_meta, p_ctx, payload, payload_len);
    }

    /* Fallback: If func_id was standard app/unassigned but opcode is in pairing/mgmt band, check pairing func */
    if (func_id != MESHX_FUNC_ID_SYS_PAIRING)
    {
        uint32_t pairing_key = make_registry_key(MESHX_FUNC_ID_SYS_PAIRING, opcode);
        auto p_it = s_handler_registry.find(pairing_key);
        if (p_it != s_handler_registry.end() && p_it->second != nullptr)
        {
            return p_it->second(p_meta, p_ctx, payload, payload_len);
        }
    }

    MESHX_LOGD(MODULE_ID_COMMON, "UVP Registry: No handler registered for func 0x%04X, opcode 0x%02X",
               func_id, opcode);
    return MESHX_NOT_FOUND;
}
