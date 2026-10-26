# TRD Page 6: Long-Term Inactivity Eviction & DevKey Re-Healing

**Module:** `TRD-PROV-06`  
**Parent Document:** [TRD Index](./README.md)  
**Related Requirements:** [requirements.md](../requirements.md) (REQ-F09)  

---

## Table of Contents
- [1. Eviction Horizon (>10 Minutes Inactivity)](#1-eviction-horizon-10-minutes-inactivity)
- [2. Re-Healing Protocol Sequence](#2-re-healing-protocol-sequence)
- [3. Node Lifecycle State Machine](#3-node-lifecycle-state-machine)
- [4. Re-Healing Payload Wire Layouts](#4-re-healing-payload-wire-layouts)
  - [Request Payload (MESHX_UVP_OP_REHEAL_BEACON_REQ = 0xCE)](#request-payload-meshx_uvp_op_reheal_beacon_req--0xce)
  - [Response Payload (MESHX_UVP_OP_REHEAL_ADMIT_RSP = 0xCF)](#response-payload-meshx_uvp_op_reheal_admit_rsp--0xcf)

---

## 1. Eviction Horizon (>10 Minutes Inactivity)

When a node remains offline for $>10\text{ minutes}$ across multiple Key Rotation cycles, it falls outside the active Dual-RX overlap window ($\Delta\text{Epoch} \ge 2$). Inbound mesh beacons fail MIC authentication because the old NetKey has been purged from the network.

---

## 2. Re-Healing Protocol Sequence

```mermaid
sequenceDiagram
    autonumber
    participant Evicted as Evicted Node (Epoch 100)
    participant Proxy as Proxy / Relay Node
    participant Master as Master Provisioner (Epoch 102)

    Note over Evicted: Step 1: Isolation & Eviction Detection (>10 min offline)
    Evicted->>Evicted: Power ON after long outage / radio blackout
    Evicted->>Evicted: Inbound mesh beacons fail MIC authentication (K100 revoked)
    Evicted->>Evicted: Isolation watchdog expires (>600s / Delta Epoch >= 2)
    Evicted->>Evicted: Transition state = PROV_STATE_ISOLATED_HEALING

    Note over Evicted,Proxy: Step 2: Authenticated Re-Healing Request
    Evicted->>Evicted: Generate 32-bit Monotonic Nonce / Timestamp
    Evicted->>Evicted: Compute HMAC-SHA256 signature using permanent DevKey over [UUID + Addr + Nonce]
    Evicted->>Proxy: BLE Advertisement: MESHX_UVP_OP_REHEAL_BEACON_REQ (0xCE) [UUID, Old_Addr, Nonce, HMAC-16]

    Note over Proxy,Master: Step 3: UVP Tunneling & DevKey Verification
    Proxy->>Master: UVP Mesh Forward: MESHX_UVP_OP_REHEAL_BEACON_REQ (0xCE) (Relayed with proxy RSSI)
    Master->>Master: Lookup Device UUID in Provisioned Node Database
    Master->>Master: Retrieve stored DevKey for target UUID
    Master->>Master: Verify HMAC-SHA256 signature & freshness of Nonce
    Note over Master: Authenticated: Device is legitimate former member

    Note over Evicted,Master: Step 4: Encrypted Re-Admission & State Restoration
    Master->>Master: Encrypt admission payload with target Node's DevKey [Active NetKey_102, Epoch 102, Addr]
    Master->>Proxy: UVP Mesh Response: MESHX_UVP_OP_REHEAL_ADMIT_RSP (0xCF)
    Proxy->>Evicted: BLE Direct / PB-ADV: MESHX_UVP_OP_REHEAL_ADMIT_RSP (0xCF)
    Evicted->>Evicted: Decrypt payload using local immutable DevKey
    Evicted->>Evicted: Install Active NetKey_102, update Epoch = 102, commit to NVS
    Evicted->>Evicted: Transition state = PROV_STATE_PROVISIONED
    Note over Evicted: Node autonomously re-admitted without button presses or factory wipe
```

---

## 3. Node Lifecycle State Machine

```mermaid
stateDiagram-v2
    [*] --> PROV_STATE_PROVISIONED: Initial Onboarding Complete
    
    PROV_STATE_PROVISIONED --> PROV_STATE_PROVISIONED: Periodic Key Rotation (Sync, Epoch N+1)
    
    PROV_STATE_PROVISIONED --> PROV_STATE_OFFLINE: Power Loss / Radio Blackout / Disconnect
    
    state PROV_STATE_OFFLINE {
        [*] --> Short_Outage
        Short_Outage --> Reconnected: Power ON (< 4 mins)
        Reconnected --> Dual_RX_Active: Key in secondary slot
        Dual_RX_Active --> Normal_Operation: Overlap recovery
        
        Short_Outage --> Long_Outage: Inactive > 10 mins (600s)
        Long_Outage --> Key_Revoked: Delta Epoch >= 2 (Old keys wiped from mesh)
        Key_Revoked --> Node_Evicted: Master marks DECOMMISSIONED_STALE
    }
    
    PROV_STATE_OFFLINE --> PROV_STATE_ISOLATED_HEALING: Power ON after > 10 mins (MIC Auth Fails)
    
    state PROV_STATE_ISOLATED_HEALING {
        [*] --> Generate_Nonce
        Generate_Nonce --> Compute_HMAC: Signs [UUID + Addr + Nonce] with immutable DevKey
        Compute_HMAC --> Transmit_Beacon: Broadcasts MESHX_UVP_OP_REHEAL_BEACON_REQ (0xCE)
        Transmit_Beacon --> Await_Admit_Rsp: Listens on BLE / PB-ADV
        Await_Admit_Rsp --> Decrypt_Payload: Receives MESHX_UVP_OP_REHEAL_ADMIT_RSP (0xCF)
        Decrypt_Payload --> Install_Credentials: Decrypts with DevKey (K_active, Epoch)
        Install_Credentials --> Commit_NVS: Writes updated epoch and NetKey to flash
    }
    
    PROV_STATE_ISOLATED_HEALING --> PROV_STATE_PROVISIONED: Re-Admission Verified & Synchronized
    PROV_STATE_ISOLATED_HEALING --> [*]: Manual Factory Reset (Fallback)
```

---

## 4. Re-Healing Payload Wire Layouts

### Request Payload (`MESHX_UVP_OP_REHEAL_BEACON_REQ = 0xCE`)
- `target_uuid[16]`: 16-byte Device UUID.
- `previous_addr[2]`: Last assigned 16-bit unicast address.
- `timestamp_nonce[4]`: 32-bit monotonic timestamp / nonce to prevent replay attacks.
- `hmac_signature[16]`: HMAC-SHA256 truncated to 16 bytes computed with the node's permanent `DevKey` over `[target_uuid || previous_addr || timestamp_nonce]`.

### Response Payload (`MESHX_UVP_OP_REHEAL_ADMIT_RSP = 0xCF`)
- Encrypted with target node's `DevKey`.
- `active_netkey[16]`: Current active NetKey.
- `active_epoch[4]`: Current active epoch.
- `assigned_addr[2]`: Restored or newly allocated unicast address.
