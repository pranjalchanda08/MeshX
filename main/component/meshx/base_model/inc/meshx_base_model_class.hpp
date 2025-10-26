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
 * @brief Base class for all mesh models
 *
 * This class serves as the foundation for all MeshX models, providing common
 * functionality and interfaces for both client and server models.
 *
 * @tparam meshXBaseT Template parameter for base model customization.
 *                    This should be a derived class that implements platform-specific behavior.
 * @tparam ble_mesh_send_msg_params_t BLE mesh message parameters type.
 *                                    This type defines the structure used for sending BLE mesh messages.
 *
 * @note This is an abstract base class that cannot be instantiated directly.
 *       Use meshXBaseServerModel or meshXBaseClientModel instead.
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
     * @brief Register BLE message callback for this model
     * @details Automatically called during model initialization to register a callback
     *          function that handles BLE messages for this model's ID.
     * @return MESHX_SUCCESS if registration successful, error code otherwise
     */
    meshx_err_t from_ble_reg_cb(void) const;

    /**
     * @brief Deregister BLE message callback for this model
     * @details Automatically called during model destruction to remove the callback
     *          function that handles BLE messages for this model's ID.
     * @return MESHX_SUCCESS if deregistration successful, error code otherwise
     */
    meshx_err_t from_ble_dereg_cb(void) const;

public:
    /**
     * @brief Initialize platform model
     * @details Pure virtual function to be implemented by derived classes for platform-specific
     *          model initialization. This function is called during model construction.
     *
     * @tparam meshXBaseT Derived model type
     * @tparam ble_mesh_send_msg_params_t Type for BLE mesh send message parameters
     *
     * @return MESHX_SUCCESS on success, error code otherwise
     * @retval MESHX_SUCCESS Initialization successful
     * @retval MESHX_ERR_* Error code indicating reason for failure
     */
    virtual meshx_err_t plat_model_init(void) = 0;

    /**
     * @brief Send message through the model
     * @details Pure virtual function to be implemented by derived classes for sending messages
     *          through the BLE mesh network. The implementation should handle the actual
     *          transmission of the message to the BLE stack.
     *
     * @tparam meshXBaseT Derived model type
     * @tparam ble_mesh_send_msg_params_t Type for BLE mesh send message parameters
     *
     * @param[in] params Pointer to message parameters structure containing:
     *                   - opcode: Message opcode
     *                   - msg: Pointer to message data
     *                   - len: Length of message data
     *                   - dst: Destination address
     *                   - app_idx: Application key index
     *                   - ttl: Time to live value
     *
     * @return MESHX_SUCCESS on success, error code otherwise
     * @retval MESHX_SUCCESS Message sent successfully
     * @retval MESHX_ERR_INVALID_ARG Invalid parameters
     * @retval MESHX_ERR_NO_MEM Insufficient memory
     * @retval MESHX_ERR_INTERNAL Internal error
     */
    virtual meshx_err_t plat_send_msg(ble_mesh_send_msg_params_t *params) = 0;

    /**
     * @brief Get the current status of the model
     * @return Current status code
     */
    meshx_err_t get_status(void) const { return status; }

    /**
     * @brief Get the model identifier
     * @return Model ID value
     */
    uint32_t get_model_id(void) const { return model_id; }

    /**
     * @brief Get the BLE message callback function
     * @return Callback function for handling BLE messages
     */
    control_msg_cb get_from_ble_cb(void) const { return from_ble_cb; }

    /**
     * @brief Get the model type (server/client)
     * @return Model type enumeration value
     */
    meshXBaseModelType_t get_model_type(void) const { return model_type; }

    /**
     * @brief Set the model status
     * @param[in] err Status code to set
     */
    void set_status(meshx_err_t err) { status = err; }

    /**
     * @brief Set the model identifier
     * @param[in] id Model ID to set
     */
    void set_model_id(uint32_t id) { model_id = id; }

    /**
     * @brief Set the BLE message callback function
     * @param[in] cb Callback function for handling BLE messages
     */
    void set_from_ble_cb(const control_msg_cb& cb) { from_ble_cb = cb; }

    /**
     * @brief Set the model type
     * @param[in] type Model type (server/client) to set
     */
    void set_model_type(meshXBaseModelType_t type) { model_type = type; }

    /**
     * @brief Construct a new meshXBaseModel object
     * @param[in] model_id Model identifier
     * @param[in] from_ble_cb Callback for handling BLE messages
     * @param[in] model_type Type of the model (server/client)
     */
    meshXBaseModel(uint32_t model_id, const control_msg_cb &from_ble_cb, meshXBaseModelType_t model_type);
    meshXBaseModel() = delete;

    /**
     * @brief Virtual destructor for meshXBaseModel
     */
    virtual ~meshXBaseModel();
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
