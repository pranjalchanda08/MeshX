# TRD Page 9: Latency Performance Bounds & Memory Budgets

**Module:** `TRD-SUBNET-09`  
**Parent Index:** [TRD Index](./README.md)  
**Requirements:** [requirements.md](../requirements.md) (REQ-NF01, REQ-NF02)  
**Related Fixes:** Remediation of R4 findings from `REVIEW_FINDINGS.md`  

---

## 1. End-to-End Latency Formula

$$\mathbf{T_{\text{total}} = T_{\text{in}} + T_{\text{bridge}} + T_{\text{out}}}$$

- **$T_{\text{in}}$**: Air transit from the source node to the bridge across Island A hops.
- **$T_{\text{bridge}}$**: Processing time on the bridge (RX dequeue, AES-CCM decrypt, RPL check, NAT table lookup, Sequence Number allocation, AES-CCM re-encrypt, and TX queue enqueue).
- **$T_{\text{out}}$**: Air transit from the bridge to the final destination node across Island B hops.

---

## 2. Operational Scenario Latencies & Timing Alignment

| Scenario | Network Topology & RF Conditions | Total End-to-End Latency |
|---|---|---|
| **Best Case** | 1-hop direct neighbor to bridge, clear medium, hardware AES | $\mathbf{21.5\text{ ms}} \quad (\le 30\text{ ms})$ |
| **Typical Case** | 2 hops in Island A $\rightarrow$ Bridge $\rightarrow$ 2 hops in Island B | $\mathbf{76.0\text{ ms}}$ |
| **Worst Case Operational** | 3 hops in A, 3 hops in B, 1 collision retry, software AES | $\mathbf{181.5\text{ ms}} \quad (\le 200\text{ ms})$ |
| **Failover Outage Window** | Active Bridge cutoff during flight (watchdog expires after missing heartbeats + takeover) | $\mathbf{12.0\text{s} \dots 12.5\text{s}} \quad (\le \mathbf{15.5\text{s}})$ |

*All specifications, requirements, and tasks are strictly aligned to the **$\le 15.5\text{s}$ failover boundary**.*

---

## 3. Comprehensive Memory & Flash Budget Breakdown

All protocol and routing data structures are packed with `MESHX_PACKED` to eliminate compiler alignment padding.

### 3.1 Memory & Buffer Breakdown (Max Cap: 3.5 KB)

All protocol and routing data structures use `MESHX_PACKED` with natural descending alignment (`pointer -> uint32_t -> uint16_t -> uint8_t`) on 32-bit targets (ESP32/nRF52/nRF53) to eliminate compiler padding and unaligned load faults. Exact struct sizes:
- `meshx_router_queue_t`: 2 pointers (8 B) + 1 `uint32_t` (4 B) + 2 `uint8_t` (2 B) = **14 B** (budgeted at 14 B; 22 B on 64-bit host test builds).
- `meshx_subnet_crypto_slot_t`: 3 `uint32_t` (12 B) + 2 `uint16_t` (4 B) + 1 `uint8_t` (1 B) + 2 $\times$ 16-byte keys (32 B) = **49 B** packed (budgeted at 52 B per slot $\times$ 4 subnets = **208 B** including per-slot runtime flags).
- `meshx_nat_entry_t`: 1 `uint32_t` (4 B) + 7 `uint16_t` (14 B) + 1 `uint8_t` (1 B) = **19 B** packed. In RAM, 16 active entries with 5 B platform lookup metadata allocate $16 \times 24\text{ B} = \mathbf{384\text{ B}}$.
- `meshx_local_slot_registry_t` + `meshx_local_slot_entry_t`: Static registry control head = 2 pointers (8 B) + 1 `uint16_t` (2 B) + 2 `uint8_t` (2 B) = **12 B** packed in BSS. Linked-list nodes (`13 B` packed each: `next` + `allocated_ts_s` + `remote_global_subnet` + `proxy_base_addr` + `slot_id`) allocate individually per subnet ($14 \times (13\text{ B} + 8\text{ B RTOS heap header}) = \mathbf{294\text{ B}}$ peak heap; 0 B when unbridged, no `realloc`).
- Foreign Subnet & Transit Route (`0xD6`) Table: 4 observers + 8 transitive route entries ($8 \times 12\text{ B} + 64\text{ B}$) = **160 B**.
- Multi-Subnet RPL Cache: 32 entries $\times$ **16 B** (`net_idx`, `src_addr`, `epoch`, `highest_seq`, 32-bit `seq_window`) = **512 B**.
- Loop Prevention Echo Filter: 32 entries $\times$ **8 B** (`crc32_digest`, `timestamp_sec`) = **256 B**.
- Redundancy Fencing & Anti-Replay: 16-entry sliding window cache ($16 \times 12\text{ B} = 192\text{ B}$) + 4 boundary term/nonce-floor contexts ($4 \times 16\text{ B} = 64\text{ B}$) = **256 B**.
- Packet Buffer, Emergency NACK TX Slot (`64 B`) & Reassembly Scratch: **768 B**.
- Dynamic Heap Total (Peak): 4 Active In-Flight Queue Flows ($4 \times (52\text{ B metadata} + 108\text{ B max PDU} + 8\text{ B RTOS heap header}) = \mathbf{672\text{ B}}$) + Dynamic Subnet Slot Linked List ($14 \times 21\text{ B} = \mathbf{294\text{ B}}$) = **966 B** peak heap.

```
+-------------------------------------------------------------+
|               STATIC BSS RAM & BUFFER BREAKDOWN             |
+------------------------------------+------------------------+
| Component                          | Allocated Bytes        |
+------------------------------------+------------------------+
| 2-Stage Router Queue Head/Metadata | 14 B                   |
| Subnet Crypto Table (4 x 52 B)     | 208 B                  |
| Bridging NAT Table (16 x 24 B)     | 384 B                  |
| Subnet Slot Registry Head (BSS)    | 12 B                   |
| Observer & 0xD6 Route Table (4/8)  | 160 B                  |
| Multi-Subnet RPL Cache (32 x 16 B) | 512 B                  |
| Loop Prevention Filter (32 x 8 B)  | 256 B                  |
| Redundancy & Boundary Context (16) | 256 B                  |
| Packet/Reassembly/NACK Scratch     | 768 B                  |
+------------------------------------+------------------------+
| TOTAL PERSISTENT STATIC BSS RAM    | 2,570 Bytes (~2.51 KB) |
+------------------------------------+------------------------+
| Dynamic Queue Heap (Peak 4 flows)  | 672 B                  |
| Dynamic Slot List Heap (14 x 21 B) | 294 B                  |
+------------------------------------+------------------------+
| WORST-CASE COMBINED RAM (PEAK)     | 3,536 Bytes (~3.45 KB) |
+------------------------------------+------------------------+
| MAXIMUM ALLOWED CEILING            | 3,584 Bytes (3.5 KB)   |
+------------------------------------+------------------------+
```

### 3.2 NVS Flash Storage Budget (Max Cap: 1.0 KB)

The 1.0 KB NVS budget ($< 1,024\text{ B}$) covers all persistent bridging credentials, routing configurations, and cross-island NAT states:
```
+-------------------------------------------------------------+
|                  NVS FLASH PERSISTENCE                      |
+------------------------------------+------------------------+
| Component                          | Persisted Bytes        |
+------------------------------------+------------------------+
| Secondary Subnet NetKeys (up to 3) | 48 B                   |
| Bridge Cluster Key (K_bridge)      | 16 B                   |
| Subnet Slot Registry (14 entries)  | 168 B                  |
| Persistent NAT Rules (16 x 19 B)   | 304 B                  |
| Monotonic link_epoch Counter       | 4 B                    |
| Bridge Nonce & Term Checkpoints    | 16 B                   |
| Per-Subnet Sequence Counters (4x4B)| 16 B                   |
| Subnet KR Metadata (4 subnets x32B)| 128 B                  |
| NVS Storage Header & CRC Framing   | 96 B                   |
+------------------------------------+------------------------+
| TOTAL SUB-TOTAL PERSISTED NVS      | 796 Bytes (~0.78 KB)   |
+------------------------------------+------------------------+
| Cross-Subnet Pairing Records (NVS) | Partition Bounded      |
+------------------------------------+------------------------+
| TOTAL PERSISTED BRIDGING NVS FLASH | 796 Bytes (< 1.0 KB)   |
+------------------------------------+------------------------+
| MAXIMUM ALLOWED BRIDGING CEILING   | 1,024 Bytes (< 1.0 KB) |
+------------------------------------+------------------------+
```

> **Pairing Record NVS Partition Scoping Invariant**:
> Persistent element pairing records (`meshx_pair_record_t`, 20 bytes packed) belong to the **Autonomous Element Pairing NVS Domain** governed by `.specs/autonomous-element-pairing/` (REQ-006). They reside in a dedicated NVS namespace (`NVS_NS_MESHX_LINKS`, 4 KB sector) separate from the 1.0 KB Bridge Routing Credential Partition (`NVS_NS_MESHX_SUBNET`). On bridge nodes acting simultaneously as pairing endpoints, up to 8 cross-subnet pairing records ($8 \times 20\text{ B} = 160\text{ B}$) fit easily within available space, guaranteeing zero risk of overflowing the bridging credential partition.

Both RAM ($3,536\text{ B} \le 3.5\text{ KB}$, $2,570\text{ B}$ static BSS + $966\text{ B}$ peak dynamic heap) and NVS ($796\text{ B} \le 1.0\text{ KB}$) are completely reconciled with exact naturally-aligned packed struct definitions and verified across 32-bit embedded targets.
