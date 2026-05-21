/**
 * @file meshx_uvp_logical_models.cpp
 * @brief Concrete implementations of UVP logical model subclasses.
 *
 * NOTE: In derived classes, inherited members from meshXLogicalModel must be
 * accessed via this-> for dependent name lookup resolution in C++ inheritance.
 *
 * @author MeshX
 * @date 2025
 */

#include <meshx_uvp_logical_models.hpp>
#include <meshx_uvp_model.hpp>         /* meshXUVPModel full definition (for send_with_func_id) */
#include <meshx_fwd_decl.hpp>          /* meshXElementIF full definition, meshx_element_common_ctx_t */
#include <meshx_element_class.hpp>     /* get_element_idx, get_element_variant, get_element_ctx */
#include <meshx_api.h>
#include "interface/logging/meshx_log.h"
#include <cstring>
#include <algorithm>

/* =========================================================================
 * Helpers
 * ========================================================================= */

static inline const meshx_element_common_ctx_t*
get_common_ctx(meshXElementIF* elem)
{
    if (!elem) return nullptr;
    return static_cast<const meshx_element_common_ctx_t*>(elem->get_element_ctx());
}

/* =========================================================================
 * meshXRelayClientModel
 * ========================================================================= */

meshx_err_t meshXRelayClientModel::handle_rx(
    const void*            param,
    size_t                 param_size,
    const meshx_uvp_ctx_t* ctx)
{
    if (!ctx) return MESHX_SUCCESS;

    const uint16_t el_idx  = this->parent_element->get_element_idx();
    const uint16_t type_id = (uint16_t)this->parent_element->get_element_variant();

    if (ctx->src_addr == 0x0001u) {
        /* Host command path (REQ-007) */
        const auto* el_ctx = get_common_ctx(this->parent_element);
        if (!el_ctx || el_ctx->pub_addr == MESHX_ADDR_UNASSIGNED) {
            MESHX_LOGW(MODULE_ID_BLE_MESH_ELEMENT,
                "RelayClient [%d] func_id=0x%02x: no pub_addr configured",
                el_idx, this->get_func_id());
            return MESHX_INVALID_STATE;
        }
        MESHX_LOGD(MODULE_ID_BLE_MESH_ELEMENT,
            "RelayClient [%d] func_id=0x%02x: forwarding to 0x%04x",
            el_idx, this->get_func_id(), el_ctx->pub_addr);
        return this->physical_model->send_with_func_id(
            el_ctx->pub_addr, type_id, this->get_func_id(),
            param, (uint16_t)param_size, true);
    }

    /* BLE response path (REQ-007) */
    MESHX_LOGI(MODULE_ID_BLE_MESH_ELEMENT,
        "RelayClient [%d] func_id=0x%02x: ACK from 0x%04x",
        el_idx, this->get_func_id(), ctx->src_addr);

    meshx_api_relay_client_evt_t evt;
    std::memset(&evt, 0, sizeof(evt));
    evt.err_code = 0;
    if (param && param_size > 0) {
        evt.on_off = *(static_cast<const uint8_t*>(param));
    }
    return meshx_send_msg_to_app(el_idx, type_id, this->get_func_id(), sizeof(evt), &evt);
}

meshx_err_t meshXRelayClientModel::handle_timeout(const meshx_uvp_ctx_t* /*ctx*/)
{
    meshx_api_relay_client_evt_t evt;
    std::memset(&evt, 0, sizeof(evt));
    evt.err_code = 1;
    MESHX_LOGW(MODULE_ID_BLE_MESH_ELEMENT,
        "RelayClient [%d] func_id=0x%02x: TXCM timeout",
        this->parent_element->get_element_idx(), this->get_func_id());
    return meshx_send_msg_to_app(
        this->parent_element->get_element_idx(),
        (uint16_t)this->parent_element->get_element_variant(),
        this->get_func_id(), sizeof(evt), &evt);
}

/* =========================================================================
 * meshXRelayServerModel
 * ========================================================================= */

meshx_err_t meshXRelayServerModel::handle_rx(
    const void*            param,
    size_t                 param_size,
    const meshx_uvp_ctx_t* ctx)
{
    if (!ctx) return MESHX_SUCCESS;

    const uint16_t el_idx  = this->parent_element->get_element_idx();
    const uint16_t type_id = (uint16_t)this->parent_element->get_element_variant();
    const auto*    el_ctx  = get_common_ctx(this->parent_element);

    MESHX_LOGI(MODULE_ID_BLE_MESH_ELEMENT,
        "RelayServer [%d] func_id=0x%02x: cmd from 0x%04x (ack_req=%d)",
        el_idx, this->get_func_id(), ctx->src_addr, ctx->ack_req);

    /* Step 1: Unicast ACK (REQ-006) */
    if (ctx->ack_req) {
        this->physical_model->send_with_func_id(
            ctx->src_addr, type_id, this->get_func_id(),
            param, (uint16_t)param_size, false);
    }

    /* Step 2: Publish (REQ-006) */
    if (el_ctx &&
        el_ctx->pub_addr != MESHX_ADDR_UNASSIGNED &&
        el_ctx->pub_addr != ctx->src_addr) {
        this->physical_model->send_with_func_id(
            el_ctx->pub_addr, type_id, this->get_func_id(),
            param, (uint16_t)param_size, false);
    }

    /* Step 3: App telemetry (REQ-006) */
    meshx_err_t err = meshx_send_msg_to_app(
        el_idx, type_id, this->get_func_id(), (uint16_t)param_size, param);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_BLE_MESH_ELEMENT,
            "RelayServer [%d]: telemetry failed: 0x%x", el_idx, err);
    }
    return MESHX_SUCCESS;
}

meshx_err_t meshXRelayServerModel::handle_timeout(const meshx_uvp_ctx_t* /*ctx*/)
{
    return MESHX_SUCCESS; /* Server no-op (REQ-008) */
}

/* =========================================================================
 * meshXLightCWWWClientModel
 * ========================================================================= */

meshx_err_t meshXLightCWWWClientModel::handle_rx(
    const void*            param,
    size_t                 param_size,
    const meshx_uvp_ctx_t* ctx)
{
    if (!ctx) return MESHX_SUCCESS;

    const uint16_t el_idx  = this->parent_element->get_element_idx();
    const uint16_t type_id = (uint16_t)this->parent_element->get_element_variant();

    if (ctx->src_addr == 0x0001u) {
        /* Host command path (REQ-007) */
        const auto* el_ctx = get_common_ctx(this->parent_element);
        if (!el_ctx || el_ctx->pub_addr == MESHX_ADDR_UNASSIGNED) {
            MESHX_LOGW(MODULE_ID_BLE_MESH_ELEMENT,
                "CWWWClient [%d] func_id=0x%02x: no pub_addr configured",
                el_idx, this->get_func_id());
            return MESHX_INVALID_STATE;
        }
        MESHX_LOGD(MODULE_ID_BLE_MESH_ELEMENT,
            "CWWWClient [%d] func_id=0x%02x: forwarding to 0x%04x",
            el_idx, this->get_func_id(), el_ctx->pub_addr);
        return this->physical_model->send_with_func_id(
            el_ctx->pub_addr, type_id, this->get_func_id(),
            param, (uint16_t)param_size, true);
    }

    /* BLE response path (REQ-007) */
    MESHX_LOGI(MODULE_ID_BLE_MESH_ELEMENT,
        "CWWWClient [%d] func_id=0x%02x: response from 0x%04x",
        el_idx, this->get_func_id(), ctx->src_addr);

    meshx_api_light_cwww_client_evt_t evt;
    std::memset(&evt, 0, sizeof(evt));
    evt.err_code = 0;
    if (param && param_size > 0) {
        size_t copy = std::min(param_size, sizeof(evt.state_change));
        std::memcpy(&evt.state_change, param, copy);
    }
    return meshx_send_msg_to_app(el_idx, type_id, this->get_func_id(), sizeof(evt), &evt);
}

meshx_err_t meshXLightCWWWClientModel::handle_timeout(const meshx_uvp_ctx_t* /*ctx*/)
{
    meshx_api_light_cwww_client_evt_t evt;
    std::memset(&evt, 0, sizeof(evt));
    evt.err_code = 1;
    MESHX_LOGW(MODULE_ID_BLE_MESH_ELEMENT,
        "CWWWClient [%d] func_id=0x%02x: TXCM timeout",
        this->parent_element->get_element_idx(), this->get_func_id());
    return meshx_send_msg_to_app(
        this->parent_element->get_element_idx(),
        (uint16_t)this->parent_element->get_element_variant(),
        this->get_func_id(), sizeof(evt), &evt);
}

/* =========================================================================
 * meshXLightCWWWServerModel
 * ========================================================================= */

meshx_err_t meshXLightCWWWServerModel::handle_rx(
    const void*            param,
    size_t                 param_size,
    const meshx_uvp_ctx_t* ctx)
{
    if (!ctx) return MESHX_SUCCESS;

    const uint16_t el_idx  = this->parent_element->get_element_idx();
    const uint16_t type_id = (uint16_t)this->parent_element->get_element_variant();
    const auto*    el_ctx  = get_common_ctx(this->parent_element);

    MESHX_LOGI(MODULE_ID_BLE_MESH_ELEMENT,
        "CWWWServer [%d] func_id=0x%02x: cmd from 0x%04x (ack_req=%d)",
        el_idx, this->get_func_id(), ctx->src_addr, ctx->ack_req);

    /* Step 1: Unicast ACK (REQ-006) */
    if (ctx->ack_req) {
        this->physical_model->send_with_func_id(
            ctx->src_addr, type_id, this->get_func_id(),
            param, (uint16_t)param_size, false);
    }

    /* Step 2: Publish (REQ-006) */
    if (el_ctx &&
        el_ctx->pub_addr != MESHX_ADDR_UNASSIGNED &&
        el_ctx->pub_addr != ctx->src_addr) {
        this->physical_model->send_with_func_id(
            el_ctx->pub_addr, type_id, this->get_func_id(),
            param, (uint16_t)param_size, false);
    }

    /* Step 3: App telemetry (REQ-006) */
    meshx_err_t err = meshx_send_msg_to_app(
        el_idx, type_id, this->get_func_id(), (uint16_t)param_size, param);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_BLE_MESH_ELEMENT,
            "CWWWServer [%d] func_id=0x%02x: telemetry failed: 0x%x",
            el_idx, this->get_func_id(), err);
    }
    return MESHX_SUCCESS;
}

meshx_err_t meshXLightCWWWServerModel::handle_timeout(const meshx_uvp_ctx_t* /*ctx*/)
{
    return MESHX_SUCCESS; /* Server no-op (REQ-008) */
}

/* =========================================================================
 * meshXSensorServerModel
 * ========================================================================= */

meshx_err_t meshXSensorServerModel::handle_rx(
    const void*            param,
    size_t                 param_size,
    const meshx_uvp_ctx_t* ctx)
{
    if (!ctx) return MESHX_SUCCESS;

    const uint16_t el_idx  = this->parent_element->get_element_idx();
    const uint16_t type_id = (uint16_t)this->parent_element->get_element_variant();
    const auto*    el_ctx  = get_common_ctx(this->parent_element);

    MESHX_LOGI(MODULE_ID_BLE_MESH_ELEMENT,
        "SensorServer [%d] func_id=0x%02x: cmd from 0x%04x (ack_req=%d)",
        el_idx, this->get_func_id(), ctx->src_addr, ctx->ack_req);

    if (ctx->ack_req) {
        this->physical_model->send_with_func_id(
            ctx->src_addr, type_id, this->get_func_id(),
            param, (uint16_t)param_size, false);
    }

    if (el_ctx &&
        el_ctx->pub_addr != MESHX_ADDR_UNASSIGNED &&
        el_ctx->pub_addr != ctx->src_addr) {
        this->physical_model->send_with_func_id(
            el_ctx->pub_addr, type_id, this->get_func_id(),
            param, (uint16_t)param_size, false);
    }

    meshx_err_t err = meshx_send_msg_to_app(
        el_idx, type_id, this->get_func_id(), (uint16_t)param_size, param);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_BLE_MESH_ELEMENT,
            "SensorServer [%d] func_id=0x%02x: telemetry failed: 0x%x",
            el_idx, this->get_func_id(), err);
    }
    return MESHX_SUCCESS;
}

meshx_err_t meshXSensorServerModel::handle_timeout(const meshx_uvp_ctx_t* /*ctx*/)
{
    return MESHX_SUCCESS;
}

/* =========================================================================
 * meshXSensorClientModel
 * ========================================================================= */

meshx_err_t meshXSensorClientModel::handle_rx(
    const void*            param,
    size_t                 param_size,
    const meshx_uvp_ctx_t* ctx)
{
    if (!ctx) return MESHX_SUCCESS;

    const uint16_t el_idx  = this->parent_element->get_element_idx();
    const uint16_t type_id = (uint16_t)this->parent_element->get_element_variant();

    if (ctx->src_addr == 0x0001u) {
        const auto* el_ctx = get_common_ctx(this->parent_element);
        if (!el_ctx || el_ctx->pub_addr == MESHX_ADDR_UNASSIGNED) {
            MESHX_LOGW(MODULE_ID_BLE_MESH_ELEMENT,
                "SensorClient [%d] func_id=0x%02x: no pub_addr configured",
                el_idx, this->get_func_id());
            return MESHX_INVALID_STATE;
        }
        MESHX_LOGD(MODULE_ID_BLE_MESH_ELEMENT,
            "SensorClient [%d] func_id=0x%02x: forwarding to 0x%04x",
            el_idx, this->get_func_id(), el_ctx->pub_addr);
        return this->physical_model->send_with_func_id(
            el_ctx->pub_addr, type_id, this->get_func_id(),
            param, (uint16_t)param_size, true);
    }

    MESHX_LOGI(MODULE_ID_BLE_MESH_ELEMENT,
        "SensorClient [%d] func_id=0x%02x: response from 0x%04x",
        el_idx, this->get_func_id(), ctx->src_addr);

    meshx_api_sensor_client_evt_t evt;
    std::memset(&evt, 0, sizeof(evt));
    evt.err_code = 0;
    if (param && param_size > 0) {
        size_t copy = std::min(param_size, sizeof(evt.state_change));
        std::memcpy(&evt.state_change, param, copy);
    }
    return meshx_send_msg_to_app(el_idx, type_id, this->get_func_id(), sizeof(evt), &evt);
}

meshx_err_t meshXSensorClientModel::handle_timeout(const meshx_uvp_ctx_t* /*ctx*/)
{
    meshx_api_sensor_client_evt_t evt;
    std::memset(&evt, 0, sizeof(evt));
    evt.err_code = 1;
    MESHX_LOGW(MODULE_ID_BLE_MESH_ELEMENT,
        "SensorClient [%d] func_id=0x%02x: TXCM timeout",
        this->parent_element->get_element_idx(), this->get_func_id());
    return meshx_send_msg_to_app(
        this->parent_element->get_element_idx(),
        (uint16_t)this->parent_element->get_element_variant(),
        this->get_func_id(), sizeof(evt), &evt);
}

/* =========================================================================
 * meshXLightHSLServerModel
 * ========================================================================= */

meshx_err_t meshXLightHSLServerModel::handle_rx(
    const void*            param,
    size_t                 param_size,
    const meshx_uvp_ctx_t* ctx)
{
    if (!ctx) return MESHX_SUCCESS;

    const uint16_t el_idx  = this->parent_element->get_element_idx();
    const uint16_t type_id = (uint16_t)this->parent_element->get_element_variant();
    const auto*    el_ctx  = get_common_ctx(this->parent_element);

    MESHX_LOGI(MODULE_ID_BLE_MESH_ELEMENT,
        "HSLServer [%d] func_id=0x%02x: cmd from 0x%04x (ack_req=%d)",
        el_idx, this->get_func_id(), ctx->src_addr, ctx->ack_req);

    if (ctx->ack_req) {
        this->physical_model->send_with_func_id(
            ctx->src_addr, type_id, this->get_func_id(),
            param, (uint16_t)param_size, false);
    }

    if (el_ctx &&
        el_ctx->pub_addr != MESHX_ADDR_UNASSIGNED &&
        el_ctx->pub_addr != ctx->src_addr) {
        this->physical_model->send_with_func_id(
            el_ctx->pub_addr, type_id, this->get_func_id(),
            param, (uint16_t)param_size, false);
    }

    meshx_err_t err = meshx_send_msg_to_app(
        el_idx, type_id, this->get_func_id(), (uint16_t)param_size, param);
    if (err != MESHX_SUCCESS) {
        MESHX_LOGE(MODULE_ID_BLE_MESH_ELEMENT,
            "HSLServer [%d] func_id=0x%02x: telemetry failed: 0x%x",
            el_idx, this->get_func_id(), err);
    }
    return MESHX_SUCCESS;
}

meshx_err_t meshXLightHSLServerModel::handle_timeout(const meshx_uvp_ctx_t* /*ctx*/)
{
    return MESHX_SUCCESS;
}

/* =========================================================================
 * meshXLightHSLClientModel
 * ========================================================================= */

meshx_err_t meshXLightHSLClientModel::handle_rx(
    const void*            param,
    size_t                 param_size,
    const meshx_uvp_ctx_t* ctx)
{
    if (!ctx) return MESHX_SUCCESS;

    const uint16_t el_idx  = this->parent_element->get_element_idx();
    const uint16_t type_id = (uint16_t)this->parent_element->get_element_variant();

    if (ctx->src_addr == 0x0001u) {
        const auto* el_ctx = get_common_ctx(this->parent_element);
        if (!el_ctx || el_ctx->pub_addr == MESHX_ADDR_UNASSIGNED) {
            MESHX_LOGW(MODULE_ID_BLE_MESH_ELEMENT,
                "HSLClient [%d] func_id=0x%02x: no pub_addr configured",
                el_idx, this->get_func_id());
            return MESHX_INVALID_STATE;
        }
        MESHX_LOGD(MODULE_ID_BLE_MESH_ELEMENT,
            "HSLClient [%d] func_id=0x%02x: forwarding to 0x%04x",
            el_idx, this->get_func_id(), el_ctx->pub_addr);
        return this->physical_model->send_with_func_id(
            el_ctx->pub_addr, type_id, this->get_func_id(),
            param, (uint16_t)param_size, true);
    }

    MESHX_LOGI(MODULE_ID_BLE_MESH_ELEMENT,
        "HSLClient [%d] func_id=0x%02x: response from 0x%04x",
        el_idx, this->get_func_id(), ctx->src_addr);

    meshx_api_light_hsl_client_evt_t evt;
    std::memset(&evt, 0, sizeof(evt));
    evt.err_code = 0;
    if (param && param_size > 0) {
        size_t copy = std::min(param_size, sizeof(evt.state_change));
        std::memcpy(&evt.state_change, param, copy);
    }
    return meshx_send_msg_to_app(el_idx, type_id, this->get_func_id(), sizeof(evt), &evt);
}

meshx_err_t meshXLightHSLClientModel::handle_timeout(const meshx_uvp_ctx_t* /*ctx*/)
{
    meshx_api_light_hsl_client_evt_t evt;
    std::memset(&evt, 0, sizeof(evt));
    evt.err_code = 1;
    MESHX_LOGW(MODULE_ID_BLE_MESH_ELEMENT,
        "HSLClient [%d] func_id=0x%02x: TXCM timeout",
        this->parent_element->get_element_idx(), this->get_func_id());
    return meshx_send_msg_to_app(
        this->parent_element->get_element_idx(),
        (uint16_t)this->parent_element->get_element_variant(),
        this->get_func_id(), sizeof(evt), &evt);
}
