# TRD Index: Island Subnet Coexistence, Bridging & Routing (Case 4)

**Module:** `TRD-SUBNET-INDEX`
**Feature:** Island Subnet Coexistence, Non-Destructive Bridging, Redundancy, Multi-Island, KR Orchestration, MitM Defense, 2-Stage Store-and-Forward Routing & Subnet Prefix NAT
**Issue Link:** https://github.com/pranjalchanda08/MeshX/issues/162
**Branch:** `fr/162-fr-island-subnet`
**Parent Requirements:** [requirements.md](../requirements.md)
**Parent Design Overview:** [design.md](../design.md)

---

## Technical Reference Document (TRD) Sitemap

| Page | Document | Title & Subsystem Scope |
|---|---|---|
| **01** | [page1_overview_topologies.md](./page1_overview_topologies.md) | **Architecture & Multi-Island Topologies**: Problem statement, Star vs Graph architectures, boundary detection, and platform abstraction layer (PAL). |
| **02** | [page2_addressing_nat.md](./page2_addressing_nat.md) | **Global Addressing & Subnet Prefix NAT**: 16-bit Global Subnet IDs, 15-bit Unicast Partitioning (`0x4000..0x7FFF`), and CIDR-style virtual proxy mapping. |
| **03** | [page3_group_addressing.md](./page3_group_addressing.md) | **Autonomous Group Addressing & Offset Prefixing**: Interoperability with Anchor Formula (`0xC000 + (Anchor & 0x03FF)`), group domain isolation, and dynamic translation (`0xC001_A` $\leftrightarrow$ `0xC401_B`). |
| **04** | [page4_store_forward_router.md](./page4_store_forward_router.md) | **2-Stage Store-and-Forward Router Architecture**: Ingress termination, local Lower Transport ACK, inter-stage queueing, and independent egress SAR session dispatch. |
| **05** | [page5_discovery_pairing.md](./page5_discovery_pairing.md) | **Cross-Island Discovery & Intent-Driven Pairing**: UVP `0x8000` broadcast relay, Capability Manifest reflection, Semantic Compatibility Engine, `link_epoch` progression, and pre-reset unpair teardown. |
| **06** | [page6_key_rotation.md](./page6_key_rotation.md) | **Multi-Subnet Key Rotation (KR) Orchestration**: $O(1)$ crypto slots, independent phase transitions (Phase 0..3), burst containment (`0xC4..0xC6`), and egress key selection. |
| **07** | [page7_redundancy_failover.md](./page7_redundancy_failover.md) | **Active-Standby Redundancy ($N+1$) & Loop Prevention**: Progressive eligibility pipeline (`BRIDGE_ROLE_CANDIDATE`), $4.0\text{s}$ heartbeat (`0xD1`), $12.0\text{s}$ watchdog failover (`0xD2`), and Subnet Path Vector loop prevention. |
| **08** | [page8_mitm_security.md](./page8_mitm_security.md) | **Man-in-the-Middle (MitM) Defense & Threat Modeling**: End-to-end AppKey opacity, Bridge Cluster Key ($K_{\text{bridge}}$), partitioned RPL, and ingress topology validation. |
| **09** | [page9_latency_memory.md](./page9_latency_memory.md) | **Performance, Latency Bounds & Memory Budgets**: Best/worst-case timing ($21.5\text{ ms} \dots 181.5\text{ ms}$), failover recovery ($12.0\text{s} \dots 12.5\text{s} \le 15.5\text{s}$), RAM ($2,570\text{ B}$ static BSS, $3,536\text{ B}$ peak $\le 3.5\text{ KB}$), and NVS flash ($796\text{ B} < 1.0\text{ KB}$). |
| **10** | [page10_multihop_transit.md](./page10_multihop_transit.md) | **Multi-Hop Transit NAT & Inter-Bridge Synchronization**: Linear / Mesh Graph Topology (A $\leftrightarrow$ B $\leftrightarrow$ C), deterministic slot derivation, transit subnet proxy synchronization, and `MESHX_UVP_OP_ROUTE_ADV`. |
| **11** | [page11_slot_assignment.md](./page11_slot_assignment.md) | **Subnet Slot ID Assignment & Registry Architecture**: Who assigns slot IDs (Autonomous Anchor vs Operator App), local scoping vs global identity, and hop-by-hop prefix rewriting. |

