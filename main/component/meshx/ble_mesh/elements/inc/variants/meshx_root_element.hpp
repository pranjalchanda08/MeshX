/**
 * @file meshx_root_element.hpp
 * @brief MeshX Root Element class definition
 * This file contains the meshXRootElement class which represents the root element
 * in the MeshX BLE mesh network.
 *
 * @author Pranjal Chanda
 * @date 2024-2025
 * @copyright Copyright 2024 - 2025 MeshX
 */

#ifndef __MESHX_ROOT_ELEMENT_HPP__
#define __MESHX_ROOT_ELEMENT_HPP__

#include <meshx_element_class.hpp>

#define MESHX_ROOT_ELEMENT_TEMPLATE_PROTO
#define MESHX_ROOT_ELEMENT_TEMPLATE_PARAMS

#define MESHX_ROOT_DEPENDENCY_RESOLVER_TEMPLATE_PROTO  template <typename ModelT>
#define MESHX_ROOT_DEPENDENCY_RESOLVER_TEMPLATE_PARAMS <ModelT>

/*********************************************************************************
 * meshXRootElement
 *********************************************************************************/
enum class meshxRootElementComposition : uint8_t
{
    MESHX_ROOT_ELEMENT_COMP_CONFIG_SERVER = 0,
    MESHX_ROOT_ELEMENT_COMP_MAX,
};

/**
 * @class meshXRootElement
 * @brief Derived class for the root element
 */
MESHX_ROOT_ELEMENT_TEMPLATE_PROTO
class meshXRootElement : public meshXElementServer MESHX_ROOT_ELEMENT_TEMPLATE_PARAMS
{
private:
    uint8_t list_sig_models() override;
    uint8_t list_ven_models() override;
    const char* get_element_name(void) const override;
    meshx_err_t element_state_change_notify(meshx_ptr_t param, size_t param_size) override;

    void sync(control_task_msg_evt_t evt) override;
    void handle_config(control_task_msg_evt_t evt, const meshx_config_srv_cb_param_t *params) override;

    MESHX_ROOT_DEPENDENCY_RESOLVER_TEMPLATE_PROTO
    void add_sig_model_if_present(uint16_t trigger_model_id, const char* log_name);

public:
    /**
     * @brief Constructs a new meshXRootElement instance.
     *
     * The meshXRootElement represents the primary element in the MeshX BLE mesh network.
     * It automatically initializes and configures all required SIG models for the root element,
     * including the Configuration Server model and other essential models.
     *
     * The constructor follows these steps:
     * 1. Creates the root element with the predefined index
     * 2. Determines the number of required SIG models
     * 3. Allocates memory for the SIG models
     * 4. Adds all required SIG models to the element
     */
    meshXRootElement();
};

#endif /* __MESHX_ROOT_ELEMENT_HPP__ */
