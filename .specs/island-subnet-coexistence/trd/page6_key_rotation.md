# TRD Page 6: Multi-Subnet Key Rotation (KR) Orchestration

**Module:** `TRD-SUBNET-06`  
**Parent Index:** [TRD Index](./README.md)  
**Requirements:** [requirements.md](../requirements.md) (REQ-F04)  
**Parent Integration:** [.specs/meshx-prov-srv-mxcp/trd/page4_key_rotation.md](../meshx-prov-srv-mxcp/trd/page4_key_rotation.md)  
**Related Fixes:** Remediation of R3 findings from `REVIEW_FINDINGS.md`  

---

## 1. Multi-Subnet Key Rotation Challenges

In MeshX, each island executes periodic or compromise-driven Key Rotation via UVP broadcast bursts (`0xC4` $\rightarrow$ `0xC5` $\rightarrow$ `0xC6`):
- **Phase 0 (Normal)**: Old key active.
- **Phase 1 (Key Distribution)**: New key delivered, nodes still transmit on old key.
- **Phase 2 (Key Transition)**: Nodes transmit on new key, accept old or new key.
- **Phase 3 (Old Key Revocation)**: Old key is permanently erased.

### Isolation Imperatives:
1. **Cascade Isolation**: Key Rotation on Island A must never stall, pause, or desynchronize Island B.
2. **Burst Containment**: UVP Key Refresh control bursts (`0xC4`, `0xC5`, `0xC6`) must **never** be forwarded across subnet boundaries; bridge nodes terminate them locally.
3. **Dual-Key Bridge Tracking**: A bridge node must maintain **two key slots** ($K_{\text{current}}$ and $K_{\text{new}}$) and an independent `kr_phase` for **every** bridged subnet simultaneously.

---

## 2. Dedicated Crypto Slots & Single-Epoch Reassembly Binding

To prevent interleaved old/new-key segments from corrupting reassembly buffers during Phase 1/2:

```c
typedef enum {
    MESHX_KR_PHASE_0_NORMAL     = 0, /* Transmit old, accept old */
    MESHX_KR_PHASE_1_DISTRIBUTE = 1, /* Transmit old, accept old or new */
    MESHX_KR_PHASE_2_TRANSITION = 2, /* Transmit new, accept old or new */
    MESHX_KR_PHASE_3_REVOKED    = 3, /* Transmit new, accept new (old erased) */
} meshx_kr_phase_t;

/* Natural field ordering: uint32_t -> uint16_t -> uint8_t -> key arrays = 49 B packed */
typedef struct {
    uint32_t        current_epoch;       /* Subnet-specific epoch */
    uint32_t        iv_index;            /* Subnet-specific IV index (IV Update before seq >= 0xF00000) */
    uint32_t        last_seq_num;        /* Egress Sequence Number for this subnet */
    uint16_t        net_idx;             /* Local platform net_idx */
    uint16_t        global_subnet_id;    /* 16-bit global subnet identifier */
    uint8_t         kr_phase;            /* meshx_kr_phase_t */
    meshx_key_t     current_key;         /* Active / Old NetKey (16 B) */
    meshx_key_t     new_key;             /* In-flight New NetKey during Phase 1/2 (16 B) */
} MESHX_PACKED meshx_subnet_crypto_slot_t;
```

### 2.1 Strict Single-Epoch Rule & Mid-SAR Continuity
- **Stage 1 Ingress**: Segments of an Upper Transport PDU must decrypt with the **exact same key epoch**. If a packet starts receiving segments under `current_key`, any segment decrypted with `new_key` for that same `SeqZero` is dropped. On Phase 3 revocation, incomplete Stage 1 SAR buffers still tagged with the erased old epoch are aborted immediately.
- **Stage 2 Egress Latch**: Stage 2 egress SAR latches `(egress_key_ptr, egress_epoch)` at **Segment 0 dispatch** and transmits every remaining segment and retry under that latched key, preventing Phase 1 $\rightarrow$ Phase 2 transitions mid-burst from violating Single-Epoch validation at receivers. If Phase 3 erases `current_key` before SAR completion, Stage 2 aborts and restarts cleanly from Segment 0 under `new_key`.
- **Inter-Bridge KR Desync Recovery**: Active bridges include per-subnet `(kr_phase, current_epoch)` digests inside periodic `0xD1` heartbeats so Standby or recovering bridges reconcile missed `0xC4`/`0xC5`/`0xC6` bursts prior to transmitting.
- **Sequence Exhaustion / IV Update Margin**: Before `last_seq_num` approaches 24-bit saturation (trigger threshold `>= 0xF00000`, accounting for `+512` boot recovery jumps), the subnet initiates standard BLE Mesh IV Index Update so sequence wraparound never breaks peer RPLs.

---

## 3. Forwarding & Key Selection Logic

When translating a frame from Ingress Subnet $X$ to Egress Subnet $Y$:

- **Decoupled Ingress Acceptance**: Ingress frame acceptance depends **strictly on the Key Rotation phase of Ingress Subnet $X$**, completely decoupled from Egress Subnet $Y$.
- **Decoupled Egress Key Selection**: Egress encryption key selection depends **strictly on the Key Rotation phase of Egress Subnet $Y$**.

| Ingress Subnet $X$ Phase | Ingress Acceptance Policy |
|---|---|
| **Phase 0 (Normal)** | Only frames decrypting under `current_key[X]` accepted |
| **Phase 1 (Distribution)** | Frames decrypting under either `current_key[X]` or `new_key[X]` accepted (tagged in queue entry) |
| **Phase 2 (Transition)** | Frames decrypting under either `current_key[X]` or `new_key[X]` accepted (tagged in queue entry) |
| **Phase 3 (Revocation)** | `new_key[X]` promoted to `current_key[X]`; old key permanently wiped |

| Egress Subnet $Y$ Phase | Egress Encryption Key Selected by Router |
|---|---|
| **Phase 0 (Normal)** | Encrypt with `current_key[Y]` |
| **Phase 1 (Distribution)** | Encrypt with `current_key[Y]` |
| **Phase 2 (Transition)** | Encrypt with `new_key[Y]` |
| **Phase 3 (Revocation)** | `new_key[Y]` promoted to `current_key[Y]`; encrypt with `current_key[Y]` |

---

## 4. AppKey_Shared Independence from NetKey Rotation

- Application Keys (`AppKey_Shared`) are cryptographically independent of Network Keys (`NetKey`).
- When Island A executes a NetKey rotation and advances its network epoch, `AppKey_Shared` remains valid and unperturbed.
- The router continues relaying Upper Transport PDUs across the boundary without requiring simultaneous AppKey updates on peer islands.
