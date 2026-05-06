# MeshX Comprehensive Class Documentation

To ensure readability, the architecture is broken down into logical layers with detailed class members.

## 1. Core Architecture
This diagram shows the relationship between the Device, Composition, and the Base Interfaces.

```mermaid
classDiagram
    direction TD
    
    class MeshXDevice {
        <<Singleton>>
        -dev_struct_t* pdev
        +get_instance()$ MeshXDevice
        +init(pdev: dev_struct_t*) meshx_err_t
        +visualize_status() void
    }

    class MeshXComposition {
        <<Singleton>>
        -vector~unique_ptr~meshXElementIF~~ elements
        +get_instance()$ MeshXComposition
        +bake(cid: uint16_t, pid: uint16_t, vid: uint16_t) meshx_err_t
    }

    class meshXElementIF {
        <<Interface>>
        +initialize()* meshx_err_t
        +reset()* meshx_err_t
        +on_model_cb(param: meshx_ptr_t, size: size_t)* meshx_err_t
        +on_baked(index: uint16_t)* void
        +get_element_idx() uint16_t
    }

    class meshXModelIF {
        <<Interface>>
        -MESHX_MODEL* p_plat_model
        -meshXElementIF* parent_element
        +plat_model_create(p_plat_model_ptr: MESHX_MODEL*)* meshx_err_t
        +plat_model_delete()* meshx_err_t
        +element_state_change_handle()* meshx_err_t
        +prepare_element_msg(msg_ptr: meshx_ptr_t*, msg_size: size_t*)* meshx_err_t
        +on_baked()* void
        +get_model_id()* uint16_t
    }

    MeshXDevice --> MeshXComposition
    MeshXComposition "1" *-- "many" meshXElementIF
    meshXElementIF "1" *-- "many" meshXModelIF
```

## 2. Element Hierarchy
Detailed implementation of the Element base class and its specialized Server/Client variants.

```mermaid
classDiagram
    direction TD
    class meshXElementIF { <<Interface>> }
    
    class meshXElement {
        <<Template>>
        -uint8_t no_of_sig_models
        -uint8_t no_of_ven_models
        -vector~unique_ptr~meshXModelIF~~ sig_models
        -vector~unique_ptr~meshXModelIF~~ ven_models
        -meshx_ptr_t element_ctx
        +list_sig_models() virtual uint8_t
        +list_ven_models() virtual uint8_t
        +add_sig_model() meshx_err_t
        +add_ven_model() meshx_err_t
        +register_element_ctx(ctx: meshx_ptr_t, size: size_t) void
        +element_state_change_notify(param: meshx_ptr_t, size: size_t) virtual meshx_err_t
    }

    class meshXElementServer {
        +meshXElementServer(element_idx: uint16_t)
    }

    class meshXElementClient {
        +meshXElementClient(element_idx: uint16_t)
    }

    meshXElementIF <|-- meshXElement
    meshXElement <|-- meshXElementServer
    meshXElement <|-- meshXElementClient

    meshXElementServer <|-- meshXRelayServerElement
    meshXElementServer <|-- meshXRootElement
    meshXElementServer <|-- meshXSensorElement
    meshXElementServer <|-- meshXCWWWElement
    meshXElementServer <|-- meshXRGBElement
    meshXElementClient <|-- meshXRelayClientElement
```

## 3. Model Hierarchy
Detailed implementation of the Model base class and its specialized Server/Client templates.

```mermaid
classDiagram
    direction TD
    class meshXModelIF { <<Interface>> }

    class meshXModel {
        <<Template>>
        -meshxBaseModel_t* base_model
        -uint16_t model_id
        -uint16_t model_func_id
        +model_from_ble_cb(dev: dev_struct_t*, evt: control_task_msg_evt_t, data: meshx_ptr_t)* meshx_err_t
        +model_send(params: meshx_send_packet_params_t*)* meshx_err_t
        +send_to_parent_element(msg_ptr: meshx_ptr_t, msg_size: size_t) meshx_err_t
        #update_element_state_change_header(element_state_change: meshx_err_t, msg_ptr: meshx_ptr_t)* void
    }

    class meshXServerModel {
        <<Template>>
        +update_element_state_change_header(element_state_change: meshx_err_t, msg_ptr: meshx_ptr_t) void
    }

    class meshXClientModel {
        <<Template>>
        +plat_model_create(p_plat_model_ptr: MESHX_MODEL*) meshx_err_t
        +plat_model_delete() meshx_err_t
        +update_element_state_change_header(element_state_change: meshx_err_t, msg_ptr: meshx_ptr_t) void
    }

    meshXModelIF <|-- meshXModel
    meshXModel <|-- meshXServerModel
    meshXModel <|-- meshXClientModel
```

## 4. Specialized SIG Models
Implementation examples of the standard Bluetooth SIG models.

```mermaid
classDiagram
    direction TD
    class meshXServerModel { <<Template>> }
    class meshXClientModel { <<Template>> }

    namespace Generic_Models {
        class meshXGenericOnOffServerModel
        class meshXGenericOnOffClientModel
        class meshXGenericLevelServerModel
        class meshXGenericLevelClientModel
    }

    namespace Light_Models {
        class meshXLightLightnessServerModel
        class meshXLightLightnessClientModel
        class meshXLightCTLServerModel
        class meshXLightHSLServerModel
    }

    meshXServerModel <|-- meshXGenericOnOffServerModel
    meshXServerModel <|-- meshXGenericLevelServerModel
    meshXServerModel <|-- meshXLightLightnessServerModel
    meshXServerModel <|-- meshXLightCTLServerModel

    meshXClientModel <|-- meshXGenericOnOffClientModel
    meshXClientModel <|-- meshXGenericLevelClientModel
    meshXClientModel <|-- meshXLightLightnessClientModel
    meshXClientModel <|-- meshXLightHSLServerModel
```
