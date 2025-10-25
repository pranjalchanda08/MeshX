/**
 * Copyright © 2024 - 2025 MeshX
 *
 * @file meshx_base_model_class.hpp
 * @brief This file declares the meshXBaseModel class and its derived Client and Server classes.
 *
 * This file contains the meshXBaseModel class and its derived Client and Server classes.
 * The meshXBaseModel class is used for the meshXBaseModel.
 *
 * @author Pranjal Chanda
 */
#ifndef _MESHX_BASE_MODEL_CLASS_H_
#define _MESHX_BASE_MODEL_CLASS_H_

#include <meshx_fwd_decl.hpp>

/**************************************************************************************************************************************************************
 * meshXBaseModel
 * @brief This class is used for the meshXBaseModel.
 **************************************************************************************************************************************************************/
enum class meshXBaseModelType
{
    MESHX_BASE_MODEL_TYPE_SERVER,
    MESHX_BASE_MODEL_TYPE_CLIENT
};

using meshXBaseModelType_t = enum meshXBaseModelType;

using control_msg_cb = std::function<meshx_err_t(dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t)>;

/**
 * @class meshXBaseModel
 * @brief This class is used for the meshXBaseModel.
 *
 * @tparam ble_mesh_send_msg_params_t The type of the BLE mesh send message parameters.
 */
MESHX_BASE_TEMPLATE_PROTO
    class meshXBaseModel {
private:
    uint32_t model_id;
    control_msg_cb from_ble_cb;
    meshXBaseModelType_t model_type;
    /* meshx_err_t status to be used where return value is not used */
    meshx_err_t status = MESHX_SUCCESS;
    /**
     * @brief This function is used to register a callback function for a BLE message associated with the given model ID.
     * @note This function is self invoking and shall be called as soon as the model is initialized using meshXBaseModel(...)
     */
    meshx_err_t from_ble_reg_cb(void) const;
    /**
     * @brief This function is used to deregister a callback function for a BLE message associated with the given model ID.
     * @note This function is self invoking and shall be called as soon as the model is deinitialized using ~meshXBaseModel()
     */
    meshx_err_t from_ble_dereg_cb(void) const;

public:
    /**
     * @brief A virtual function to be implemented by derived classes which shall be used to initialize the platform model library
     *        and not actually create a logical model
     */
    virtual meshx_err_t plat_model_init(void) = 0;
    /**
     * @brief A virtual function to be implemented by derived classes which shall be used to send a message to the platform model
     */
    virtual meshx_err_t plat_send_msg(ble_mesh_send_msg_params_t *params) = 0;

    meshx_err_t get_status(void) const              { return status; }
    uint32_t get_model_id(void) const               { return model_id; }
    control_msg_cb get_from_ble_cb(void) const      { return from_ble_cb; }
    meshXBaseModelType_t get_model_type(void) const { return model_type; }

    void set_status(meshx_err_t err)               { status = err; }
    void set_model_id(uint32_t id)                 { model_id = id; }
    void set_from_ble_cb(const control_msg_cb& cb) { from_ble_cb = cb; }
    void set_model_type(meshXBaseModelType_t type) { model_type = type; }

    meshXBaseModel(uint32_t model_id, const control_msg_cb& from_ble_cb, meshXBaseModelType model_type);
    meshXBaseModel() = delete;
    ~meshXBaseModel();
};

/**************************************************************************************************************************************************************
 * meshXBaseServerModel
 * @brief This class is used for the meshXBaseServerModel.
 **************************************************************************************************************************************************************/
MESHX_BASE_SERVER_TEMPLATE_PROTO
    class meshXBaseServerModel : public meshXBaseModel<ble_mesh_send_msg_params_t> {
protected:
    static uint16_t plat_server_init;
    virtual meshx_err_t validate_server_status_opcode(uint16_t opcode) = 0;
public:
    virtual meshx_err_t server_state_restore(ble_mesh_plat_restore_params_t* param) = 0;
    meshXBaseServerModel() = delete;
    virtual ~meshXBaseServerModel() = default;
    meshXBaseServerModel(uint32_t model_id, const control_msg_cb& from_ble_cb);
};

/**************************************************************************************************************************************************************
 * meshXBaseClientModel
 * @brief This class is used for the meshXBaseClientModel.
 *************************************************************************************************************************************************************/

enum class meshx_base_cli_evt
{
    MESHX_BASE_CLI_EVT_GET = MESHX_BIT(0),
    MESHX_BASE_CLI_EVT_SET = MESHX_BIT(1),
    MESHX_BASE_CLI_PUBLISH = MESHX_BIT(2),
    MESHX_BASE_CLI_TIMEOUT = MESHX_BIT(3),
    MESHX_BASE_CLI_EVT_ALL = (       \
            MESHX_BASE_CLI_EVT_GET | \
            MESHX_BASE_CLI_EVT_SET | \
            MESHX_BASE_CLI_PUBLISH | \
            MESHX_BASE_CLI_TIMEOUT)
};

using meshx_base_cli_evt_t = enum meshx_base_cli_evt;

MESHX_BASE_CLIENT_TEMPLATE_PROTO
    class meshXBaseClientModel : public meshXBaseModel<ble_mesh_send_msg_params_t> {
private:
    using base_client_model_cb_reg_t = struct base_client_model_cb_reg
    {
        uint16_t model_id;   /**< Model ID associated with the registration. */
        control_msg_cb cb;   /**< Callback function associated with the registration. */
    };
    using base_client_model_resend_ctx_t = struct base_client_model_resend_ctx
    {
        uint16_t model_id;                   /**< Model ID associated with the re-sending. */
        ble_mesh_plat_model_cb_params_t param; /**< Params received from Platform callback */
    };
protected:
    /* Re-initialization protection by multiple client objects */
    static uint16_t plat_client_init;
    static std::forward_list<base_client_model_cb_reg_t> base_client_model_cb_list;

    /* Template type identification for debugging (RTTI-free) */
    static constexpr const char* get_client_type_name() {
        return __PRETTY_FUNCTION__;
    }

    /* Model validation function - to be implemented by derived classes */
    virtual meshx_err_t validate_client_model_id (uint32_t model_id) = 0;

    /* Per instance template based static functions */
    static meshx_err_t base_txcm_handle_ack     (uint16_t src_addr);
    static meshx_err_t base_txcm_handle_resend  (uint16_t model_id, const ble_mesh_plat_model_cb_params_t *param);
    static meshx_err_t base_from_ble_msg_handle (dev_struct_t *pdev, control_task_msg_evt_t evt, ble_mesh_plat_model_cb_params_t *params);
    static meshx_err_t base_handle_txcm_msg     (dev_struct_t *pdev, control_task_msg_evt_t evt, base_client_model_resend_ctx_t *param);
public:

    meshXBaseClientModel() = delete;
    ~meshXBaseClientModel() = default;
    meshXBaseClientModel(uint32_t model_id, const control_msg_cb& from_ble_cb);
};

#endif /* _MESHX_BASE_MODEL_CLASS_H_ */
