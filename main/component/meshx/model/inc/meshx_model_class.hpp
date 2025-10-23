/**
 * @file meshx_model_class.hpp
 * @brief Template declarations for MeshX model wrapper classes
 *
 * This file contains the template declarations for the wrapper classes that
 * provide a convenient interface around the MeshX base model classes. It includes
 * the base wrapper (meshXModel) and specialized wrappers for server and client models.
 *
 * Key Features:
 * - Template-based wrapper architecture
 * - Unified interface for both client and server models
 * - Type-safe model creation and management
 * - Simplified integration with platform-specific implementations
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#include <meshx_base_model_class.hpp>

/*********************************************************************************
 * meshXModel
 *********************************************************************************/
/**
 * @brief meshXModel class
 * @details This is a base class for both client and server models.
 */
MESHX_MODEL_TEMPLATE_PROTO
class meshXModel
{
private:
    /* private members */
    meshXElementIF *parent_element;         /*<! Pointer to the parent element interface */
    meshxBaseModel_t *base_model;           /*<! Pointer to the base model */

    meshx_err_t status;                     /*<! Status of the model */
    MESHX_MODEL *p_plat_model;              /*<! Pointer to the platform model */
    meshx_model_interface_t *model_intr;    /*<! Pointer to the model interface */

public:
    /***********************************************************
     * Virtual Functions
     ***********************************************************/
    /**
     * @brief A virtual function to be implemented by derived classes which shall be used
     *        to handle upstream events from BLE MESH.
     * @note  The defination is only required for the callback to be autoatically registered by
     *        respective base_model->base_client_model_cb_list
     * @relates meshXModel(MESHX_MODEL *p_plat_model, uint32_t model_id)
     */
    virtual meshx_err_t model_from_ble_cb(dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) = 0;
    /**
     * @brief A virtual function to be implemented by derived classes which shall be used to
     *        send a message to the platform model
     */
    virtual meshx_err_t send_packet(meshx_send_packet_params_t *params) = 0;
    /**
     * @brief A virtual function to be implemented by derived classes which shall be used to
     *        create a logical model
     */
    virtual meshx_err_t plat_model_create(void) = 0;

    /***********************************************************
     * Functions
     ***********************************************************/
    meshx_err_t get_init_status(void) const { return status; }
    MESHX_MODEL * get_plat_model(void) const { return p_plat_model; }
    meshxBaseModel_t * get_base_model(void) const { return base_model; }
    meshx_model_interface_t * get_model_intr(void) const { return model_intr; }

    void set_parent_element(meshXElementIF *parent) { parent_element = parent; }
    meshXElementIF * get_parent_element(void) const { return parent_element; }

    meshXModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element = nullptr);
    ~meshXModel() = default;
};

/*********************************************************************************
 * meshXServerModel
 *********************************************************************************/
/**
 * @class meshXServerModel class
 * @details This is a base class for server models.
 *
 * @tparam meshxBaseServerModel_t is a meshXBaseServerModel type Class for server models
 * @tparam meshx_send_packet_params_t is a meshx_send_packet_params_t structure
 */
MESHX_SERVER_MODEL_TEMPLATE_PROTO
class meshXServerModel : public meshXModel MESHX_SERVER_MODEL_TEMPLATE_PARAMS
{
public:
    meshXServerModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element = nullptr);
    ~meshXServerModel() = default;
    meshXServerModel() = delete;
};

/*********************************************************************************
 * meshXClientModel
 *********************************************************************************/
/**
 * @class meshXClientModel class
 * @details This is a base class for client models.
 *
 * @tparam meshxBaseClientModel_t is a meshXBaseClientModel type Class for client models
 * @tparam meshx_send_packet_params_t is a meshx_send_packet_params_t structure
 */
MESHX_CLIENT_MODEL_TEMPLATE_PROTO
class meshXClientModel : public meshXModel MESHX_CLIENT_MODEL_TEMPLATE_PARAMS
{
public:

    meshx_err_t plat_model_create(void) final;
    meshXClientModel(MESHX_MODEL *p_plat_model, uint32_t model_id, meshXElementIF *parent_element = nullptr);
    meshXClientModel() = delete;
    ~meshXClientModel();
};
