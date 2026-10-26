# Design Specification: Autonomous Re-Healing & Cascading Redundancy (N+1)

**Feature:** Autonomous DevKey Re-Healing & N+1 Cascading Provisioner Redundancy
**Subsystems:** `meshx_uvp_dispatcher`, `meshx_mxcp`, `meshx_crypto`, `meshx_nvs`, `meshx_group_allocator`
**Status:** Approved
**Related Requirements:** [requirements.md](./requirements.md)

---

## 1. System Architecture & Component Interaction

```mermaid
flowchart TD
    classDef prov fill:#2ecc71,stroke:#27ae60,stroke-width:2px,color:#fff;
    classDef relay fill:#3498db,stroke:#2980b9,stroke-width:2px,color:#fff;
    classDef isolated fill:#e74c3c,stroke:#c0392b,stroke-width:2px,color:#fff;
    classDef crypto fill:#9b59b6,stroke:#8e44ad,stroke-width:2px,color:#fff;

    Master["Master Provisioner (0x0001)<br/>- NVS DevKey Database<br/>- Unified 15-Bit Address Space"]:::prov
    Relay["Proxy / Relay Node<br/>- Encapsulates 0xCE in K_new<br/>- Rate Limiter (1 / 30s)"]:::relay
    Evicted["Evicted / Isolated Node<br/>- Cryptographically Blind<br/>- Broadcasts 0xCE Beacon"]:::isolated
    Crypto["MeshX Crypto Subsystem<br/>- HMAC-SHA256<br/>- AES-128-CCM (DevKey)"]:::crypto

    Evicted -- "1. BLE Adv: 0xCE \n[UUID, Nonce, HMAC]" --> Relay
    Relay -- "2. Multi-Hop Mesh UVP \n(Active K_new)" --> Master
    Master <--> Crypto
    Master -- "3. UVP Response: 0xCF \n[Encrypted with DevKey]" --> Relay
    Relay -- "4. BLE Direct PB-ADV" --> Evicted
```

---

## 2. Wire Protocol Definitions

### 2.1 UVP Opcode Extensions

| Opcode | Identifier | Direction | Purpose |
|---|---|---|---|
| `0xCD` | `MESHX_UVP_OP_REDUNDANCY_CLAIM` | Master $\rightarrow$ Broadcast | Invite candidate nodes to take over Standby role ($N+1$). |
| `0xCE` | `MESHX_UVP_OP_REHEAL_BEACON_REQ` | Evicted Node $\rightarrow$ BLE Adv | Plaintext beacon with HMAC-SHA256 signature for re-admission. |
| `0xCF` | `MESHX_UVP_OP_REHEAL_ADMIT_RSP` | Master $\rightarrow$ Evicted Node | DevKey-encrypted response payload with active NetKey & Epoch. |
| `0xD0` | `MESHX_UVP_OP_REDUNDANCY_CLAIM_ACK` | Candidate $\rightarrow$ Master | Candidate acknowledges standby claim and activates watchdog. |

### 2.2 Wire Struct Layouts

```c
#pragma pack(push, 1)

/**
 * @brief Payload for MESHX_UVP_OP_REHEAL_BEACON_REQ (0xCE)
 * Total length: 38 bytes
 */
typedef struct {
    uint8_t  target_uuid[16];   /* 16-byte Device UUID */
    uint16_t previous_addr;     /* Last known 16-bit unicast address */
    uint32_t timestamp_nonce;   /* Monotonic counter / timestamp preventing replay */
    uint8_t  hmac_signature[16];/* HMAC-SHA256 truncated to 16 bytes over [UUID || Addr || Nonce] */
} meshx_uvp_reheal_req_t;

/**
 * @brief Plaintext payload before DevKey encryption for MESHX_UVP_OP_REHEAL_ADMIT_RSP (0xCF)
 * Total length: 22 bytes + 4 bytes MIC
 */
typedef struct {
    uint8_t  active_netkey[16]; /* Current active Network Key */
    uint32_t active_epoch;      /* Current key rotation epoch */
    uint16_t assigned_addr;     /* Assigned or restored unicast address */
} meshx_uvp_reheal_rsp_plain_t;

/**
 * @brief Payload for MESHX_UVP_OP_REDUNDANCY_CLAIM (0xCD)
 */
typedef struct {
    uint32_t claim_epoch;       /* Master claim epoch */
    uint16_t master_addr;       /* Master unicast address */
    uint16_t next_free_addr;    /* Monotonic allocation high-water mark */
    uint16_t reserved;          /* Reserved alignment padding */
} meshx_uvp_redundancy_claim_t;

/**
 * @brief Payload for MESHX_UVP_OP_REDUNDANCY_CLAIM_ACK (0xD0)
 */
typedef struct {
    uint8_t  candidate_uuid[16];/* Candidate node UUID accepting Standby */
    uint16_t candidate_addr;    /* Candidate unicast address */
} meshx_uvp_redundancy_claim_ack_t;

#pragma pack(pop)
```

---

## 3. Sequence & Interaction Diagrams

### 3.1 Autonomous DevKey Re-Healing Sequence

```mermaid
sequenceDiagram
    autonumber
    participant Evicted as Evicted Node (Epoch 100)
    participant Relay as Proxy / Relay Node
    participant Master as Master Provisioner (Epoch 102)

    Note over Evicted: Step 1: Isolation & Eviction Detection (15s Blindness / Delta Epoch >= 2)
    Evicted->>Evicted: Power ON after long outage / radio blackout
    Evicted->>Evicted: Inbound mesh beacons fail MIC authentication (K100 revoked)
    Evicted->>Evicted: Blindness watchdog expires (15s silence / Delta Epoch >= 2)
    Evicted->>Evicted: Transition state = PROV_STATE_ISOLATED_HEALING

    Note over Evicted,Relay: Step 2: Authenticated Re-Healing Request
    Evicted->>Evicted: Generate 32-bit Monotonic Nonce / Timestamp
    Evicted->>Evicted: Compute HMAC-SHA256 signature using permanent DevKey over [UUID + Addr + Nonce]
    Evicted->>Relay: BLE Advertisement: MESHX_UVP_OP_REHEAL_BEACON_REQ (0xCE) [UUID, Old_Addr, Nonce, HMAC-16]

    Note over Relay,Master: Step 3: Zero-Trust Courier UVP Tunneling
    Relay->>Master: UVP Mesh Forward: MESHX_UVP_OP_REHEAL_BEACON_REQ (0xCE) (Encrypted in K_new)
    Master->>Master: Lookup Device UUID in NVS DevKey database
    Master->>Master: Verify HMAC-SHA256 signature & freshness of Nonce
    Note over Master: Authenticated: Device is legitimate former member

    Note over Evicted,Master: Step 4: Encrypted Re-Admission & State Restoration
    Master->>Master: Encrypt admission payload with target Node's DevKey [Active NetKey_102, Epoch 102, Addr]
    Master->>Relay: UVP Mesh Response: MESHX_UVP_OP_REHEAL_ADMIT_RSP (0xCF)
    Relay->>Evicted: BLE Direct / PB-ADV: MESHX_UVP_OP_REHEAL_ADMIT_RSP (0xCF)
    Evicted->>Evicted: Decrypt payload using local immutable DevKey
    Evicted->>Evicted: Install Active NetKey_102, update Epoch = 102, commit to NVS
    Evicted->>Evicted: Transition state = PROV_STATE_PROVISIONED
```

### 3.2 Cascading Standby Claim ($N+1$) Sequence

```mermaid
sequenceDiagram
    autonumber
    participant Node2 as Node 2 (Promoted Master)
    participant RF as BLE Mesh Medium (0x8F42)
    participant Node4 as Node 4 (Candidate Node)

    Note over Node2: Step 1: Promotion & Initial Claim Broadcast
    Node2->>Node2: 15s Heartbeat Watchdog fires -> Promotes to MASTER
    Node2->>RF: Broadcast: MESHX_UVP_OP_REDUNDANCY_CLAIM (0xCD) [Attempt 1]
    Note over RF: Packet Dropped / RF Collision or Node 4 Offline
    Node2->>Node2: Claim Response Timer (2s) expires -> No Candidate Reply

    Note over Node2: Step 2: Exponential Backoff Retries
    Node2->>RF: Broadcast: MESHX_UVP_OP_REDUNDANCY_CLAIM (0xCD) [Attempt 2: 4s interval]
    Note over RF: Still no candidate reply
    Node2->>Node2: Claim Response Timer (4s) expires
    Node2->>RF: Broadcast: MESHX_UVP_OP_REDUNDANCY_CLAIM (0xCD) [Attempt 3: 8s interval]

    Note over Node2,Node4: Step 3: Candidate Wakeup / Acknowledgment
    Node4->>Node4: Node 4 finishes boot / rejoins mesh as ROLE_CANDIDATE
    Node4->>RF: Hears Attempt 3 of MESHX_UVP_OP_REDUNDANCY_CLAIM (0xCD)
    Node4->>Node4: Transitions state = ROLE_STANDBY
    Node4->>Node2: Unicast UVP: MESHX_UVP_OP_REDUNDANCY_CLAIM_ACK (0xD0) [Node 4 UUID]
    Node2->>Node2: Disarms Claim Retry Timer
    Node2->>Node4: Standard 5s Heartbeats (0xC7) Established
    Note over Node2,Node4: Cascading Redundancy (N+1) Successfully Restored
```
