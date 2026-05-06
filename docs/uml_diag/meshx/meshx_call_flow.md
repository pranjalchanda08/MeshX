# MeshX Call Flow Diagrams

## System Initialization Flow

This diagram shows the sequence of calls during the MeshX stack initialization.

```mermaid
sequenceDiagram
    participant App as app_main
    participant MeshX as meshx_init()
    participant Tasks as meshx_tasks_init()
    participant Ctrl as meshx_control_task
    participant Builder as MeshXCompositionBuilder
    participant Comp as MeshXComposition
    participant Element as MeshXElement
    participant BT as Platform BT Stack

    App->>MeshX: meshx_init(config)
    
    rect rgb(240, 240, 240)
    Note over MeshX, Builder: Dynamic Composition Setup
    MeshX->>Builder: begin()
    MeshX->>Builder: add_element(TYPE, COUNT)
    Builder->>Element: create variants
    MeshX->>Builder: commit()
    end

    MeshX->>Tasks: meshx_tasks_init()
    Tasks->>Ctrl: create_control_task()
    
    MeshX->>MeshX: meshx_ble_mesh_init()
    MeshX->>Comp: bake(cid, pid, vid)
    Comp->>Element: list_sig_models()
    Element->>Element: add_sig_model()
    Comp->>Comp: Flatten to MESHX_ELEMENT[]
    
    MeshX->>BT: meshx_plat_ble_mesh_init()
    BT-->>MeshX: Success
    
    MeshX->>Comp: Notify baked()
    Comp->>Element: on_baked(index)
    Element->>Element: update models
```

## Runtime Event Flow (Downstream)

This diagram shows how a message from the BLE Mesh network is routed to the application and synchronized with the element's persistent state.

```mermaid
sequenceDiagram
    participant BT as BT Stack (C)
    participant Bridge as Platform Bridge
    participant Ctrl as Control Task
    participant Model as MeshXModel (C++)
    participant Element as MeshXElement (C++)
    participant NVS as MeshX NVS
    participant User as App Callback

    BT->>Bridge: esp_ble_mesh_model_cb(evt, param)
    Bridge->>Ctrl: control_task_msg_publish(MSG_CODE, EVT, param)
    
    Note over Ctrl: Async Dispatch
    
    Ctrl->>Bridge: control_task_msg_dispatch()
    Bridge->>Model: model_handle_from_ble_cb()
    Model->>Model: model_from_ble_cb() [Virtual]
    
    rect rgb(230, 255, 230)
    Note over Model, NVS: State Synchronization & Persistence
    Model->>Model: send_to_parent_element()
    Model->>Element: on_model_cb(param)
    Element->>Model: element_state_change_handle() [Virtual]
    Model->>Element: Sync model_state -> element_ctx
    Element->>NVS: meshx_nvs_set(element_ctx)
    end
    
    Element->>User: app_element_cb()
```

## Runtime Command Flow (Upstream - Client)

This diagram illustrates how a Client Model sends a command, utilizing the **Transmission Control Manager (TXCM)** for reliability and queue management.

```mermaid
sequenceDiagram
    participant User as App Logic
    participant Model as meshXClientModel (Derived)
    participant Base as meshXBaseClientModel (Template)
    participant TXCM as TX Control Manager (C)
    participant PAL as Platform Abstraction
    participant BT as BT Stack (C)

    User->>Model: model_send(app_params)
    
    Note over Model: Validate Opcode & Context
    Model->>Model: Prepare plat_send_params
    
    Model->>Base: plat_send_msg(plat_params)
    
    rect rgb(240, 240, 255)
    Note over Base, TXCM: Reliability & Queue Management
    Base->>Base: Determine req_type (ENQ vs DIRECT)
    Base->>TXCM: meshx_txcm_request_send(req_type, cb, data)
    TXCM->>TXCM: (See TXCM Detailed Flow)
    end

    Base->>PAL: meshx_plat_gen_cli_send_msg()
    PAL->>BT: esp_ble_mesh_client_model_send_msg()
    
    BT-->>PAL: Stack Result
    PAL-->>Base: meshx_err_t
    Base-->>Model: meshx_err_t
    Model-->>User: meshx_err_t
```

## TXCM Detailed Algorithm Flow

This diagram details the internal logic of the Transmission Control Manager, including queuing, retries, and ACK handling.

```mermaid
sequenceDiagram
    participant Base as meshXBaseModel
    participant API as TXCM API
    participant Task as TXCM Task
    participant TXQ as TX Queue (Ring Buffer)
    participant BT as Platform Stack

    Base->>API: meshx_txcm_request_send(SIG_ENQ, addr, data, cb)
    API->>API: Malloc Context
    API->>Task: Push to Sig Queue
    
    Note over Task: Task wakes up
    Task->>Task: Dispatch SIG_ENQ handler
    Task->>TXQ: Search for duplicate
    alt No Duplicate
        Task->>TXQ: Enqueue(data, cb, state=NEW, retry=3)
    end
    
    rect rgb(240, 255, 240)
    Note over Task, BT: Try Send Logic (Optimized)
    Task->>TXQ: Scan Queue for first runnable NEW msg
    Note right of Task: Skip if destination is already WAITING_ACK
    Task->>TXQ: Dequeue_at(index)
    Task->>Task: retry_cnt--
    Task->>BT: Execute cb -> meshx_plat_..._send()
    
    alt ACKED Type
        Task->>TXQ: EnqueueFront(state=WAITING_ACK)
    else UNACKED Type
        Task->>Task: Discard Context
    end
    end

    Note over Task: Handling Feedback (Multi-slot)
    alt ACK Received (from BT callback)
        BT->>API: meshx_txcm_request_send(SIG_ACK, addr)
        API->>Task: Push SIG_ACK
        Task->>TXQ: Search Queue for addr + WAITING_ACK
        Task->>TXQ: Dequeue_at(match_index)
        Task->>Task: Trigger try_send() for next runnable
    else Retry/Timeout (from BT timer)
        BT->>API: meshx_txcm_request_send(SIG_RESEND, addr)
        API->>Task: Push SIG_RESEND
        alt retry_cnt > 0
            Task->>Task: Trigger try_send(resend=true)
        else retry_cnt == 0
            Task->>Task: Publish TIMEOUT to Control Task
            Task->>Task: Trigger next message try_send()
        end
    end
```

## Best Practices & Deadlock Prevention

### 1. Preventing Circular Deadlocks in Control Task
The `meshx_control_task` is a single-threaded sequencer. A common deadlock occurs when the application logic blocks the task while waiting for a response that the task itself must dispatch.

> [!CAUTION]
> **NEVER** use blocking primitives (semaphores, mutexes, or busy-loops) inside a model callback or element callback to wait for a network response.

**Recommended Strategy: Asynchronous State Machine**
Instead of:
```cpp
// DEADLOCK PRONE
model.send(cmd);
xSemaphoreTake(response_sem, portMAX_DELAY); 
```

Use the event-driven pattern:
1.  **Initiate**: Call `model_send()`.
2.  **Yield**: Return control to the `meshx_control_task` loop.
3.  **Handle**: Process the response in the next `on_model_cb` or `on_ble_evt` trigger.

### 2. TXCM Queue Management
With the updated non-Head-of-Line blocking logic, multiple nodes can be communicated with in parallel.
-   **Transaction Isolation**: TXCM now isolates transactions by destination address.
-   **Concurrency**: The `meshx_txcm_request_send` API is now thread-safe and can be called from multiple application tasks.
-   **Binary Payloads**: TXCM uses full memory comparison (`memcmp`); ensure `msg_param_len` is accurately provided to avoid false duplicate detection.
