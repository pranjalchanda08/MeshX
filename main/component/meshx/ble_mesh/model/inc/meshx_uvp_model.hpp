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
    meshXUVPModel(meshXElementIF* parent);

    /**
     * @return MESHX_MODEL_ID_UVP (UVP ID)
     */
    uint16_t get_model_id(void) const override;

    /**
     * @return MESHX_COMPANY_ID_UVP (MeshX CID)
     */
    uint16_t get_company_id(void) const;

#if CONFIG_TXCM_ENABLE
#include <meshx_txcm.h>

    struct uvp_txcm_param_t {
        void* p_model;
        uint16_t dst_addr;
        uint16_t type_id;
        uint16_t payload_len;
        uint16_t app_idx;
        bool ack_req;
        uint8_t payload[MESHX_TXCM_MSG_PARAM_MAX_LEN - sizeof(void*) - 4 * sizeof(uint16_t) - sizeof(bool)];
    };
#endif

    /**
     * @brief Send a UVP message from this model.
     * 
     * @param dst_addr      Destination address.
     * @param type_id       Target element Type ID.
     * @param payload       TLV payload.
     * @param payload_len   Payload length.
     * @param ack_req       Whether an ACK is requested.
     * @param app_idx       App key index.
     * @return MESHX_SUCCESS on success, error code otherwise.
     */
    meshx_err_t send(uint16_t dst_addr, uint16_t type_id, const void *payload, uint16_t payload_len, bool ack_req = false, uint16_t app_idx = 0);

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
     * @param app_idx     App key index.
     * @return MESHX_SUCCESS on success, error code otherwise
     */
    meshx_err_t send_with_func_id(uint16_t dst_addr, uint16_t type_id, uint16_t func_id, const void* payload, uint16_t payload_len, bool ack_req = false, uint16_t app_idx = 0);

    /**
     * @brief Create the platform-specific model instance.
     * 
     * For ESP-IDF, this populates the model_id with the combined 32-bit 
     * value (CID << 16 | ID).
     *
     * @param p_baked_model Pointer to the allocated platform model structure.
     * @return MESHX_SUCCESS on success.
     */
    meshx_err_t plat_model_create(MESHX_MODEL* p_baked_model) override;

    /**
     * @brief Delete the platform-specific model instance.
     * @return MESHX_SUCCESS on success.
     */
    meshx_err_t plat_model_delete(void) override;

    /**
     * @brief Handle element state changes.
     * @return MESHX_SUCCESS on success.
     */
    meshx_err_t element_state_change_handle(void) override;

    /**
     * @brief Prepare an element message.
     * @param msg_ptr Pointer to the message pointer.
     * @param msg_size Pointer to the message size.
     * @return MESHX_SUCCESS on success.
     */
    meshx_err_t prepare_element_msg(meshx_ptr_t *msg_ptr, size_t *msg_size) override;

    /**
     * @brief Check if the model is initialized.
     * @return true if initialized, false otherwise.
     */
    bool is_initialized(void) const override;
};

#endif /* __MESHX_UVP_MODEL_HPP__ */
