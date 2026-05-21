/**
 * @file meshx_uvp_logical_model.hpp
 * @brief Abstract base class for UVP logical models (meshXLogicalModel).
 *
 * A logical model encapsulates one functional capability (identified by func_id)
 * on a UVP element. Multiple logical models may share the same physical
 * meshXUVPModel transport. This is the REQ-001/REQ-002/REQ-005 interface contract.
 *
 * @note Named meshXLogicalModel to avoid collision with the existing meshXModel
 *       template class in meshx_model_class.hpp.
 *
 * @author MeshX
 * @date 2025
 */

#ifndef __MESHX_UVP_LOGICAL_MODEL_HPP__
#define __MESHX_UVP_LOGICAL_MODEL_HPP__

#include <meshx_uvp.h>           /* meshx_uvp_ctx_t — now carries func_id */
#include <meshx_common.h>        /* MESHX_SUCCESS, meshx_err_t */
#include <stdint.h>
#include <stddef.h>

/* Forward declarations — full definitions provided at point of use in .cpp files */
class meshXElementIF;
class meshXUVPModel;

/**
 * @class meshXLogicalModel
 * @brief Abstract base class for all UVP logical (functional) models.
 *
 * Each concrete subclass represents one functional unit within a UVP element,
 * identified by a unique @c func_id registered at construction time.
 *
 * Routing is performed by @c can_handle(): the default implementation returns
 * true when @c ctx->func_id matches the registered @c func_id. Subclasses
 * MUST NOT use @c param_size as a routing criterion (REQ-005).
 *
 * Ownership: instances are held by @c meshXUVPElement as
 * @c std::unique_ptr<meshXLogicalModel> entries in @c logical_models.
 */
class meshXLogicalModel {
protected:
    meshXElementIF* parent_element; ///< Non-owning pointer to the containing element
    meshXUVPModel*  physical_model; ///< Non-owning pointer to the shared physical transport
    uint16_t        func_id;        ///< Registered function ID (REQ-002)

public:
    /**
     * @brief Construct a logical model.
     * @param parent       Pointer to the parent element (non-owning).
     * @param phys_model   Pointer to the shared physical UVP transport (non-owning).
     * @param func_id      Function ID registered for this model instance (REQ-002).
     */
    meshXLogicalModel(meshXElementIF* parent,
                      meshXUVPModel*  phys_model,
                      uint16_t        func_id)
        : parent_element(parent)
        , physical_model(phys_model)
        , func_id(func_id)
    {}

    virtual ~meshXLogicalModel() = default;

    /** @brief Returns the registered function ID. */
    uint16_t get_func_id() const { return func_id; }

    /**
     * @brief Query whether this model should handle the given incoming packet.
     *
     * Default: returns true when @c ctx->func_id equals the registered @c func_id.
     * Overrides MUST NOT use @p param_size as a routing criterion (REQ-005).
     */
    virtual bool can_handle(const void*            param,
                             size_t                 param_size,
                             const meshx_uvp_ctx_t* ctx) const
    {
        (void)param;
        (void)param_size;
        return (ctx != nullptr) && (ctx->func_id == func_id);
    }

    /**
     * @brief Handle an incoming packet (BLE RX or host command). Pure virtual.
     * @param param        Payload bytes (func_id prefix already stripped by dispatcher)
     * @param param_size   Payload length
     * @param ctx          UVP routing context (func_id set by dispatcher, REQ-003)
     */
    virtual meshx_err_t handle_rx(const void*            param,
                                  size_t                 param_size,
                                  const meshx_uvp_ctx_t* ctx) = 0;

    /**
     * @brief Handle a TXCM transmission timeout (REQ-008). Pure virtual.
     * Client models report err_code=1 to app. Server models are no-ops.
     * @param ctx  UVP context (src_addr=MESHX_ADDR_UNASSIGNED, func_id=0xFFFF)
     */
    virtual meshx_err_t handle_timeout(const meshx_uvp_ctx_t* ctx) = 0;
};

#endif /* __MESHX_UVP_LOGICAL_MODEL_HPP__ */
