# Technical Reference Manual (TRD): UVP-Driven Provisioning & Security Management

**Subsystem:** `meshx_prov_srv`, `meshx_uvp_dispatcher`, `meshx_mxcp`  
**Status:** Approved Architecture (Zero-Bloat UVP Architecture)  
**Parent Requirements:** [requirements.md](../requirements.md)  

---

## Technical Reference Documents (TRD Modules)

This technical reference manual splits the full architecture, protocol framing, message sequence diagrams, memory analysis, and resilience mechanisms into modular documents:

| Module Document | Title & Focus | Primary Scope |
|---|---|---|
| **[page1_overview.md](./page1_overview.md)** | **Architecture & Philosophy** | Zero-Bloat architectural philosophy, separation of concerns, high-level control and data planes. |
| **[page2_protocol_definitions.md](./page2_protocol_definitions.md)** | **Wire Protocols & Data Structures** | MXCP host commands/events (`0x0B..0x0F`, `0x30..0x34`, `0x81..0x8F`, `0xB0..0xB4`), UVP vendor opcodes (`0xC0..0xD0`), and packed C structs. |
| **[page3_remote_provisioning.md](./page3_remote_provisioning.md)** | **Remote Provisioning & Wavefront Tunneling** | Dual-model architecture: Model A ad-hoc transparent tunneling (`0xC0..0xC3`) and Model B autonomous fast wavefront bulk engine. |
| **[page4_key_rotation.md](./page4_key_rotation.md)** | **5-Minute Zero-ACK Key Rotation** | Company secret virtual addressing (`prod_profile.yml`), 3-burst push, 4-minute dual-key overlap, NVS epoch pre-commit, reverse-engineering defense-in-depth, and orphan unicast pull. |
| **[page5_redundancy_cascading.md](./page5_redundancy_cascading.md)** | **Cascading Redundancy & Redundancy Claim** | Real-time state replication engine (incremental address bitmap deltas, DevKey vault mirroring, 5s heartbeats `0xC7`), 15s watchdog failover, $N+1$ standby claim (`0xCD`), and split-brain reconciliation. |
| **[page6_eviction_healing.md](./page6_eviction_healing.md)** | **Long-Term Eviction & DevKey Self-Healing** | Inactivity timeout (>10 min), cryptographic HMAC-SHA256 beacon (`0xCE`), DevKey re-admission (`0xCF`), and state machine diagram. |
| **[page7_resilience_memory.md](./page7_resilience_memory.md)** | **Failure Modes & Memory Analysis** | Comprehensive mitigation matrix (OOB deadlock, split-brain, framing glitches) and Flash/RAM footprint budget comparison. |
| **[page8_fast_bulk_provisioning.md](./page8_fast_bulk_provisioning.md)** | **Wavefront Provisioning Engine (Configurable K)** | Configurable branching ($K=0 \implies \infty$ natural spatial clustering vs. $K \ge 2$ bounded), 100% local proxy crypto, JIT dynamic element allocation (`0xC8`/`0xCC`), Paged Sparse 2-Bit Bitmap, and cross-platform NVS debounce. |
| **[page9_node_replacement.md](./page9_node_replacement.md)** | **Node Replacement & Unicast Reallocation** | In-place address reuse for identical composition, contiguity collision resolution & relocation for expansion, RPL sequence number protection, and DevKey vault swap. |
