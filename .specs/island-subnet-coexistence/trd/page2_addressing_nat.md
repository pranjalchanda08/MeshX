# TRD Page 2: Global Addressing & Subnet Prefix NAT

**Module:** `TRD-SUBNET-02`  
**Parent Index:** [TRD Index](./README.md)  
**Requirements:** [requirements.md](../requirements.md) (REQ-F01, REQ-F03)  
**Related Fixes:** Remediation of R1 findings from `REVIEW_ROUND2_FINDINGS.md`  

---

## 1. Global Subnet Identifier (`global_subnet_id`) & Genesis Remap

Each island derives a 16-bit Global Subnet Identifier at genesis:
$$\mathbf{global\_subnet\_id = \text{CRC16-CCITT}(\text{anchor\_uuid} \parallel \text{net\_key\_epoch\_0}[0..3])}$$

### Genesis Bounded Remap Loop:
- Valid Range: `0x0001..0xFFFE` (65,534 valid identifiers).
- The input seed buffer binds the 16-byte `anchor_uuid` and the initial 4 bytes of NetKey:
  $$\mathbf{seed = [\text{anchor\_uuid (16 B)} \parallel \text{net\_key\_epoch\_0}[0..3] \text{ (4 B)}] \quad (20\text{ B})}$$
- If the raw calculation produces reserved value `0x0000` or `0xFFFF`:
  ```c
  uint8_t seed[20];
  memcpy(seed, anchor_uuid, 16);
  memcpy(seed + 16, net_key_epoch_0, 4);

  uint8_t nonce = 1;
  while ((id == 0x0000 || id == 0xFFFF) && nonce < 10) {
      uint8_t remap_buf[sizeof(seed) + 1];
      memcpy(remap_buf, seed, sizeof(seed));
      remap_buf[sizeof(seed)] = nonce++;
      id = crc16_ccitt(remap_buf, sizeof(remap_buf), 0xFFFF);
  }
  if (id == 0x0000 || id == 0xFFFF) {
      /* Fallback uses full 16-byte anchor_uuid CRC to provide pseudo-random distribution in 0x0002..0xFFFE */
      uint16_t fallback_crc = crc16_ccitt(anchor_uuid, 16, 0xA5A5);
      if (fallback_crc == 0x0000 || fallback_crc == 0xFFFF) {
          fallback_crc = 0x0002;
      }
      id = fallback_crc;
  }
  ```
  Guarantees termination in bounded cycles without collapsing colliding islands to a static `0x0001` or small modulo cluster.

---

## 2. Address Space Partitioning & The Virtual Proxy Window

To eliminate unicast address collisions across independently bootstrapped islands, the 15-bit unicast address space (`0x0001..0x7FFF`) is partitioned:

```
0x0001                                  0x3FFF 0x4000                  0x77FF 0x7800                  0x7FFF
+─────────────────────────────────────────────+──────────────────────────────+──────────────────────────────+
|        LOCAL PHYSICAL NODE SPACE            |    TIER-1 STATIC PROXY SLOTS |   TIER-2 DYNAMIC ALIAS POOL  |
|         (16,383 Local Elements)             |  (14 Slots x 1,024 Elements) |    (2,048 Dynamic Aliases)   |
+─────────────────────────────────────────────+──────────────────────────────+──────────────────────────────+
```

1. **Local Physical Range (`0x0001..0x3FFF`)**:
   - Reserved strictly for physical devices provisioned locally within that island.
2. **Tier-1 Static Virtual Subnet Slots (`0x4000..0x77FF`)**:
   - Divided into fourteen 1,024-address virtual subnet slots ($n \in [0..13]$).
   - Base address: $\mathbf{0\text{x4000} + (n \ll 10)}$ (offset `0` corresponds to reserved Unassigned Address `0x0000`, yielding `0x0001..0x03FF` = **1,023 usable unicast elements** per slot).
3. **Tier-2 Dynamic Flow Alias Pool (`0x7800..0x7FFF`)**:
   - 2,048 dedicated addresses completely disjoint from Tier-1 slots.
   - Used for overflow, remote devices with physical element address $\ge 1,024$, and transit proxies.
   - **Reservation Invariant**: Tier-1 slot formulas never emit addresses $\ge 0\text{x7800}$. Zero collision between static and dynamic windows!

### 2.1 Post-Boot `global_subnet_id` Collision Recovery
If two independently bootstrapped islands collide on the same 16-bit `global_subnet_id` ($0.0015\%$ base probability) upon coming into RF proximity:
- The discovering bridge compares `(anchor_uuid, NetKey_digest)` against its registered subnet table.
- On mismatch with an existing `global_subnet_id`, the newly joining island's Anchor increments its local deterministic salt index and re-runs the bounded CRC16 derivation (`nonce++`), notifying peer nodes via authenticated UVP before committing the distinct `global_subnet_id` to the bridge registry.

---

## 3. Subnet Prefix Mapping & Dynamic Alias Allocation

### 3.1 Tier-1 Static Mapping Formula ($n \in [0..13]$)
$$\mathbf{\text{Virtual Proxy Address} = 0\text{x4000} \mid (slot\_id \ll 10) \mid (phys\_addr \ \& \ 0\text{x03FF})}$$

### 3.2 Tier-2 Dynamic Bidirectional NAT Allocation & Deterministic Fallback
When a foreign element has `phys_addr >= 1024`, or when slot collision occurs:
- The router allocates a 1-to-1 dynamic alias from the dedicated `0x7800..0x7FFF` pool (2,048 available addresses).
- **Active in-flight table is bounded to 16 concurrent active entries** ($16 \times 19\text{ B} = 304\text{ B}$ raw packed footprint, budgeted at $16 \times 24\text{ B} = 384\text{ B}$ in RAM with 5 B platform lookup/hash metadata), strictly conforming to the 3.5 KB RAM ceiling.
- **Persistent vs Volatile Tier-2 Eviction Policy (`NAT_FLAG_PERSISTENT`)**:
  - Entries backing active endpoints in `meshx_pair_record_t` carry `NAT_FLAG_PERSISTENT` (Bit 2) and are **exempt from the 300s inactivity timeout**. If evicted from RAM under load, they re-hydrate deterministically from NVS `meshx_pair_record_t`.
  - Volatile entries (`NAT_FLAG_PERSISTENT == 0`, including 30s temporary discovery probes) expire after 300s inactivity (`(int32_t)(now_s - last_activity_ts_s) >= 300`, wrap-safe delta).
  - If all 16 entries are active when a 17th entry is needed: temporary discovery aliases are evicted first, then oldest volatile entries (`LRU`). If all 16 entries are pinned `NAT_FLAG_PERSISTENT`, the new allocation fails cleanly and returns `0xD7` (`ERR_NAT_FULL`).
- **Deterministic Alias Hash Derivation & Standby Recovery**:
  - Dynamic alias assignment uses the forward flow tuple:
    $$\mathbf{\text{alias\_offset} = \text{CRC16}(\text{global\_subnet\_src} \parallel \text{phys\_src\_addr} \parallel \text{global\_subnet\_dst} \parallel \text{phys\_dst\_addr}) \ \& \ 0\text{x07FF}}$$
    $$\mathbf{\text{deterministic\_alias} = 0\text{x7800} \mid \text{alias\_offset}}$$
  - If a collision occurs with an active entry, linear probing with step $+1$ (wrapping modulo `0x0800`) is used.
  - **Replication Before Forwarding Invariant**: The active bridge SHALL transmit authenticated `0xD5` (`BRIDGE_NAT_SYNC`) and receive acknowledgement before dispatching the translated frame into the egress subnet (ensuring any linear-probed collision offset is replicated to Standby before egress).
  - **Return Flow Recovery on Standby Promoted Bridge**:
    - When return frames transit back across intermediate hops, the multi-hop transit header (`meshx_subnet_transit_hdr_t`) encapsulates `orig_subnet_id` and `orig_phys_src`.
    - Furthermore, during cross-subnet intent-driven pairing, both communicating endpoints persist their peer binding record in NVS (`meshx_pair_record_t`). If Standby promotes before a non-colliding entry replicates, the Standby bridge matches the incoming return frame's `(virtual_proxy_addr, dst_phys_addr)` against `meshx_pair_record_t` to disambiguate multiple pairings per destination, resolve `(global_subnet_src, phys_src_addr)`, and re-evaluate the deterministic hash to restore the exact reverse translation entry.
- **Boundary Invariant**: Tier-1 translation strictly enforces `(phys_addr >= 1 && phys_addr < 1024)`. Any physical address with `phys_addr >= 1024` is automatically steered to Tier-2 dynamic allocation, preventing any truncation collision with slot base addresses.

```c
/* Natural field ordering: 32-bit -> 16-bit -> 8-bit prevents unaligned access faults (19 B packed) */
typedef struct {
    uint32_t last_activity_ts_s;    /* Monotonic seconds timestamp (300s volatile TTL; wrap-safe delta) */
    uint16_t ingress_net_idx;       /* Ingress Subnet NetKey Index */
    uint16_t egress_net_idx;        /* Egress Subnet NetKey Index */
    uint16_t global_subnet_src;     /* Source Global Subnet ID */
    uint16_t phys_src_addr;         /* Source physical unicast address (full 15-bit) */
    uint16_t global_subnet_dst;     /* Destination Global Subnet ID */
    uint16_t phys_dst_addr;         /* Destination physical unicast address (full 15-bit) */
    uint16_t virtual_proxy_addr;    /* Assigned virtual address in 0x7800..0x7FFF */
    uint8_t  flags;                 /* Bit 0: FORWARD, Bit 1: REVERSE, Bit 2: PERSISTENT, Bit 3: TEMP_DISCOVERY */
} MESHX_PACKED meshx_nat_entry_t;
```

---

## 4. NAT Table Synchronization & Takeover Reconciliation

- **Immediate Synchronization (`0xD5`)**: Whenever an Active Bridge allocates a dynamic NAT mapping, it transmits an authenticated `MESHX_UVP_OP_BRIDGE_NAT_SYNC` (`0xD5`) authenticated with $K_{\text{bridge}}$ HMAC.
- **Standby Table Audit on Takeover**: Upon executing takeover claim (`0xD2`), the promoted Standby Bridge audits its local NAT table against its local slot registry. If an in-flight entry was lost during active bridge power-off, the router regenerates the identical deterministic alias using the deterministic hash rule on the first packet, preventing dropped return traffic.
