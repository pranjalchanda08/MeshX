# TRD Page 3: Autonomous Group Addressing & Subnet Offset Prefixing

**Module:** `TRD-SUBNET-03`  
**Parent Index:** [TRD Index](./README.md)  
**Requirements:** [requirements.md](../requirements.md) (REQ-F02)  
**Parent Integration:** [.specs/autonomous-element-pairing/](../autonomous-element-pairing/requirements.md) (REQ-005)  
**Related Fixes:** Remediation of R4/C findings from `REVIEW_ROUND2_FINDINGS.md`  

---

## 1. Autonomous Group Collision Context

MeshX specifies **Autonomous Group Derivation** in `autonomous-element-pairing` (REQ-005):
$$\mathbf{\text{Local Group Address} = 0\text{xC000} \mid (\text{Anchor\_Unicast} \ \& \ 0\text{x03FF})}$$

Because Island A's Master Anchor is `0x0001`, and Island B's Master Anchor is ALSO `0x0001`:
$$\text{Both islands calculate identical local group address: } \mathbf{0\text{xC001}}!$$

---

## 2. Subnet Group Offset Prefixing & Band Allocation

To prevent the address overflow identified in Round 2 (where extrapolating 16 group bands exceeded the valid BLE Mesh multicast window `0xC000..0xFEFF`):

### 2.1 The 14-Band Subnet Group Formula
The group address space is partitioned into fourteen non-overlapping 1,024-address group blocks:

$$\mathbf{\text{band}(n) = 0\text{xC000} + ((n + 1) \ll 10)} \quad \text{for } n \in [0..13]$$

| Slot Index ($n$) | Virtual Group Band | Address Range | Target Subnet Mapped |
|---|---|---|---|
| **Local Island** | Base Band | `0xC000..0xC3FF` | Local Autonomous Groups (`0xC001`) |
| **Slot 0** | Band 0 | `0xC400..0xC7FF` | Remote Subnet Slot 0 (e.g. Island B) |
| **Slot 1** | Band 1 | `0xC800..0xCBFF` | Remote Subnet Slot 1 (e.g. Island C) |
| **Slot 2** | Band 2 | `0xCC00..0xCFFF` | Remote Subnet Slot 2 (e.g. Island D) |
| ... | ... | ... | ... |
| **Slot 13** | Band 13 | `0xF800..0xFBFF` | Remote Subnet Slot 13 |
| **Overflow (14+)** | Dynamic Fallback | Explicit 1:1 NAT | Dynamic group mapping rules |

### 2.2 Strict Overflow Invariant
- Maximum Band 13 address is `0xFBFF`.
- In Bluetooth Mesh (v1.1 §3.4.2.4), fixed group addresses are defined in `0xFF00..0xFFFF` (All-nodes `0xFFFF`, Relays `0xFFFE`, Friends `0xFFFD`, Proxies `0xFFFC`), while `0xFC00..0xFEFF` represents unallocated dynamically assignable group space.
- The 14-band group formula strictly constrains static derivation to `0xC400..0xFBFF`.
- Subnet connections beyond the 14 static slots ($n \ge 14$) fallback to explicit dynamic group translation mappings, guaranteeing that static derivation never enters `0xFC00..0xFFFF`.

---

## 3. Dynamic Group Translation Rule

When Switch on Island A is paired to lamps on both Island A and Island B:
$$\{ \text{NetKey\_A}, \text{Grp } 0\text{xC001} \} \longleftrightarrow \{ \text{NetKey\_B}, \text{Grp } 0\text{xC401} \}$$

- Switch toggles `0xC001`.
- Local lamps on Island A toggle via `0xC001`.
- **Selective Egress Filtering**: The Bridge Router evaluates an explicit per-egress-subnet subscription bitmask (`group_bridge_mask`) populated when bindings are created. Purely local `0xC001` traffic with zero active cross-subnet group bindings on a neighbor subnet is **NOT** forwarded, preventing star-hub broadcast storms across idle islands.
- For each subscribed target subnet (e.g. Island B), the Bridge Router rewrites the destination group to `0xC401`, re-encrypts under `NetKey_B`, and floods into Island B.
- Remote lamps on Island B subscribe at the BLE Mesh layer strictly to `0xC401` (never `0xC001`), toggling in exact synchronization while reading `CANONICAL_DST = 0xC001` solely inside the Transport Shim's TransMIC nonce derivation.
- Island B's local groups at `0xC001` are never disturbed.
