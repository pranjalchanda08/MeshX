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


#if CONFIG_TXCM_ENABLE
#include <meshx_txcm.h>

struct uvp_txcm_param_t {
    void* p_model;
    uint16_t dst_addr;
    uint16_t type_id;
    uint16_t payload_len;
    bool ack_req;
    uint8_t payload[MESHX_TXCM_MSG_PARAM_MAX_LEN - sizeof(void*) - 3 * sizeof(uint16_t) - sizeof(bool)];
};

static inline meshx_err_t uvp_txcm_send_wrapper(meshx_cptr_t msg_param, size_t msg_param_len) {
    if (!msg_param || msg_param_len < sizeof(uvp_txcm_param_t)) {
        return MESHX_INVALID_ARG;
    }
    const auto* param = static_cast<const uvp_txcm_param_t*>(msg_param);
    return meshx_uvp_send(param->p_model, param->dst_addr, param->type_id, param->payload, param->payload_len, param->ack_req);
}
#endif

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
        
#if CONFIG_TXCM_ENABLE
        if (payload_len <= (MESHX_TXCM_MSG_PARAM_MAX_LEN - sizeof(void*) - 3 * sizeof(uint16_t) - sizeof(bool))) {
            uvp_txcm_param_t param;
            param.p_model = (void*)p_model;
            param.dst_addr = dst_addr;
            param.type_id = type_id;
            param.payload_len = payload_len;
            param.ack_req = ack_req;
            if (payload && payload_len > 0) {
                memcpy(param.payload, payload, payload_len);
            }
            meshx_txcm_sig_t request_type = ack_req ? MESHX_TXCM_SIG_ENQ_SEND : MESHX_TXCM_SIG_DIRECT_SEND;
            return meshx_txcm_request_send(request_type, dst_addr, &param, sizeof(param), uvp_txcm_send_wrapper);
        }
#endif
        return meshx_uvp_send((void*)p_model, dst_addr, type_id, payload, payload_len, ack_req);
    }

    /**
     * @brief Send a UVP message with func_id prepended as a 2-byte LE wire prefix.
     *
     * Wire layout: [ func_id (2 B, LE) | payload (N B) ]
     *
     * The receiver dispatcher strips the prefix and populates ctx->func_id before
     * invoking the element callback. See REQ-004 and meshx_uvp_dispatcher.cpp.
     *
     * @param dst_addr    Destination unicast or group address
     * @param type_id     Element type ID (variant)
     * @param func_id     Function ID to embed at the wire payload prefix
     * @param payload     Application payload (may be nullptr if payload_len == 0)
     * @param payload_len Application payload length in bytes
     * @param ack_req     Whether to request an ACK from the destination
     * @return MESHX_SUCCESS on success, error code otherwise
     */
    meshx_err_t send_with_func_id(uint16_t    dst_addr,
                                  uint16_t    type_id,
                                  uint16_t    func_id,
                                  const void* payload,
                                  uint16_t    payload_len,
                                  bool        ack_req = false)
    {
        /* Allocate wire buffer: [func_id (2B)] + [payload (N B)] */
        const uint16_t wire_len = static_cast<uint16_t>(sizeof(uint16_t) + payload_len);

        /* Use stack buffer for common small payloads to avoid heap overhead */
        uint8_t stack_buf[64];
        uint8_t* wire = (wire_len <= static_cast<uint16_t>(sizeof(stack_buf)))
                            ? stack_buf
                            : static_cast<uint8_t*>(malloc(wire_len));
        if (!wire) return MESHX_NO_MEM;

        /* Prepend func_id in little-endian byte order */
        wire[0] = static_cast<uint8_t>(func_id & 0xFFu);
        wire[1] = static_cast<uint8_t>((func_id >> 8u) & 0xFFu);

        if (payload && payload_len > 0) {
            memcpy(wire + sizeof(uint16_t), payload, payload_len);
        }

        meshx_err_t err = send(dst_addr, type_id, wire, wire_len, ack_req);

        if (wire != stack_buf) { free(wire); }
        return err;
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
