/**
 * @file meshx_uvp_model.hpp
 * @brief UVP Vendor Model implementation.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 */

#ifndef __MESHX_UVP_MODEL_HPP__
#define __MESHX_UVP_MODEL_HPP__

#include <meshx_model_class.hpp>
#include <meshx_uvp.h>
#include <interface/ble_mesh/meshx_ble_mesh_cmn.h>

/**
 * @class meshXUVPModel
 * @brief Vendor model for MeshX Unified Vendor Protocol (UVP).
 * 
 * This class provides a C++ wrapper for the mandatory UVP Vendor Model
 * that must be present on every element in a MeshX device.
 */
class meshXUVPModel : public meshXModelIF {
public:
    /**
     * @brief Construct a new meshXUVPModel object.
     * @param parent Pointer to the parent element.
     */
    meshXUVPModel(meshXElementIF* parent) {
        set_parent_element(parent);
    }

    /**
     * @return MESHX_MODEL_ID_UVP (UVP ID)
     */
    uint16_t get_model_id(void) const override { return MESHX_MODEL_ID_UVP; }

    /**
     * @return MESHX_COMPANY_ID_UVP (MeshX CID)
     */
    uint16_t get_company_id(void) const { return MESHX_COMPANY_ID_UVP; }

    /**
     * @brief Send a UVP message from this model.
     * 
     * @param dst_addr      Destination address.
     * @param el_idx        Target element index on destination.
     * @param type_id       Target element Type ID.
     * @param payload       TLV payload.
     * @param payload_len   Payload length.
     * @return MESHX_SUCCESS on success, error code otherwise.
     */
    meshx_err_t send(uint16_t dst_addr, uint16_t type_id, const void *payload, uint16_t payload_len, bool ack_req = false) {
        MESHX_MODEL *p_model = this->get_plat_model();
        if (!p_model) return MESHX_INVALID_STATE;
        
        return meshx_uvp_send((void*)p_model, dst_addr, type_id, payload, payload_len, ack_req);
    }

    /**
     * @brief Create the platform-specific model instance.
     * 
     * For ESP-IDF, this populates the model_id with the combined 32-bit 
     * value (CID << 16 | ID).
     *
     * @param p_baked_model Pointer to the allocated platform model structure.
     * @return MESHX_SUCCESS on success.
     */
    meshx_err_t plat_model_create(MESHX_MODEL* p_baked_model) override {
        if (!p_baked_model) return MESHX_INVALID_ARG;
        
        /* Set vendor model ID (4 bytes: CID + ID) */
        uint32_t vnd_id = MESHX_VND_MODEL_ID_UVP;
        memcpy((void*)&p_baked_model->model_id, &vnd_id, sizeof(vnd_id));
        
        /* Create the publication context for the vendor model */
        meshx_ptr_t p_pub = nullptr;
        meshx_err_t err = meshx_plat_create_model_pub(&p_pub, 1);
        if (err != MESHX_SUCCESS) {
            return err;
        }
        meshx_ptr_t *p_baked_pub = (meshx_ptr_t *)&p_baked_model->pub;
        *p_baked_pub = p_pub;
        this->set_pub_struct(p_pub);
        
        /* 
         * Note: Opcode handling and callbacks are registered globally in the port layer 
         * via esp_ble_mesh_register_vendor_model_callback.
         */
        this->set_plat_model(p_baked_model);
        return MESHX_SUCCESS;
    }

    meshx_err_t plat_model_delete(void) override {
        MESHX_MODEL *p_model = this->get_plat_model();
        if (p_model) {
            meshx_ptr_t *p_baked_pub = (meshx_ptr_t *)&p_model->pub;
            *p_baked_pub = nullptr;
        }
        meshx_ptr_t p_pub = this->get_pub_struct();
        if (p_pub) {
            meshx_plat_del_model_pub(&p_pub);
            this->set_pub_struct(nullptr);
        }
        return MESHX_SUCCESS;
    }
    meshx_err_t element_state_change_handle(void) override { return MESHX_SUCCESS; }
    meshx_err_t prepare_element_msg(meshx_ptr_t *msg_ptr, size_t *msg_size) override { 
        if (msg_ptr) *msg_ptr = nullptr;
        if (msg_size) *msg_size = 0;
        return MESHX_SUCCESS; 
    }
    bool is_initialized(void) const override { return true; }
};

#endif /* __MESHX_UVP_MODEL_HPP__ */
