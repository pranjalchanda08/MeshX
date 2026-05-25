/**
 * @file meshx_uvp_element.cpp
 * @brief Implementation of Unified MeshX Element for UVP.
 *
 * Refactored (REQ-001) to use a composition-based logical model architecture.
 * The monolithic if-else variant routing in element_state_change_notify has been
 * replaced by a generic loop over logical_models that uses explicit func_id
 * matching via can_handle() (REQ-005).
 */

#include <variants/meshx_uvp_element.hpp>
#include <meshx_element_registry.hpp>
#include <meshx_api.h>

/* Func_id constants for each element variant (REQ-002) */
#define MESHX_FUNC_ID_RELAY_ON_OFF  0x00u
#define MESHX_FUNC_ID_CWWW_ON_OFF   0x00u
#define MESHX_FUNC_ID_CWWW_CTL     0x01u
#define MESHX_FUNC_ID_HSL_ON_OFF    0x00u
#define MESHX_FUNC_ID_HSL_HSL      0x01u
#define MESHX_FUNC_ID_SENSOR_DATA   0x00u

/**
 * @brief Constructor
 * @param element_idx Element index
 * @param variant     Element type
 */
meshXUVPElement::meshXUVPElement(uint16_t element_idx, meshx_element_type_t variant)
    : meshXElementServer(element_idx, 0, 1) // 0 SIG, 1 Vendor
{
    memset(&element_ctx, 0, sizeof(element_ctx));
    element_ctx.pub_addr = MESHX_ADDR_UNASSIGNED;

    this->register_element_ctx(&element_ctx, sizeof(element_ctx));
    this->set_element_variant(variant);

    if (MESHX_ELEMENT_TYPE_IS_CLIENT(variant)) {
        this->set_element_type(meshxElementType_t::MESHX_ELEMENT_TYPE_CLIENT);
    } else {
        this->set_element_type(meshxElementType_t::MESHX_ELEMENT_TYPE_SERVER);
    }
}

/**
 * @brief Compose logical models onto the physical UVP transport (REQ-001).
 *
 * Called by meshXComposition::bake() after the physical vendor model is allocated.
 * The physical model pointer is valid at this point.
 *
 * @return Number of physical vendor models (always 1 for UVP).
 */
uint8_t meshXUVPElement::list_ven_models()
{
    /* Allocate the single physical UVP vendor model */
    this->get_ven_models().push_back(std::make_unique<meshXUVPModel>(this));
    meshXUVPModel* phys = static_cast<meshXUVPModel*>(this->get_ven_models()[0].get());

    /* Compose logical models based on element variant (REQ-001, REQ-002) */
    switch (this->get_element_variant()) {
        case MESHX_ELEMENT_TYPE_RELAY_CLIENT:
            logical_models.push_back(
                std::make_unique<meshXRelayClientModel>(this, phys, MESHX_FUNC_ID_RELAY_ON_OFF));
            break;

        case MESHX_ELEMENT_TYPE_RELAY_SERVER:
            logical_models.push_back(
                std::make_unique<meshXRelayServerModel>(this, phys, MESHX_FUNC_ID_RELAY_ON_OFF));
            break;

        case MESHX_ELEMENT_TYPE_LIGHT_CWWW_CLIENT:
            logical_models.push_back(
                std::make_unique<meshXLightCWWWClientModel>(this, phys, MESHX_FUNC_ID_CWWW_ON_OFF));
            logical_models.push_back(
                std::make_unique<meshXLightCWWWClientModel>(this, phys, MESHX_FUNC_ID_CWWW_CTL));
            break;

        case MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER:
            logical_models.push_back(
                std::make_unique<meshXLightCWWWServerModel>(this, phys, MESHX_FUNC_ID_CWWW_ON_OFF));
            logical_models.push_back(
                std::make_unique<meshXLightCWWWServerModel>(this, phys, MESHX_FUNC_ID_CWWW_CTL));
            break;

        case MESHX_ELEMENT_TYPE_SENSOR_SERVER:
            logical_models.push_back(
                std::make_unique<meshXSensorServerModel>(this, phys, MESHX_FUNC_ID_SENSOR_DATA));
            break;

        case MESHX_ELEMENT_TYPE_SENSOR_CLIENT:
            logical_models.push_back(
                std::make_unique<meshXSensorClientModel>(this, phys, MESHX_FUNC_ID_SENSOR_DATA));
            break;

        case MESHX_ELEMENT_TYPE_LIGHT_HSL_SERVER:
            logical_models.push_back(
                std::make_unique<meshXLightHSLServerModel>(this, phys, MESHX_FUNC_ID_HSL_ON_OFF));
            logical_models.push_back(
                std::make_unique<meshXLightHSLServerModel>(this, phys, MESHX_FUNC_ID_HSL_HSL));
            break;

        case MESHX_ELEMENT_TYPE_LIGHT_HSL_CLIENT:
            logical_models.push_back(
                std::make_unique<meshXLightHSLClientModel>(this, phys, MESHX_FUNC_ID_HSL_ON_OFF));
            logical_models.push_back(
                std::make_unique<meshXLightHSLClientModel>(this, phys, MESHX_FUNC_ID_HSL_HSL));
            break;

        default:
            MESHX_LOGW(MODULE_ID_BLE_MESH_ELEMENT,
                "UVP Element [%d] variant=%d: no logical models registered",
                get_element_idx(), (int)get_element_variant());
            break;
    }

    return 1; /* Always 1 physical vendor model */
}

/**
 * @brief Get element name string.
 */
const char* meshXUVPElement::get_element_name(void) const
{
    switch(get_element_variant()) {
        case MESHX_ELEMENT_TYPE_RELAY_SERVER:      return "UVP Relay Server";
        case MESHX_ELEMENT_TYPE_RELAY_CLIENT:      return "UVP Relay Client";
        case MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER: return "UVP CWWW Server";
        case MESHX_ELEMENT_TYPE_LIGHT_CWWW_CLIENT: return "UVP CWWW Client";
        case MESHX_ELEMENT_TYPE_LIGHT_HSL_SERVER:  return "UVP RGB Server";
        case MESHX_ELEMENT_TYPE_LIGHT_HSL_CLIENT:  return "UVP RGB Client";
        case MESHX_ELEMENT_TYPE_SENSOR_SERVER:     return "UVP Sensor Server";
        case MESHX_ELEMENT_TYPE_SENSOR_CLIENT:     return "UVP Sensor Client";
        default:                                   return "UVP Generic Element";
    }
}

/**
 * @brief Sync control task message.
 */
void meshXUVPElement::sync(control_task_msg_evt_t evt)
{
    MESHX_LOGD(MODULE_ID_BLE_MESH_ELEMENT, "UVP Element [%d] sync: %d", get_element_idx(), evt);
}

/**
 * @brief Handle config task message.
 */
void meshXUVPElement::handle_config(control_task_msg_evt_t evt,
                                     const meshx_config_srv_cb_param_t *params)
{
    MESHX_LOGD(MODULE_ID_BLE_MESH_ELEMENT, "UVP Element [%d] config: %d", get_element_idx(), evt);
}

/**
 * @brief Route incoming packet or timeout to the matching logical model (REQ-001, REQ-005).
 *
 * Routing rules:
 *  - ctx->src_addr == MESHX_ADDR_UNASSIGNED → TXCM timeout; call handle_timeout() on ALL models.
 *  - Otherwise → call handle_rx() on the FIRST model for which can_handle() returns true.
 *
 * func_id is set by the dispatcher (REQ-003) — no param_size heuristics here.
 */
meshx_err_t meshXUVPElement::element_state_change_notify(
    meshx_ptr_t            param,
    size_t                 param_size,
    const meshx_uvp_ctx_t* ctx)
{
    if (!ctx) {
        return MESHX_SUCCESS;
    }

    const bool is_timeout = (ctx->src_addr == MESHX_ADDR_UNASSIGNED);

    if (logical_models.empty()) {
        /* No logical models — legacy / unregistered variant: fall through silently */
        MESHX_LOGW(MODULE_ID_BLE_MESH_ELEMENT,
            "UVP Element [%d]: no logical models — packet dropped (func_id=0x%04x)",
            get_element_idx(), ctx->func_id);
        return MESHX_SUCCESS;
    }

    if (is_timeout) {
        /* REQ-008: broadcast timeout to ALL logical models */
        MESHX_LOGW(MODULE_ID_BLE_MESH_ELEMENT,
            "UVP Element [%d]: TXCM timeout — broadcasting to %zu model(s)",
            get_element_idx(), logical_models.size());
        for (auto& model : logical_models) {
            model->handle_timeout(ctx);
        }
        return MESHX_SUCCESS;
    }

    /* Normal RX / host command: route to the first matching model (REQ-005) */
    for (auto& model : logical_models) {
        if (model->can_handle(param, param_size, ctx)) {
            MESHX_LOGD(MODULE_ID_BLE_MESH_ELEMENT,
                "UVP Element [%d]: func_id=0x%04x → model 0x%x",
                get_element_idx(), ctx->func_id, (uintptr_t)model.get());
            return model->handle_rx(param, param_size, ctx);
        }
    }

    MESHX_LOGW(MODULE_ID_BLE_MESH_ELEMENT,
        "UVP Element [%d]: no model matched func_id=0x%04x — packet dropped",
        get_element_idx(), ctx->func_id);
    return MESHX_SUCCESS;
}
