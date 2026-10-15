# Requirements: Autonomous & Intent-Driven Element Pairing

This document defines the functional and non-functional requirements for autonomous and intent-driven node and element pairing in the MeshX BLE Mesh ecosystem.  
Reference Architecture: [Master Design Index (TRM)](./design.md) | [Interactive Protocol Architecture](./protocol-architecture.html)  

---

## 1. Context & Motivation
In standard Bluetooth Mesh, pairing a client switch or sensor to an actuator requires deep protocol knowledge: provisioning unicast addresses, element offsets, model IDs, publication credentials, and subscription group addresses. 

This feature enables **Intent-Driven Pairing**:
1. Nodes autonomously expose and report **what they can do** (their active elements, logical models, roles, and functional IDs).
2. Users (via app, gateway, or physical button gestures) specify **what to connect** at a semantic level (e.g., *"Connect Switch 1 to Living Room Ceiling Light"*).
3. The MeshX runtime and orchestration engine automatically resolve the network plumbing (AppKey binding, 1:1 unicast or 1:N group allocation, publication destination, subscription lists, and NVS persistence).
4. Full unpairing support and provisioner synchronization guarantee that offline/field configurations stay reconciled with off-board management tools.

---

## 2. Requirements Matrix

| ID | Title | Description | Acceptance Criteria | Priority |
| :--- | :--- | :--- | :--- | :--- |
| **REQ-001** | **Dynamic Capability Manifest & Self-Reflection** | Each node must expose an internal runtime reflection API that summarizes its active elements, logical model variants, roles (`PRODUCER`, `CONSUMER`, `BIDIRECTIONAL`), and supported `func_id` list. | `meshXElementRegistry` provides a programmatic introspection method returning a `meshx_capability_manifest_t` matching active baked elements. | P0 |
| **REQ-002** | **In-Mesh UVP Capability Discovery Protocol & Secure Addressing** | Introduce discovery commands within the Unified Vendor Protocol (UVP) enabling a provisioner, commissioner, or peer to query the capability manifest of any node in the mesh. All management broadcasts are routed via a 128-bit Management Virtual Address (`0x8000`–`0xBFFF`) subscribed exclusively by Element 0; `0xFFFF` is hard-blocked for security isolation. | Nodes respond to discovery queries within the $\le 375$-byte budget; UVP dispatcher drops any management frame addressed to `0xFFFF`; Element 0 handles all management frames exclusively while secondary elements ignore them. | P0 |
| **REQ-003** | **Pre-Provisioning Capability Advertising** | Nodes must encode a compact capability summary (role and primary traits bitmask) into unprovisioned BLE beacon service data. | Unprovisioned devices advertise capability flags allowing scanners to identify device personality before provisioning begins. | P1 |
| **REQ-004** | **Semantic Compatibility Engine** | The system must validate pairing requests against a strict compatibility matrix (e.g. matching `RELAY_CLIENT` with `RELAY_SERVER` or `LIGHT_SERVER` on `func_id 0x00`), rejecting invalid bindings before modifying network state. | Attempting to pair incompatible elements (e.g., Sensor Client with RGB Color Server) returns a clear rejection status (`MESHX_ERR_INCOMPATIBLE_TRAIT`). | P0 |
| **REQ-005** | **Automated Topology Resolution & Clean 2-Tier Group Management** | The orchestration engine must translate user pairing intents into concrete mesh configuration: 1:1 direct unicast publication (0 group leases), centralized sequential pool allocation (`0xC001`+) when commissioner is active, or autonomous collision-free group derivation using the Anchor Formula (`0xC000 + (Anchor_Unicast & 0x03FF)`). When an existing 1:1 switch is paired to an additional lamp, the engine must dynamically promote the link to a group address via `MESHX_SYS_CMD_TOPOLOGY_UPGRADE`. | 1:1 pairing configures client `pub_addr` directly; 1:N or N:1 multi-node groups derive group addresses from the Anchor Node with 0% collision and 0 broadcast probe overhead; dynamic promotion upgrades 1:1 links smoothly; provisioner lazily syncs via `link_epoch`. | P0 |
| **REQ-006** | **NVS Link Table Persistence** | Active pairing links (publication destinations, assigned group addresses, bound element indices, and `link_epoch`) must be persisted in NVS alongside the element composition schema. | Node reboots and power cycles retain all active publication and subscription bindings without requiring external re-commissioning. | P0 |
| **REQ-007** | **Autonomous Pairing (1-Hop Proximity & Multi-Hop Relayed)** | Nodes must support autonomous gateway-less pairing in two operational modes: (1) 1-hop physical proximity filtered by an RSSI threshold ($\ge -50\text{ dBm}$), and (2) multi-hop mesh-relayed pairing across intermediate hopping nodes using standard Mesh TTL forwarding and end-to-end `AppKey` encryption when nodes are distant. | Distant provisioned nodes on the same network/AppKey successfully negotiate and establish element links across $\ge 2$ intermediate mesh relay hops. | P1 |
| **REQ-008** | **Granular Unpair, Pre-Reset Teardown & Dangling Peer Prevention** | The system must provide clean teardown APIs to unbind specific element links. Crucially, before executing a factory reset (`meshx_handle_node_reset()`), the node must broadcast pre-reset unpair notifications to all active peers in its link table and drain network buffers (500 ms delay) so peers clear publication addresses and subscriptions before the node reboots. | Peers clear `pub_addr` and group subscriptions before the resetting node reboots; offline peers self-heal and prune dangling links after 3 consecutive transaction timeouts. | P0 |
| **REQ-009** | **Host Serial (MXCP) Capability & Pairing Control** | Extend MXCP over UART so companion MCUs and edge gateways can trigger discovery scans, retrieve cached manifests, and issue pairing/unpairing commands. | Companion hosts can execute `MXCP_CMD_DISCOVER_CAPABILITIES`, `MXCP_CMD_BIND_ELEMENTS`, and `MXCP_CMD_UNBIND_ELEMENTS` with immediate structured feedback. | P1 |
| **REQ-010** | **Provisioner State Synchronization & Offline Tolerance** | Nodes must maintain a monotonic 16-bit `link_epoch` in NVS incremented on every pair/unpair; re-connecting provisioners query this epoch to reconcile local database caches lazily without disrupting runtime mesh operation. | Smartphone app or gateway re-synchronizes its UI/mesh database by querying `link_epoch` and fetching updated manifests if `node.link_epoch > cached_epoch`. | P1 |
| **REQ-011** | **Dynamic Personality Conflict Resolution** | When element layout changes via MXCP (`MXCP_CMD_SET_COMPOSITION`), the node must proactively notify peers, dismantle affected links, self-audit its NVS link table on boot (`meshx_link_reconcile()`), return `MESHX_SYS_STATUS_ERR_PERSONALITY_DIFF` on legacy frames, and force re-provisioning if element count exceeds provisioned capacity. | Mutated or removed elements are cleanly disassociated without dangling retries; peers auto-prune links upon receiving error frames; element expansion beyond mesh capacity safely resets for re-provisioning. | P0 |

---

## 3. Out of Scope
The following aspects are out of scope for the embedded device firmware specification and deferred to future planning:
- **Zero-Gateway Out-of-the-Box Provisioning (Deferred):** Onboarding brand-new, uncommissioned devices in consumer retail kits without any commissioner app or gateway (e.g., embedded root provisioners onboarding switches via PB-ADV or factory pre-provisioned multipacks). All devices participating in autonomous element pairing are assumed to be already provisioned with valid NetKey, AppKey, and unique unicast addresses.
- **Centralized Network Database:** Aggregating global multi-node network databases (e.g., SQLite/JSON cloud sync).
- **GUI Visualization Engine:** Web-based or mobile interactive wiring diagrams, node-pin canvases, and drag-and-drop link editors.
