# Developer Guide: Writing Custom Models and Elements

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Writing Custom Models](#writing-custom-models)
   - [Model Class Hierarchy](#model-class-hierarchy)
   - [Server Model Implementation](#server-model-implementation)
   - [Client Model Implementation](#client-model-implementation)
   - [Key Virtual Functions](#key-virtual-functions)
4. [Writing Custom Elements](#writing-custom-elements)
   - [Element Class Hierarchy](#element-class-hierarchy)
   - [Server Element Implementation](#server-element-implementation)
   - [Client Element Implementation](#client-element-implementation)
   - [Element Context and State Management](#element-context-and-state-management)
5. [Complete Example](#complete-example)
6. [Best Practices](#best-practices)
7. [Common Patterns](#common-patterns)

---

## Overview

This guide provides comprehensive instructions for developers on how to create custom model and element implementations in the MeshX BLE mesh framework. The MeshX framework uses a layered architecture with clear separation between models (which handle BLE mesh protocol logic) and elements (which manage state and coordinate multiple models).

### Key Concepts

- **Models**: Handle BLE mesh protocol-specific operations, message processing, and communication with the BLE stack
- **Elements**: Manage state, coordinate multiple models, and provide persistence (NVS) for device state
- **Server Models**: Respond to requests from other devices in the mesh network
- **Client Models**: Initiate requests to other devices in the mesh network

---

## Architecture

### Model Class Hierarchy

```
meshXModelIF (interface)
    └── meshXModel (base template)
            ├── meshXServerModel (for server models)
            │       └── YourCustomServerModel
            └── meshXClientModel (for client models)
                    └── YourCustomClientModel
```

### Element Class Hierarchy

```
meshXElementIF (interface)
    └── meshXElement (base template)
            ├── meshXElementServer (for server elements)
            │       └── YourCustomServerElement
            └── meshXElementClient (for client elements)
                    └── YourCustomClientElement
```

### Base Model Integration

Models also integrate with platform-specific base model classes:

```
meshXBaseModel (platform abstraction)
    ├── meshXBaseServerModel
    │       └── meshXBaseGenericServerModel / meshXBaseLightServerModel
    └── meshXBaseClientModel
            └── meshXBaseGenericClientModel / meshXBaseLightClientModel
```

---

## Writing Custom Models

### Model Class Hierarchy

All custom models must inherit from either [`meshXServerModel`](main/component/meshx/model/inc/meshx_model_class.hpp:304) or [`meshXClientModel`](main/component/meshx/model/inc/meshx_model_class.hpp:365), which in turn inherit from [`meshXModel`](main/component/meshx/model/inc/meshx_model_class.hpp:154).

### Server Model Implementation

#### Step 1: Define the Header File (.hpp)

Create a header file that defines your model class, state structure, and message structure.

```cpp
/**
 * @file meshx_model_custom.hpp
 * @brief Custom Model implementation for MeshX
 */

#ifndef _MESHX_MODEL_CUSTOM_HPP_
#define _MESHX_MODEL_CUSTOM_HPP_

#include <meshx_model_class.hpp>
#include <meshx_base_model_generic.hpp>  // or meshx_base_model_light.hpp for light models

// Template macros (required for template instantiation)
#define MESHX_CUSTOM_SERVER_MODEL_TEMPLATE_PROTO
#define MESHX_CUSTOM_SERVER_MODEL_TEMPLATE_PARAMS

/**
 * @brief State structure for your custom model
 * @details This structure holds the model's state values
 */
struct meshx_custom_model_state
{
    uint8_t  value1;    /**< First state value */
    uint16_t value2;   /**< Second state value */
    // Add more state fields as needed
};

using meshx_custom_model_state_t = struct meshx_custom_model_state;

/**
 * @brief Parameters for sending messages through your custom model
 */
struct meshx_custom_send_params
{
    meshx_model_t                  *model;  /**< Pointer to the model */
    meshx_ctx_t                    *ctx;    /**< Message context */
    meshx_custom_model_state_t      state;  /**< State to send */
    // Add more parameters as needed
};

using meshx_custom_send_params_t = struct meshx_custom_send_params;

/**
 * @brief Message structure for communication with parent element
 */
struct meshx_custom_srv_el_msg
{
    meshx_srv_model_send_param_header_t header; /**< Server model header */
    meshx_custom_model_state_t              state;  /**< State data */
};

using meshx_custom_srv_el_msg_t = struct meshx_custom_srv_el_msg;

/**
 * @class meshXCustomServerModel
 * @brief Your custom server model implementation
 */
MESHX_CUSTOM_SERVER_MODEL_TEMPLATE_PROTO
class meshXCustomServerModel : public meshXServerModel<meshXBaseGenericServerModel, meshx_custom_send_params_t>
{
private:
    /* Model state - updated from BLE layer */
    meshx_custom_model_state_t model_state;

    /* Message to send to parent element - stored as member to persist */
    meshx_custom_srv_el_msg_t element_msg;

    /* Flag to indicate if message was prepared for element notification */
    bool element_msg_prepared;

    /* Platform-specific model creation/deletion */
    meshx_err_t plat_model_create(void) override;
    meshx_err_t plat_model_delete(void) override;

public:
    /* Required virtual functions */
    meshx_err_t model_send(meshx_custom_send_params_t *params) override;
    meshx_err_t model_from_ble_cb(dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;
    meshx_err_t prepare_element_msg(meshx_ptr_t *msg_ptr, size_t *msg_size) override;
    meshx_err_t element_state_change_handle(void) override;

    /* Constructor */
    explicit meshXCustomServerModel(
        meshXElementIF *parent_element = nullptr,
        meshx_ptr_t     parent_element_state = nullptr
    );

    ~meshXCustomServerModel() override = default;
};

#endif /* _MESHX_MODEL_CUSTOM_HPP_ */
```

#### Step 2: Implement the Source File (.cpp)

```cpp
/**
 * @file meshx_model_custom.cpp
 * @brief Implementation of Custom Model for MeshX
 */

#include <generic_model/meshx_model_custom.hpp>

/*********************************************************************************
 * Constructor
 *********************************************************************************/

MESHX_CUSTOM_SERVER_MODEL_TEMPLATE_PROTO
meshXCustomServerModel MESHX_CUSTOM_SERVER_MODEL_TEMPLATE_PARAMS
::meshXCustomServerModel(meshXElementIF *parent_element, meshx_ptr_t parent_element_state)
    : meshXServerModel(nullptr, YOUR_MODEL_ID, parent_element, parent_element_state)
{
    // Initialize model state to default values
    model_state.value1 = 0;
    model_state.value2 = 0;
    element_msg_prepared = false;
}

/*********************************************************************************
 * Platform Model Creation/Deletion
 *********************************************************************************/

/**
 * @brief Creates the platform-specific model instance
 * @details This function initializes the model on the BLE mesh platform
 * @return MESHX_SUCCESS on success, error code otherwise
 */
MESHX_CUSTOM_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXCustomServerModel MESHX_CUSTOM_SERVER_MODEL_TEMPLATE_PARAMS
::plat_model_create(void)
{
    meshx_ptr_t p_pub = this->get_pub_struct();
    meshx_ptr_t p_gen = this->get_gen_struct();
    meshx_err_t err = MESHX_SUCCESS;

    // Call platform-specific creation function
    // You need to implement this function in the platform layer
    err = meshx_plat_custom_srv_create(this->get_plat_model(), &p_pub, &p_gen);

    if(err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to create Custom Server Model");
    }
    else
    {
        /* Set the publication and generic structures */
        this->set_pub_struct(p_pub);
        this->set_gen_struct(p_gen);
    }
    return err;
}

/**
 * @brief Deletes the platform-specific model instance
 * @details This function cleans up the model from the BLE mesh platform
 * @return MESHX_SUCCESS on success, error code otherwise
 */
MESHX_CUSTOM_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXCustomServerModel MESHX_CUSTOM_SERVER_MODEL_TEMPLATE_PARAMS
::plat_model_delete(void)
{
    meshx_ptr_t p_pub = this->get_pub_struct();
    meshx_ptr_t p_gen = this->get_gen_struct();

    // Call platform-specific deletion function
    meshx_err_t err = meshx_plat_custom_srv_delete(&p_pub, &p_gen);

    if (err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to delete Custom Server Model");
    }
    else
    {
        /* Set the publication and generic structures to NULL */
        this->set_pub_struct(nullptr);
        this->set_gen_struct(nullptr);
    }

    return err;
}

/*********************************************************************************
 * BLE Callback Handler
 *********************************************************************************/

/**
 * @brief Handle BLE mesh messages from the network
 * @details This function is called when a message is received from the BLE mesh network
 * @param[in] p_dev     Device structure containing sender information
 * @param[in] model_id  Event type/model ID
 * @param[in] params    Event-specific data payload
 * @return MESHX_SUCCESS if handled successfully, error code otherwise
 */
MESHX_CUSTOM_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXCustomServerModel MESHX_CUSTOM_SERVER_MODEL_TEMPLATE_PARAMS
::model_from_ble_cb(
    dev_struct_t *p_dev,
    control_task_msg_evt_t model_id,
    meshx_ptr_t params)
{
    if(!params || !p_dev)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameters");
        return MESHX_INVALID_ARG;
    }

    if(model_id != this->get_model_id())
    {
        /* Callback triggered not for this model */
        return MESHX_SUCCESS;
    }

    auto *param = static_cast<meshx_gen_srv_cb_param_t *>(params);

    MESHX_LOGD(MODULE_ID_MODEL_SERVER, "op|src|dst:%04" PRIx32 "|%04x|%04x",
               param->ctx.opcode, param->ctx.src_addr, param->ctx.dst_addr);

    // Update model state from received message
    model_state.value1 = param->state_change.custom_set.value1;
    model_state.value2 = param->state_change.custom_set.value2;

    // Initialize flag - message not prepared yet
    element_msg_prepared = false;

    bool send_reply = (param->ctx.opcode != YOUR_OPCODE_SET_UNACK);

    switch (param->ctx.opcode)
    {
        case YOUR_OPCODE_GET:
            // GET opcode - no state change, no element notification needed
            return MESHX_NOT_SUPPORTED;

        case YOUR_OPCODE_SET:
        case YOUR_OPCODE_SET_UNACK:
        {
            // Check if message is for this element
            if (MESHX_ADDR_IS_UNICAST(param->ctx.dst_addr)
            || (MESHX_ADDR_BROADCAST(param->ctx.dst_addr))
            || (MESHX_ADDR_IS_GROUP(param->ctx.dst_addr)
            && (MESHX_SUCCESS == meshx_is_group_subscribed(&param->model, param->ctx.dst_addr))))
            {
                // Prepare the message (store in member variable)
                element_msg = {
                    .header = {
                        .model = param->model,
                        .element_state_change = MESHX_SUCCESS,  // Will be set by base layer
                    },
                    .state = model_state,
                };
                element_msg_prepared = true;
            }
            break;
        }
        default:
            // Unknown opcode - return error
            return MESHX_NOT_SUPPORTED;
    }

    // Send reply if needed
    if (send_reply
        || param->ctx.src_addr != param->model.pub_addr)
    {
        /* Send response to the requesting client */
        MESHX_LOGD(MODULE_ID_MODEL_SERVER, "PUB: src|pub %x|%x", param->ctx.src_addr, param->model.pub_addr);
        param->ctx.dst_addr = param->model.pub_addr;

        // Create parameter structure for sending the response
        meshx_custom_send_params_t send_params = {
            .model  = &param->model,
            .ctx    = &param->ctx,
            .state  = model_state,
        };

        return this->model_send(&send_params);
    }

    // If we reach here, message was not prepared for element notification
    if (!element_msg_prepared)
    {
        MESHX_LOGD(MODULE_ID_MODEL_SERVER, "No element notification needed for opcode: %04x", param->ctx.opcode);
        return MESHX_NOT_SUPPORTED;
    }

    return MESHX_SUCCESS;
}

/*********************************************************************************
 * Element State Change Handler
 *********************************************************************************/

/**
 * @brief Handle state change request from element
 * @details This function is called when the element requests a state change
 * @return MESHX_SUCCESS on success, error code otherwise
 */
MESHX_CUSTOM_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXCustomServerModel MESHX_CUSTOM_SERVER_MODEL_TEMPLATE_PARAMS
::element_state_change_handle()
{
    auto *el_state = static_cast<meshx_custom_model_state_t*>(this->get_parent_element_state());

    if (!el_state) {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameter in element_state_change_handle");
        return MESHX_INVALID_ARG;
    }

    // Update element state if it has changed
    if(el_state->value1 != model_state.value1 || el_state->value2 != model_state.value2)
    {
        el_state->value1 = model_state.value1;
        el_state->value2 = model_state.value2;
    }
    else
    {
        return MESHX_INVALID_STATE;
    }
    return MESHX_SUCCESS;
}

/*********************************************************************************
 * Message Preparation
 *********************************************************************************/

/**
 * @brief Prepare message for element notification
 * @details This function prepares the message structure that will be sent to the parent element
 * @param[out] msg_ptr   Pointer to message structure (output parameter)
 * @param[out] msg_size  Size of the message structure (output parameter)
 * @return MESHX_SUCCESS if message prepared successfully, error code otherwise
 */
MESHX_CUSTOM_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXCustomServerModel MESHX_CUSTOM_SERVER_MODEL_TEMPLATE_PARAMS
::prepare_element_msg(meshx_ptr_t *msg_ptr, size_t *msg_size)
{
    if (!msg_ptr || !msg_size)
    {
        return MESHX_INVALID_ARG;
    }

    if (!element_msg_prepared)
    {
        // Message was not prepared, return error
        return MESHX_NOT_SUPPORTED;
    }

    *msg_ptr = &element_msg;
    *msg_size = sizeof(element_msg);

    return MESHX_SUCCESS;
}

/*********************************************************************************
 * Model Send
 *********************************************************************************/

/**
 * @brief Send a message through the model
 * @details This function sends a message to the BLE mesh network
 * @param[in] params Pointer to send parameters structure
 * @return MESHX_SUCCESS on success, error code otherwise
 */
MESHX_CUSTOM_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXCustomServerModel MESHX_CUSTOM_SERVER_MODEL_TEMPLATE_PARAMS
::model_send(meshx_custom_send_params_t *params)
{
    if (!params || !params->model || !params->ctx)
    {
        return MESHX_INVALID_ARG;
    }

    params->ctx->opcode = YOUR_OPCODE_STATUS;

    meshx_gen_server_send_params_t send_params = {
        .p_model        = params->model,
        .p_ctx          = params->ctx,
        .state_change   = {
            .custom_set = {
                .value1 = params->state.value1,
                .value2 = params->state.value2,
            }
        },
        .data_len       = sizeof(meshx_state_change_custom_set_t)
    };

    return this->get_base_model()->plat_send_msg(&send_params);
}
```

### Client Model Implementation

Client models follow a similar pattern but with some key differences:

1. They inherit from [`meshXClientModel`](main/component/meshx/model/inc/meshx_model_class.hpp:365)
2. They don't implement `plat_model_create()` and `plat_model_delete()` (these are final in the base class)
3. They use a different message header structure ([`meshx_cli_model_send_param_header_t`](main/component/meshx/model/inc/meshx_model_class.hpp:347))
4. They handle timeout events

```cpp
/**
 * @class meshXCustomClientModel
 * @brief Your custom client model implementation
 */
MESHX_CUSTOM_CLIENT_MODEL_TEMPLATE_PROTO
class meshXCustomClientModel : public meshXClientModel<meshXBaseGenericClientModel, meshx_custom_send_params_t>
{
private:
    meshx_custom_model_state_t model_state;
    meshx_custom_cli_el_msg_t element_msg;

    meshx_err_t meshx_state_change_notify(const meshx_gen_cli_cb_param_t *param, uint8_t status);
    meshx_err_t element_state_change_handle(void) override;

public:
    meshx_err_t model_send(meshx_custom_send_params_t *params) override;
    meshx_err_t model_from_ble_cb(dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;
    meshx_err_t prepare_element_msg(meshx_ptr_t *msg_ptr, size_t *msg_size) override;

    explicit meshXCustomClientModel(
        meshXElementIF *parent_element = nullptr,
        meshx_ptr_t     parent_element_state = nullptr
    );

    ~meshXCustomClientModel() override = default;
};
```

### Key Virtual Functions

All models must implement these virtual functions from [`meshXModelIF`](main/component/meshx/model/inc/meshx_model_class.hpp:29):

#### 1. `plat_model_create()` (Server only)
Creates the platform-specific model instance. Call the platform-specific creation function and set the publication and generic structures.

#### 2. `plat_model_delete()` (Server only)
Deletes the platform-specific model instance and cleans up resources.

#### 3. `element_state_change_handle()`
Handles state change requests from the parent element. Update the element state if it has changed.

#### 4. `prepare_element_msg()`
Prepares a message structure that will be sent to the parent element. The message must persist after this function returns (store as member variable).

#### 5. `model_from_ble_cb()`
Handles messages and events coming from the BLE Mesh network. Process the received message, update model state, and prepare element notification if needed.

#### 6. `model_send()`
Sends messages through the model to the BLE Mesh network. Prepare the send parameters and call the base model's `plat_send_msg()`.

---

## Writing Custom Elements

### Element Class Hierarchy

All custom elements must inherit from either [`meshXElementServer`](main/component/meshx/elements/inc/meshx_element_class.hpp:293) or [`meshXElementClient`](main/component/meshx/elements/inc/meshx_element_class.hpp:306).

### Server Element Implementation

#### Step 1: Define the Header File (.hpp)

```cpp
/**
 * @file meshx_custom_element.hpp
 * @brief Custom Element class definition
 */

#ifndef __MESHX_CUSTOM_ELEMENT_HPP__
#define __MESHX_CUSTOM_ELEMENT_HPP__

#include <meshx_element_class.hpp>
#include <generic_model/meshx_model_custom.hpp>

// Template macros
#define MESHX_CUSTOM_SERVER_ELEMENT_TEMPLATE_PROTO
#define MESHX_CUSTOM_SERVER_ELEMENT_TEMPLATE_PARAMS

#if CONFIG_CUSTOM_SERVER_COUNT > 0

/**
 * @brief Custom server element context structure
 * @details This structure contains the state context for the custom server element,
 *          including model states and publication/app binding information.
 *          This matches the C implementation pattern where state is maintained
 *          in the element layer for NVS persistence.
 */
struct meshx_custom_srv_el_ctx_t
{
    // Publication and app binding
    uint8_t  app_id;        /**< Application key ID for publication */
    uint16_t pub_addr;      /**< Publication address */

    // Model states
    meshx_custom_model_state_t custom_state;  /**< Custom model state */
    // Add more model states as needed
};

using meshx_custom_srv_el_ctx_t = struct meshx_custom_srv_el_ctx_t;

/*********************************************************************************
 * meshXCustomServerElement
 *********************************************************************************/

MESHX_CUSTOM_SERVER_ELEMENT_TEMPLATE_PROTO
class meshXCustomServerElement : public meshXElementServer MESHX_CUSTOM_SERVER_ELEMENT_TEMPLATE_PARAMS
{
private:
    meshx_custom_srv_el_ctx_t element_ctx;

    uint8_t list_sig_models(void) override;
    uint8_t list_ven_models(void) override;

public:
    /**
     * @brief Constructs a new meshXCustomServerElement instance
     * @param element_idx The index of the element within the node
     */
    meshXCustomServerElement(uint16_t element_idx);

    meshXCustomServerElement(void) = delete;
};

#endif /* CONFIG_CUSTOM_SERVER_COUNT */

#endif /* __MESHX_CUSTOM_ELEMENT_HPP__ */
```

#### Step 2: Implement the Source File (.cpp)

```cpp
/**
 * @file meshx_custom_element.cpp
 * @brief Implementation of Custom Element for MeshX
 */

#include <variants/meshx_custom_element.hpp>

#if CONFIG_CUSTOM_SERVER_COUNT > 0
/*********************************************************************************
 * meshXCustomServerElement
 *********************************************************************************/

/**
 * @brief Constructs a new meshXCustomServerElement instance
 * @param element_idx The index of the element within the node
 */
MESHX_CUSTOM_SERVER_ELEMENT_TEMPLATE_PROTO
meshXCustomServerElement MESHX_CUSTOM_SERVER_ELEMENT_TEMPLATE_PARAMS
::meshXCustomServerElement(uint16_t element_idx)
    : meshXElementServer(element_idx)
{
    // Register element context for NVS persistence
    this->register_element_ctx(
        &element_ctx,
        sizeof(meshx_custom_srv_el_ctx_t)
    );
}

/**
 * @brief Lists and initializes SIG models for Custom Server Element
 * @return uint8_t Number of SIG models added to the element
 */
MESHX_CUSTOM_SERVER_ELEMENT_TEMPLATE_PROTO
uint8_t meshXCustomServerElement MESHX_CUSTOM_SERVER_ELEMENT_TEMPLATE_PARAMS
::list_sig_models()
{
    // Create Custom Server model
    auto custom_model = std::make_unique<meshXCustomServerModel>(
        this,
        &this->element_ctx.custom_state
    );
    this->get_sig_models().push_back(std::move(custom_model));

    // Add more models as needed
    // auto another_model = std::make_unique<meshXAnotherServerModel>(
    //     this,
    //     &this->element_ctx.another_state
    // );
    // this->get_sig_models().push_back(std::move(another_model));

    return (uint8_t)this->get_sig_models().size();
}

/**
 * @brief Lists vendor-specific models for Custom Server Element
 * @return uint8_t Number of vendor models
 */
MESHX_CUSTOM_SERVER_ELEMENT_TEMPLATE_PROTO
uint8_t meshXCustomServerElement MESHX_CUSTOM_SERVER_ELEMENT_TEMPLATE_PARAMS
::list_ven_models()
{
    // Add vendor models if needed
    return 0;
}

#endif /* CONFIG_CUSTOM_SERVER_COUNT */
```

### Client Element Implementation

Client elements follow the same pattern but inherit from [`meshXElementClient`](main/component/meshx/elements/inc/meshx_element_class.hpp:306):

```cpp
MESHX_CUSTOM_CLIENT_ELEMENT_TEMPLATE_PROTO
class meshXCustomClientElement : public meshXElementClient MESHX_CUSTOM_CLIENT_ELEMENT_TEMPLATE_PARAMS
{
private:
    meshx_custom_cli_el_ctx_t element_ctx;

    uint8_t list_sig_models(void) override;
    uint8_t list_ven_models(void) override;

public:
    meshXCustomClientElement(uint16_t element_idx);
    meshXCustomClientElement(void) = delete;
};
```

### Element Context and State Management

Elements maintain state in a context structure that is persisted to NVS (Non-Volatile Storage). This ensures that device state survives power cycles.

#### Key Points:

1. **Context Structure**: Define a context structure that contains all model states and publication/app binding information
2. **Registration**: Call [`register_element_ctx()`](main/component/meshx/elements/inc/meshx_element_class.hpp:125) in the constructor to register the context
3. **Model State Pointers**: Pass pointers to individual model states when creating models
4. **NVS Persistence**: The framework automatically saves/restores the context structure to/from NVS

Example from [`meshx_relay_element.hpp`](main/component/meshx/elements/inc/variants/meshx_relay_element.hpp:33):

```cpp
struct meshx_relay_srv_el_ctx_t
{
    uint8_t                         app_id;             /**< Application key ID for publication */
    uint16_t                        pub_addr;           /**< Publication address */
    meshx_gen_onoff_model_state_t   gen_on_off_state;   /**< Current OnOff state */
};
```

---

## Complete Example

Let's walk through a complete example of creating a custom "Power Level" model and element.

### Step 1: Define Model Header

```cpp
// meshx_model_power_level.hpp
#ifndef _MESHX_MODEL_POWER_LEVEL_HPP_
#define _MESHX_MODEL_POWER_LEVEL_HPP_

#include <meshx_model_class.hpp>
#include <meshx_base_model_generic.hpp>

#define MESHX_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
#define MESHX_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PARAMS

struct meshx_power_level_model_state
{
    uint16_t power;      /**< Power level in 0.1 dBm units */
    uint16_t last_power; /**< Last power level */
};

using meshx_power_level_model_state_t = struct meshx_power_level_model_state;

struct meshx_power_level_send_params
{
    meshx_model_t                   *model;
    meshx_ctx_t                     *ctx;
    meshx_power_level_model_state_t  state;
};

using meshx_power_level_send_params_t = struct meshx_power_level_send_params;

struct meshx_power_level_srv_el_msg
{
    meshx_srv_model_send_param_header_t header;
    meshx_power_level_model_state_t      state;
};

using meshx_power_level_srv_el_msg_t = struct meshx_power_level_srv_el_msg;

MESHX_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
class meshXPowerLevelServerModel : public meshXServerModel<meshXBaseGenericServerModel, meshx_power_level_send_params_t>
{
private:
    meshx_power_level_model_state_t model_state;
    meshx_power_level_srv_el_msg_t element_msg;
    bool element_msg_prepared;

    meshx_err_t plat_model_create(void) override;
    meshx_err_t plat_model_delete(void) override;

public:
    meshx_err_t model_send(meshx_power_level_send_params_t *params) override;
    meshx_err_t model_from_ble_cb(dev_struct_t *, control_task_msg_evt_t, meshx_ptr_t) override;
    meshx_err_t prepare_element_msg(meshx_ptr_t *msg_ptr, size_t *msg_size) override;
    meshx_err_t element_state_change_handle(void) override;

    explicit meshXPowerLevelServerModel(
        meshXElementIF *parent_element = nullptr,
        meshx_ptr_t     parent_element_state = nullptr
    );

    ~meshXPowerLevelServerModel() override = default;
};

#endif /* _MESHX_MODEL_POWER_LEVEL_HPP_ */
```

### Step 2: Implement Model

```cpp
// meshx_model_power_level.cpp
#include <generic_model/meshx_model_power_level.hpp>

MESHX_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
meshXPowerLevelServerModel MESHX_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PARAMS
::meshXPowerLevelServerModel(meshXElementIF *parent_element, meshx_ptr_t parent_element_state)
    : meshXServerModel(nullptr, MESHX_MODEL_ID_GEN_POWER_LEVEL_SRV, parent_element, parent_element_state)
{
    model_state.power = 0;
    model_state.last_power = 0;
    element_msg_prepared = false;
}

MESHX_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXPowerLevelServerModel MESHX_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PARAMS
::plat_model_create(void)
{
    meshx_ptr_t p_pub = this->get_pub_struct();
    meshx_ptr_t p_gen = this->get_gen_struct();
    meshx_err_t err = MESHX_SUCCESS;

    err = meshx_plat_power_level_srv_create(this->get_plat_model(), &p_pub, &p_gen);
    if(err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to create Power Level Server Model");
    }
    else
    {
        this->set_pub_struct(p_pub);
        this->set_gen_struct(p_gen);
    }
    return err;
}

MESHX_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXPowerLevelServerModel MESHX_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PARAMS
::plat_model_delete(void)
{
    meshx_ptr_t p_pub = this->get_pub_struct();
    meshx_ptr_t p_gen = this->get_gen_struct();

    meshx_err_t err = meshx_plat_power_level_srv_delete(&p_pub, &p_gen);
    if (err)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Failed to delete Power Level Server Model");
    }
    else
    {
        this->set_pub_struct(nullptr);
        this->set_gen_struct(nullptr);
    }

    return err;
}

MESHX_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXPowerLevelServerModel MESHX_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PARAMS
::model_from_ble_cb(
    dev_struct_t *p_dev,
    control_task_msg_evt_t model_id,
    meshx_ptr_t params)
{
    if(!params || !p_dev)
    {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameters");
        return MESHX_INVALID_ARG;
    }

    if(model_id != this->get_model_id())
    {
        return MESHX_SUCCESS;
    }

    auto *param = static_cast<meshx_gen_srv_cb_param_t *>(params);

    MESHX_LOGD(MODULE_ID_MODEL_SERVER, "op|src|dst:%04" PRIx32 "|%04x|%04x",
               param->ctx.opcode, param->ctx.src_addr, param->ctx.dst_addr);

    model_state.power = param->state_change.power_level_set.power;
    element_msg_prepared = false;

    bool send_reply = (param->ctx.opcode != MESHX_MODEL_OP_GEN_POWER_LEVEL_SET_UNACK);

    switch (param->ctx.opcode)
    {
        case MESHX_MODEL_OP_GEN_POWER_LEVEL_GET:
            return MESHX_NOT_SUPPORTED;

        case MESHX_MODEL_OP_GEN_POWER_LEVEL_SET:
        case MESHX_MODEL_OP_GEN_POWER_LEVEL_SET_UNACK:
        {
            if (MESHX_ADDR_IS_UNICAST(param->ctx.dst_addr)
            || (MESHX_ADDR_BROADCAST(param->ctx.dst_addr))
            || (MESHX_ADDR_IS_GROUP(param->ctx.dst_addr)
            && (MESHX_SUCCESS == meshx_is_group_subscribed(&param->model, param->ctx.dst_addr))))
            {
                element_msg = {
                    .header = {
                        .model = param->model,
                        .element_state_change = MESHX_SUCCESS,
                    },
                    .state = model_state,
                };
                element_msg_prepared = true;
            }
            break;
        }
        default:
            return MESHX_NOT_SUPPORTED;
    }

    if (send_reply || param->ctx.src_addr != param->model.pub_addr)
    {
        MESHX_LOGD(MODULE_ID_MODEL_SERVER, "PUB: src|pub %x|%x", param->ctx.src_addr, param->model.pub_addr);
        param->ctx.dst_addr = param->model.pub_addr;

        meshx_power_level_send_params_t send_params = {
            .model  = &param->model,
            .ctx    = &param->ctx,
            .state  = model_state,
        };

        return this->model_send(&send_params);
    }

    if (!element_msg_prepared)
    {
        MESHX_LOGD(MODULE_ID_MODEL_SERVER, "No element notification needed for opcode: %04x", param->ctx.opcode);
        return MESHX_NOT_SUPPORTED;
    }

    return MESHX_SUCCESS;
}

MESHX_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXPowerLevelServerModel MESHX_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PARAMS
::element_state_change_handle()
{
    auto *el_state = static_cast<meshx_power_level_model_state_t*>(this->get_parent_element_state());

    if (!el_state) {
        MESHX_LOGE(MODULE_ID_MODEL_SERVER, "Invalid parameter in element_state_change_handle");
        return MESHX_INVALID_ARG;
    }

    if(el_state->power != model_state.power)
    {
        el_state->power = model_state.power;
    }
    else
    {
        return MESHX_INVALID_STATE;
    }
    return MESHX_SUCCESS;
}

MESHX_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXPowerLevelServerModel MESHX_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PARAMS
::prepare_element_msg(meshx_ptr_t *msg_ptr, size_t *msg_size)
{
    if (!msg_ptr || !msg_size)
    {
        return MESHX_INVALID_ARG;
    }

    if (!element_msg_prepared)
    {
        return MESHX_NOT_SUPPORTED;
    }

    *msg_ptr = &element_msg;
    *msg_size = sizeof(element_msg);

    return MESHX_SUCCESS;
}

MESHX_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PROTO
meshx_err_t meshXPowerLevelServerModel MESHX_POWER_LEVEL_SERVER_MODEL_TEMPLATE_PARAMS
::model_send(meshx_power_level_send_params_t *params)
{
    if (!params || !params->model || !params->ctx)
    {
        return MESHX_INVALID_ARG;
    }

    params->ctx->opcode = MESHX_MODEL_OP_GEN_POWER_LEVEL_STATUS;

    meshx_gen_server_send_params_t send_params = {
        .p_model        = params->model,
        .p_ctx          = params->ctx,
        .state_change   = {
            .power_level_set = {
                .power = params->state.power,
            }
        },
        .data_len       = sizeof(meshx_state_change_power_level_set_t)
    };

    return this->get_base_model()->plat_send_msg(&send_params);
}
```

### Step 3: Define Element Header

```cpp
// meshx_power_element.hpp
#ifndef __MESHX_POWER_ELEMENT_HPP__
#define __MESHX_POWER_ELEMENT_HPP__

#include <meshx_element_class.hpp>
#include <generic_model/meshx_model_power_level.hpp>

#define MESHX_POWER_SERVER_ELEMENT_TEMPLATE_PROTO
#define MESHX_POWER_SERVER_ELEMENT_TEMPLATE_PARAMS

#if CONFIG_POWER_SERVER_COUNT > 0

struct meshx_power_srv_el_ctx_t
{
    uint8_t  app_id;
    uint16_t pub_addr;
    meshx_power_level_model_state_t power_level_state;
};

using meshx_power_srv_el_ctx_t = struct meshx_power_srv_el_ctx_t;

MESHX_POWER_SERVER_ELEMENT_TEMPLATE_PROTO
class meshXPowerServerElement : public meshXElementServer MESHX_POWER_SERVER_ELEMENT_TEMPLATE_PARAMS
{
private:
    meshx_power_srv_el_ctx_t element_ctx;

    uint8_t list_sig_models(void) override;
    uint8_t list_ven_models(void) override;

public:
    meshXPowerServerElement(uint16_t element_idx);
    meshXPowerServerElement(void) = delete;
};

#endif /* CONFIG_POWER_SERVER_COUNT */

#endif /* __MESHX_POWER_ELEMENT_HPP__ */
```

### Step 4: Implement Element

```cpp
// meshx_power_element.cpp
#include <variants/meshx_power_element.hpp>

#if CONFIG_POWER_SERVER_COUNT > 0

MESHX_POWER_SERVER_ELEMENT_TEMPLATE_PROTO
meshXPowerServerElement MESHX_POWER_SERVER_ELEMENT_TEMPLATE_PARAMS
::meshXPowerServerElement(uint16_t element_idx)
    : meshXElementServer(element_idx)
{
    this->register_element_ctx(
        &element_ctx,
        sizeof(meshx_power_srv_el_ctx_t)
    );
}

MESHX_POWER_SERVER_ELEMENT_TEMPLATE_PROTO
uint8_t meshXPowerServerElement MESHX_POWER_SERVER_ELEMENT_TEMPLATE_PARAMS
::list_sig_models()
{
    auto power_model = std::make_unique<meshXPowerLevelServerModel>(
        this,
        &this->element_ctx.power_level_state
    );
    this->get_sig_models().push_back(std::move(power_model));

    return (uint8_t)this->get_sig_models().size();
}

MESHX_POWER_SERVER_ELEMENT_TEMPLATE_PROTO
uint8_t meshXPowerServerElement MESHX_POWER_SERVER_ELEMENT_TEMPLATE_PARAMS
::list_ven_models()
{
    return 0;
}

#endif /* CONFIG_POWER_SERVER_COUNT */
```

---

## Best Practices

### 1. Naming Conventions

- **Model Classes**: Prefix with `meshX`, suffix with `Model` (e.g., `meshXCustomModel`)
- **Element Classes**: Prefix with `meshX`, suffix with `Element` (e.g., `meshXCustomElement`)
- **State Structures**: Use descriptive names with `_state_t` suffix (e.g., `meshx_custom_model_state_t`)
- **Message Structures**: Use descriptive names with `_el_msg_t` suffix (e.g., `meshx_custom_srv_el_msg_t`)
- **Context Structures**: Use descriptive names with `_el_ctx_t` suffix (e.g., `meshx_custom_srv_el_ctx_t`)

### 2. Error Handling

- Always validate input parameters
- Use appropriate error codes from [`meshx_err.h`](main/component/meshx/inc/meshx_err.h)
- Log errors using `MESHX_LOGE` macro
- Return error codes consistently

### 3. Memory Management

- Store element messages as member variables to ensure persistence
- Use `std::unique_ptr` for model ownership
- The framework handles model lifecycle management

### 4. State Management

- Keep model state separate from element state
- Update element state only in `element_state_change_handle()`
- Use the element context for NVS persistence

### 5. Message Handling

- Always check if the message is for this model (compare model IDs)
- Handle GET, SET, and SET_UNACK opcodes appropriately
- Prepare element messages only when necessary
- Use the `element_msg_prepared` flag to track message preparation

### 6. Documentation

- Add comprehensive Doxygen comments to all public functions
- Document all structure members
- Include usage examples in header files
- Maintain consistent formatting

---

## Common Patterns

### Pattern 1: Simple State Model

For models with a single state value (like OnOff):

```cpp
struct meshx_simple_model_state
{
    uint8_t value;  /**< Single state value */
};
```

### Pattern 2: Multi-State Model

For models with multiple related state values (like CTL):

```cpp
struct meshx_multi_state_model_state
{
    uint16_t value1;  /**< First state value */
    uint16_t value2;  /**< Second state value */
    int16_t  value3;  /**< Third state value */
};
```

### Pattern 3: Element with Multiple Models

For elements that combine multiple models (like CWWW):

```cpp
struct meshx_multi_model_el_ctx_t
{
    uint8_t  app_id;
    uint16_t pub_addr;
    meshx_model1_state_t model1_state;
    meshx_model2_state_t model2_state;
};

uint8_t list_sig_models() override
{
    auto model1 = std::make_unique<meshXModel1ServerModel>(this, &element_ctx.model1_state);
    this->get_sig_models().push_back(std::move(model1));

    auto model2 = std::make_unique<meshXModel2ServerModel>(this, &element_ctx.model2_state);
    this->get_sig_models().push_back(std::move(model2));

    return (uint8_t)this->get_sig_models().size();
}
```

### Pattern 4: Conditional Element Notification

Only notify the element when state actually changes:

```cpp
if(el_state->value != model_state.value)
{
    el_state->value = model_state.value;
    return MESHX_SUCCESS;
}
else
{
    return MESHX_INVALID_STATE;
}
```

### Pattern 5: Opcode Switch Statement

Handle different opcodes in `model_from_ble_cb()`:

```cpp
switch (param->ctx.opcode)
{
    case YOUR_OPCODE_GET:
        // Handle GET - no state change
        return MESHX_NOT_SUPPORTED;

    case YOUR_OPCODE_SET:
    case YOUR_OPCODE_SET_UNACK:
        // Handle SET - update state
        model_state.value = param->state_change.value;
        // Prepare element message
        element_msg = { /* ... */ };
        element_msg_prepared = true;
        break;

    default:
        return MESHX_NOT_SUPPORTED;
}
```

---

## Additional Resources

### Reference Implementations

- **Generic OnOff Model**: [`meshx_model_onoff.hpp`](main/component/meshx/model/inc/generic_model/meshx_model_onoff.hpp) and [`meshx_model_onoff.cpp`](main/component/meshx/model/src/generic_model/meshx_model_onoff.cpp)
- **Light CTL Model**: [`meshx_model_ctl.hpp`](main/component/meshx/model/inc/light_model/meshx_model_ctl.hpp) and [`meshx_model_ctl.cpp`](main/component/meshx/model/src/light_model/meshx_model_ctl.cpp)
- **Relay Element**: [`meshx_relay_element.hpp`](main/component/meshx/elements/inc/variants/meshx_relay_element.hpp) and [`meshx_relay_element.cpp`](main/component/meshx/elements/src/variants/meshx_relay_element.cpp)
- **CWWW Element**: [`meshx_cwww_element.hpp`](main/component/meshx/elements/inc/variants/meshx_cwww_element.hpp) and [`meshx_cwww_element.cpp`](main/component/meshx/elements/src/variants/meshx_cwww_element.cpp)

### Base Classes

- **Model Interface**: [`meshXModelIF`](main/component/meshx/model/inc/meshx_model_class.hpp:29)
- **Model Base**: [`meshXModel`](main/component/meshx/model/inc/meshx_model_class.hpp:154)
- **Server Model**: [`meshXServerModel`](main/component/meshx/model/inc/meshx_model_class.hpp:304)
- **Client Model**: [`meshXClientModel`](main/component/meshx/model/inc/meshx_model_class.hpp:365)
- **Element Interface**: [`meshXElementIF`](main/component/meshx/elements/inc/meshx_element_class.hpp)
- **Element Base**: [`meshXElement`](main/component/meshx/elements/inc/meshx_element_class.hpp:32)
- **Server Element**: [`meshXElementServer`](main/component/meshx/elements/inc/meshx_element_class.hpp:293)
- **Client Element**: [`meshXElementClient`](main/component/meshx/elements/inc/meshx_element_class.hpp:306)

### Platform Base Models

- **Generic Server**: [`meshXBaseGenericServerModel`](main/component/meshx/base_model/inc/meshx_base_model_generic.hpp)
- **Generic Client**: [`meshXBaseGenericClientModel`](main/component/meshx/base_model/inc/meshx_base_model_generic.hpp)
- **Light Server**: [`meshXBaseLightServerModel`](main/component/meshx/base_model/inc/meshx_base_model_light.hpp)
- **Light Client**: [`meshXBaseLightClientModel`](main/component/meshx/base_model/inc/meshx_base_model_light.hpp)

---

## Conclusion

This guide provides a comprehensive overview of how to create custom models and elements in the MeshX framework. By following the patterns and best practices outlined here, you can extend the framework to support new BLE mesh models and create custom elements that meet your specific requirements.

Remember to:
1. Study the existing reference implementations
2. Follow the established naming conventions
3. Implement all required virtual functions
4. Handle errors appropriately
5. Document your code thoroughly
6. Test thoroughly with the BLE mesh network

For questions or issues, refer to the existing implementations in the codebase or consult the MeshX development team.
