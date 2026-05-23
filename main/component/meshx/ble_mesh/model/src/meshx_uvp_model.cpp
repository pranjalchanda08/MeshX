#include <meshx_uvp_model.hpp>
#include <meshx_txcm.h>

/*
 * @brief Constructor
 *
 * @param parent Pointer to the parent element.
 */
meshXUVPModel::meshXUVPModel(meshXElementIF* parent) {
    set_parent_element(parent);
}

/*
 * @brief Get model ID
 *
 * @return Model ID
 */
uint16_t meshXUVPModel::get_model_id(void) const {
    return MESHX_MODEL_ID_UVP;
}

/*
 * @brief Get company ID
 *
 * @return Company ID
 */
uint16_t meshXUVPModel::get_company_id(void) const {
    return MESHX_COMPANY_ID_UVP;
}

#if CONFIG_TXCM_ENABLE
/**
 * @brief Wrapper function to send UVP message using TXCM
 *
 * @param msg_param Pointer to the message parameter.
 * @param msg_param_len Message parameter length.
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
static inline meshx_err_t uvp_txcm_send_wrapper(meshx_cptr_t msg_param, size_t msg_param_len) {
    if (!msg_param || msg_param_len < sizeof(meshXUVPModel::uvp_txcm_param_t)) {
        return MESHX_INVALID_ARG;
    }
    const auto* param = static_cast<const meshXUVPModel::uvp_txcm_param_t*>(msg_param);
    return meshx_uvp_send(param->p_model, param->dst_addr, param->type_id, param->payload, param->payload_len, param->ack_req, param->app_idx);
}
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
meshx_err_t meshXUVPModel::send(uint16_t dst_addr, uint16_t type_id, const void *payload, uint16_t payload_len, bool ack_req, uint16_t app_idx) {
    MESHX_MODEL *p_model = this->get_plat_model();
    if (!p_model) {
        return MESHX_INVALID_STATE;
    }

#if CONFIG_TXCM_ENABLE
    if (payload_len <= (MESHX_TXCM_MSG_PARAM_MAX_LEN - sizeof(void*) - 4 * sizeof(uint16_t) - sizeof(bool))) {
        meshXUVPModel::uvp_txcm_param_t param;
        param.p_model = (void*)p_model;
        param.dst_addr = dst_addr;
        param.type_id = type_id;
        param.payload_len = payload_len;
        param.app_idx = app_idx;
        param.ack_req = ack_req;
        if (payload && payload_len > 0) {
            memcpy(param.payload, payload, payload_len);
        }
        meshx_txcm_sig_t request_type = ack_req ? MESHX_TXCM_SIG_ENQ_SEND : MESHX_TXCM_SIG_DIRECT_SEND;
        return meshx_txcm_request_send(request_type, dst_addr, &param, sizeof(param), uvp_txcm_send_wrapper);
    }
#endif
    return meshx_uvp_send((void*)p_model, dst_addr, type_id, payload, payload_len, ack_req, app_idx);
}

/*
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
meshx_err_t meshXUVPModel::send_with_func_id(uint16_t dst_addr, uint16_t type_id, uint16_t func_id, const void* payload, uint16_t payload_len, bool ack_req, uint16_t app_idx) {
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

    meshx_err_t err = send(dst_addr, type_id, wire, wire_len, ack_req, app_idx);

    if (wire != stack_buf) { free(wire); }
    return err;
}

/*
 * @brief Create a platform-specific model instance.
 *
 * @param p_baked_model Pointer to the platform-specific model structure.
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshXUVPModel::plat_model_create(MESHX_MODEL* p_baked_model) {
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

    /* Bind the platform-specific opcodes to the model instance.
     * The implementation handles allocating/assigning the op array. */
    meshx_plat_bind_uvp_opcodes(p_baked_model);

    /*
     * Note: Opcode handling and callbacks are registered globally in the port layer
     * via esp_ble_mesh_register_custom_model_callback.
     */
    this->set_plat_model(p_baked_model);
    return MESHX_SUCCESS;
}

/*
 * @brief Delete a platform-specific model instance.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshXUVPModel::plat_model_delete(void) {
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

/*
 * @brief Handle element state changes.
 *
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshXUVPModel::element_state_change_handle(void) {
    return MESHX_SUCCESS;
}

/*
 * @brief Prepare an element message.
 *
 * @param msg_ptr   Pointer to the message pointer.
 * @param msg_size  Pointer to the message size.
 * @return MESHX_SUCCESS on success, error code otherwise.
 */
meshx_err_t meshXUVPModel::prepare_element_msg(meshx_ptr_t *msg_ptr, size_t *msg_size) {
    if (msg_ptr) *msg_ptr = nullptr;
    if (msg_size) *msg_size = 0;
    return MESHX_SUCCESS;
}

/**
 * @brief Check if the model is initialized.
 *
 * @return true if the model is initialized, false otherwise.
 */
bool meshXUVPModel::is_initialized(void) const {
    return true;
}
