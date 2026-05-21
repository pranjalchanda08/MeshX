/**
 * @file meshx_uvp_element.hpp
 * @brief Unified MeshX Element class for UVP protocol.
 */

#ifndef __MESHX_UVP_ELEMENT_HPP__
#define __MESHX_UVP_ELEMENT_HPP__

#include <meshx_element_class.hpp>
#include <meshx_uvp_model.hpp>
#include <meshx_uvp_logical_model.hpp>   /* meshXLogicalModel base class */
#include <meshx_uvp_logical_models.hpp>  /* Concrete model subclasses */
#include <vector>
#include <memory>

/**
 * @class meshXUVPElement
 * @brief A generic element that uses the Unified Vendor Protocol (UVP) model.
 */
class meshXUVPElement : public meshXElementServer
{
private:
    /**
     * @brief Element NVS context
     */
    meshx_element_common_ctx_t element_ctx;

    /**
     * @brief Logical model registry (REQ-001).
     * Populated by list_ven_models() after the physical model is baked.
     * Each entry handles one func_id on this element.
     */
    std::vector<std::unique_ptr<meshXLogicalModel>> logical_models;

public:
    /**
     * @brief Constructor
     * @param element_idx Element index
     * @param variant Element type
     */
    meshXUVPElement(uint16_t element_idx, meshx_element_type_t variant);

    /**
     * @brief Get list of supported SIG models
     * @return Number of SIG models
     */
    uint8_t list_sig_models() override { return 0; }

    /**
     * @brief Get list of supported Vendor models
     * @return Number of Vendor models
     */
    uint8_t list_ven_models() override;

    void sync(control_task_msg_evt_t evt) override;
    void handle_config(control_task_msg_evt_t evt, const meshx_config_srv_cb_param_t *params) override;

    /**
     * @brief Get the element name string.
     */
    const char* get_element_name(void) const override;
    
    /**
     * @brief Handle UVP state change notification and perform dual-routing (ACK + Pub).
     */
    meshx_err_t element_state_change_notify(meshx_ptr_t param, size_t param_size, const meshx_uvp_ctx_t* ctx) override;

    /* on_model_cb is final in meshXElement — not overridden here.
     * UVP data is dispatched via control_task_msg_publish_uvp in the port layer. */
};

#endif /* __MESHX_UVP_ELEMENT_HPP__ */
