/**
 * @file meshx_uvp_logical_models.hpp
 * @brief Concrete UVP logical model subclasses (meshXLogicalModel derivatives).
 *
 * @author MeshX
 * @date 2025
 */

#ifndef __MESHX_UVP_LOGICAL_MODELS_HPP__
#define __MESHX_UVP_LOGICAL_MODELS_HPP__

#include <meshx_uvp_logical_model.hpp>
#include <meshx_api.h>

/* =========================================================================
 * meshXRelayClientModel  —  func_id=0x00 (OnOff)
 * ========================================================================= */
class meshXRelayClientModel : public meshXLogicalModel {
public:
    using meshXLogicalModel::meshXLogicalModel;
    meshx_err_t handle_rx(const void* param, size_t param_size,
                           const meshx_uvp_ctx_t* ctx) override;
    meshx_err_t handle_timeout(const meshx_uvp_ctx_t* ctx) override;
};

/* =========================================================================
 * meshXRelayServerModel  —  func_id=0x00 (OnOff)
 * ========================================================================= */
class meshXRelayServerModel : public meshXLogicalModel {
public:
    using meshXLogicalModel::meshXLogicalModel;
    meshx_err_t handle_rx(const void* param, size_t param_size,
                           const meshx_uvp_ctx_t* ctx) override;
    meshx_err_t handle_timeout(const meshx_uvp_ctx_t* ctx) override;
};

/* =========================================================================
 * meshXLightCWWWClientModel  —  func_id=0x00 OnOff / 0x01 CTL (two instances)
 * ========================================================================= */
class meshXLightCWWWClientModel : public meshXLogicalModel {
public:
    using meshXLogicalModel::meshXLogicalModel;
    meshx_err_t handle_rx(const void* param, size_t param_size,
                           const meshx_uvp_ctx_t* ctx) override;
    meshx_err_t handle_timeout(const meshx_uvp_ctx_t* ctx) override;
};

/* =========================================================================
 * meshXLightCWWWServerModel  —  func_id=0x00 OnOff / 0x01 CTL (two instances)
 * ========================================================================= */
class meshXLightCWWWServerModel : public meshXLogicalModel {
public:
    using meshXLogicalModel::meshXLogicalModel;
    meshx_err_t handle_rx(const void* param, size_t param_size,
                           const meshx_uvp_ctx_t* ctx) override;
    meshx_err_t handle_timeout(const meshx_uvp_ctx_t* ctx) override;
};

/* =========================================================================
 * meshXSensorServerModel  —  func_id=0x00 (Data)
 * ========================================================================= */
class meshXSensorServerModel : public meshXLogicalModel {
public:
    using meshXLogicalModel::meshXLogicalModel;
    meshx_err_t handle_rx(const void* param, size_t param_size,
                           const meshx_uvp_ctx_t* ctx) override;
    meshx_err_t handle_timeout(const meshx_uvp_ctx_t* ctx) override;
};

/* =========================================================================
 * meshXSensorClientModel  —  func_id=0x00 (Data)
 * ========================================================================= */
class meshXSensorClientModel : public meshXLogicalModel {
public:
    using meshXLogicalModel::meshXLogicalModel;
    meshx_err_t handle_rx(const void* param, size_t param_size,
                           const meshx_uvp_ctx_t* ctx) override;
    meshx_err_t handle_timeout(const meshx_uvp_ctx_t* ctx) override;
};

/* =========================================================================
 * meshXLightHSLServerModel  —  func_id=0x00 OnOff / 0x01 HSL (two instances)
 * ========================================================================= */
class meshXLightHSLServerModel : public meshXLogicalModel {
public:
    using meshXLogicalModel::meshXLogicalModel;
    meshx_err_t handle_rx(const void* param, size_t param_size,
                           const meshx_uvp_ctx_t* ctx) override;
    meshx_err_t handle_timeout(const meshx_uvp_ctx_t* ctx) override;
};

/* =========================================================================
 * meshXLightHSLClientModel  —  func_id=0x00 OnOff / 0x01 HSL (two instances)
 * ========================================================================= */
class meshXLightHSLClientModel : public meshXLogicalModel {
public:
    using meshXLogicalModel::meshXLogicalModel;
    meshx_err_t handle_rx(const void* param, size_t param_size,
                           const meshx_uvp_ctx_t* ctx) override;
    meshx_err_t handle_timeout(const meshx_uvp_ctx_t* ctx) override;
};

/* Aliases for ColorRGB models to support strict naming compliance */
using meshXColorRGBServerModel = meshXLightHSLServerModel;
using meshXColorRGBClientModel = meshXLightHSLClientModel;

#endif /* __MESHX_UVP_LOGICAL_MODELS_HPP__ */
