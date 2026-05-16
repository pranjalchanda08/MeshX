/**
 * @file meshx_uvp_element.cpp
 * @brief Implementation of Unified MeshX Element for UVP.
 */

#include <variants/meshx_uvp_element.hpp>
#include <meshx_element_registry.hpp>
#include <meshx_api.h>

/**
 * @brief Constructor
 * @param element_idx Element index
 * @param variant Element type
 */
meshXUVPElement::meshXUVPElement(uint16_t element_idx, meshx_element_type_t variant)
    : meshXElementServer(element_idx, 0, 1) // 0 SIG, 1 Vendor
{
    memset(&element_ctx, 0, sizeof(element_ctx));
    element_ctx.pub_addr = MESHX_ADDR_UNASSIGNED;

    /* Restore from NVS if available */
    meshx_nvs_element_ctx_get(element_idx, variant, &element_ctx, sizeof(element_ctx));

    this->register_element_ctx(&element_ctx, sizeof(element_ctx));
    this->set_element_variant(variant);
}

/**
 * @brief Get list of supported Vendor models
 * @return Number of Vendor models
 */
uint8_t meshXUVPElement::list_ven_models()
{
    this->get_ven_models().push_back(std::make_unique<meshXUVPModel>(this));
    return 1;
}

/**
 * @brief Get element name
 * @return Element name
 */
const char* meshXUVPElement::get_element_name(void) const
{
    switch(get_element_variant()) {
        case MESHX_ELEMENT_TYPE_RELAY_SERVER: return "UVP Relay Server";
        case MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER: return "UVP CWWW Server";
        case MESHX_ELEMENT_TYPE_LIGHT_HSL_SERVER: return "UVP RGB Server";
        case MESHX_ELEMENT_TYPE_SENSOR_SERVER: return "UVP Sensor Server";
        default: return "UVP Generic Element";
    }
}

/**
 * @brief Sync control task message
 * @param evt Event
 */
void meshXUVPElement::sync(control_task_msg_evt_t evt)
{
    MESHX_LOGD(MODULE_ID_BLE_MESH_ELEMENT, "UVP Element [%d] sync: %d", get_element_idx(), evt);
}

/**
 * @brief Handle config task message
 * @param evt Event
 * @param params Parameters
 */
void meshXUVPElement::handle_config(control_task_msg_evt_t evt, const meshx_config_srv_cb_param_t *params)
{
    MESHX_LOGD(MODULE_ID_BLE_MESH_ELEMENT, "UVP Element [%d] config: %d", get_element_idx(), evt);
}

/* on_model_cb is final in meshXElement — base class implementation handles app notification.
 * No override needed here. */

