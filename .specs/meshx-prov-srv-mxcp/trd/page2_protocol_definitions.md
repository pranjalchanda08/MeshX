# TRD Page 2: Protocol & Wire Definitions

**Module:** `TRD-PROV-02`
**Parent Document:** [TRD Index](./README.md)
**Related Requirements:** [requirements.md](../requirements.md) (REQ-F01, REQ-F02, REQ-F03)

---

## Table of Contents
- [1. MXCP Wire Format](#1-mxcp-wire-format)
- [2. MXCP Host Commands (Host → Engine, Bit 7 = 0)](#2-mxcp-host-commands-host--engine-bit-7--0)
- [3. MXCP Host Events (Engine → Host, Bit 7 = 1)](#3-mxcp-host-events-engine--host-bit-7--1)
- [4. UVP Vendor Opcode Extensions (Inter-Node Mesh Control)](#4-uvp-vendor-opcode-extensions-inter-node-mesh-control)
- [5. Packed C Structures](#5-packed-c-structures)

---

## 1. MXCP Wire Format

Every MXCP frame conforms to the canonical wire structure:
`[SOF 0xFE] [LEN] [TYPE] [PAYLOAD (0..N bytes)] [CHECKSUM] [EOF 0xEF]`

```python
CHECKSUM = LEN ^ TYPE ^ XOR(PAYLOAD[0..N-1])
```

---

## 2. MXCP Host Commands (Host → Engine, Bit 7 = 0)

| Command ID | Enum Name | Description | Payload Struct |
|---|---|---|---|
| `0x0B` | `MXCP_CMD_PROV_ENABLE` | Enable virgin unprov beaconing | `mxcp_cmd_prov_enable_t` |
| `0x0C` | `MXCP_CMD_PROV_DISABLE` | Disable virgin unprov beaconing | None (0 bytes) |
| `0x0D` | `MXCP_CMD_PROV_GET_STATE` | Query node prov state & unicast addr | None (0 bytes) |
| `0x0E` | `MXCP_CMD_PROV_SET_NAME` | Set unprov device friendly name | `mxcp_cmd_prov_set_name_t` |
| `0x0F` | `MXCP_CMD_PROV_AUTH_INPUT` | Submit user/host OOB auth input | `mxcp_cmd_prov_auth_input_t` |
| `0x30` | `MXCP_CMD_UVP_RPR_SCAN_START` | Command mesh nodes to scan for unprov beacons | `mxcp_cmd_uvp_rpr_scan_t` |
| `0x31` | `MXCP_CMD_UVP_RPR_TUNNEL_SEND`| Tunnel prov PDU via intermediate node | `mxcp_cmd_uvp_rpr_tunnel_t` |
| `0x32` | `MXCP_CMD_UVP_KEY_ROTATE_TRIGGER` | Initiate 5-min key rotation phase | `mxcp_cmd_uvp_key_rotate_t` |
| `0x33` | `MXCP_CMD_UVP_REDUNDANCY_SET_ROLE`| Configure Master/Standby redundancy role | `mxcp_cmd_uvp_redundancy_role_t` |
| `0x34` | `MXCP_CMD_UVP_FAST_PROV_START` | Initiate fast bulk provisioning session | `mxcp_cmd_uvp_fast_prov_start_t` |
| `0x36` | `MXCP_CMD_NODE_REPLACE_START`  | Initiate supervised physical node replacement | `mxcp_cmd_node_replace_start_t` |

---

## 3. MXCP Host Events (Engine → Host, Bit 7 = 1)

| Event ID | Enum Name | Description | Payload Struct |
|---|---|---|---|
| `0x81` | `MXCP_EVT_PROV_COMP` | Virgin node provisioning complete | `mxcp_evt_prov_comp_t` |
| `0x82` | `MXCP_EVT_PROV_FAILED` | Virgin node provisioning failed | `mxcp_evt_prov_failed_t` |
| `0x8C` | `MXCP_EVT_PROV_STATE_IND` | Local provisioning status indication | `mxcp_evt_prov_state_ind_t` |
| `0x8D` | `MXCP_EVT_PROV_LINK_OPEN` | Local link opened | `mxcp_evt_prov_link_open_t` |
| `0x8E` | `MXCP_EVT_PROV_LINK_CLOSE`| Local link closed | `mxcp_evt_prov_link_close_t` |
| `0x8F` | `MXCP_EVT_PROV_AUTH_REQ` | Local OOB authentication request | `mxcp_evt_prov_auth_req_t` |
| `0xB0` | `MXCP_EVT_UVP_RPR_BEACON_IND` | Unprov beacon discovered by remote mesh node | `mxcp_evt_uvp_rpr_beacon_ind_t` |
| `0xB1` | `MXCP_EVT_UVP_RPR_TUNNEL_RECV`| PDU response received from remote unprov node | `mxcp_evt_uvp_rpr_tunnel_recv_t` |
| `0xB2` | `MXCP_EVT_UVP_KEY_ROTATION_PHASE`| Key rotation phase update (Phase 1, 2, 3) | `mxcp_evt_uvp_key_rot_phase_t` |
| `0xB3` | `MXCP_EVT_UVP_REDUNDANCY_STATUS`| Active/Standby health & epoch status | `mxcp_evt_uvp_redundancy_status_t` |
| `0xB4` | `MXCP_EVT_UVP_FAST_PROV_PROGRESS`| Bulk provisioning batch progress update | `mxcp_evt_uvp_fast_prov_progress_t` |
| `0xB6` | `MXCP_EVT_NODE_REPLACE_STATUS` | Supervised replacement progress & address assignment | `mxcp_evt_node_replace_status_t` |
| `0xB7` | `MXCP_EVT_NODE_REPLACE_SCHEMA_DIFF` | Composition model divergence & orphaned/new model indication | `mxcp_evt_node_replace_schema_diff_t` |

---

## 4. UVP Vendor Opcode Extensions (Inter-Node Mesh Control)

| UVP Opcode | Macro / Identifier | Direction | Purpose |
|---|---|---|---|
| `0xC0` | `MESHX_UVP_OP_RPR_SCAN_REQ` | Master $\rightarrow$ Mesh Group | Instruct intermediate nodes to listen for unprov beacons |
| `0xC1` | `MESHX_UVP_OP_RPR_BEACON_REPORT`| Intermediate Node $\rightarrow$ Master | Report discovered unprov device UUID, RSSI, OOB info |
| `0xC2` | `MESHX_UVP_OP_RPR_TUNNEL_DOWN` | Master $\rightarrow$ Intermediate Node | Send provisioning PDU down to unprov node over PB-ADV |
| `0xC3` | `MESHX_UVP_OP_RPR_TUNNEL_UP` | Intermediate Node $\rightarrow$ Master | Send provisioning response PDU up to Master |
| `0xC4` | `MESHX_UVP_OP_KR_DISTRIBUTE` | Master $\rightarrow$ Mesh Group | Distribute new Key/Salt (KR Phase 1) |
| `0xC5` | `MESHX_UVP_OP_KR_SWITCHOVER` | Master $\rightarrow$ Mesh Group | Switch primary TX key to new key (KR Phase 2) |
| `0xC6` | `MESHX_UVP_OP_KR_REVOKE_OLD` | Master $\rightarrow$ Mesh Group | Invalidate old key slot (KR Phase 3) |
| `0xC7` | `MESHX_UVP_OP_REDUNDANCY_SYNC` | Master $\rightarrow$ Standby | Synchronize node allocation high-water mark & DevKeys |
| `0xC8` | `MESHX_UVP_OP_INFILL_LEASE_REQ` | Proxy $\rightarrow$ Master | Request JIT contiguous address lease for target element count |
| `0xC9` | `MESHX_UVP_OP_FAST_PROV_REPORT` | Intermediate Proxy $\rightarrow$ Master | Report completed child nodes, DevKey registration & quota status |
| `0xCA` | `MESHX_UVP_OP_KR_KEY_REQ` | Straggler $\rightarrow$ Master | Orphan unicast key pull request |
| `0xCB` | `MESHX_UVP_OP_KR_KEY_CONFIRM` | Straggler $\rightarrow$ Master | Orphan key confirm |
| `0xCC` | `MESHX_UVP_OP_INFILL_LEASE_RSP` | Master $\rightarrow$ Proxy | Grant leased contiguous unicast address from unified pool |
| `0xCD` | `MESHX_UVP_OP_REDUNDANCY_CLAIM` | Master $\rightarrow$ Candidate Nodes | Invite candidate to promote to Standby ($N+1$ cascading) |
| `0xCE` | `MESHX_UVP_OP_REHEAL_BEACON_REQ` | Evicted Node $\rightarrow$ Mesh/Master | DevKey-signed re-healing beacon request |
| `0xCF` | `MESHX_UVP_OP_REHEAL_ADMIT_RSP` | Master $\rightarrow$ Evicted Node | DevKey-encrypted re-admit response with active NetKey & Epoch |
| `0xD0` | `MESHX_UVP_OP_REDUNDANCY_CLAIM_ACK` | Candidate Node $\rightarrow$ Master | Candidate accepts Standby duty with local UUID |

---

## 5. Packed C Structures

```c
#pragma pack(push, 1)

typedef struct {
    uint16_t     proxy_node_addr;   /* 0xFFFF for all nodes, or specific relay address */
    uint8_t      duration_sec;      /* Scan duration (seconds) */
    meshx_uuid_t uuid_filter;       /* All 0s = match any unprov node */
} mxcp_cmd_uvp_rpr_scan_t;

typedef struct {
    uint16_t     proxy_node_addr;   /* Address of relay node in range of unprov device */
    meshx_uuid_t target_uuid;       /* Target unprovisioned node UUID */
    uint8_t      pdu_len;           /* Length of raw provisioning PDU */
    uint8_t      pdu_data[64];      /* Provisioning PDU bytes */
} mxcp_cmd_uvp_rpr_tunnel_t;

typedef struct {
    uint16_t     proxy_node_addr;   /* Node that heard the beacon */
    meshx_uuid_t uuid;              /* Discovered device UUID */
    uint16_t     oob_info;          /* BLE Mesh OOB information flags */
    int8_t       rssi;              /* Measured RSSI */
} mxcp_evt_uvp_rpr_beacon_ind_t;

typedef struct {
    uint16_t     proxy_node_addr;   /* Relay that forwarded the response */
    meshx_uuid_t uuid;              /* Originating node UUID */
    uint8_t      pdu_len;           /* Length of response PDU */
    uint8_t      pdu_data[64];      /* Provisioning PDU payload */
} mxcp_evt_uvp_rpr_tunnel_recv_t;

typedef struct {
    uint8_t      phase;             /* 1 = Distribute, 2 = Switchover, 3 = Revoke */
    uint32_t     target_epoch;      /* Target Key Rotation Epoch */
    uint8_t      new_netkey[16];    /* Key data (Phase 1 only, all 0s for Phase 2/3) */
} mxcp_cmd_uvp_key_rotate_t;

typedef struct {
    uint8_t      phase;             /* Active phase completed */
    uint32_t     epoch;             /* Active Epoch number */
    uint8_t      status;            /* 0x00 = In Progress, 0x01 = Overlap Active, 0x02 = Success */
} mxcp_evt_uvp_key_rot_phase_t;

typedef struct {
    uint8_t      role;              /* 0x01 = Master, 0x02 = Standby, 0x03 = Candidate */
    uint16_t     pool_start;        /* Unified default: 0x0002 (or custom lower bound) */
    uint16_t     pool_end;          /* Unified default: 0x7FFF (or custom upper bound) */
    uint16_t     peer_addr;         /* Peer provisioner address */
} mxcp_cmd_uvp_redundancy_role_t;

typedef struct {
    uint8_t      current_role;      /* Active role */
    uint8_t      peer_alive;        /* 1 = Heartbeat OK, 0 = Missing */
    uint16_t     next_free_addr;    /* Local allocation high-water mark */
    uint32_t     sync_epoch;        /* Redundancy sync counter */
} mxcp_evt_uvp_redundancy_status_t;

typedef struct {
    meshx_uuid_t uuid_mask;          /* Matching prefix for bulk enrollment */
    uint16_t     target_count;       /* Total cluster target count */
    uint16_t     root_base_addr;     /* Starting unicast address (default 0x0002) */
    uint8_t      k_factor;           /* 0 = Uncapped (K=inf, natural silence), >0 = Bounded quota (e.g. 2, 4) */
} mxcp_cmd_uvp_fast_prov_start_t;

typedef struct {
    uint8_t      completed_count;   /* Nodes provisioned so far */
    uint8_t      failed_count;      /* Nodes that failed OOB/link */
    uint8_t      progress_percent;  /* 0 - 100% */
    uint8_t      status;            /* 0 = Scanning, 1 = Onboarding, 2 = Complete */
} mxcp_evt_uvp_fast_prov_progress_t;

typedef struct {
    uint16_t     proxy_node_addr;   /* Requesting proxy unicast address */
    meshx_uuid_t target_uuid;       /* Target unprovisioned node UUID */
    uint8_t      num_elements;      /* Declared target elements from Capabilities */
} meshx_uvp_infill_lease_req_t;

typedef struct {
    meshx_uuid_t target_uuid;       /* Target unprovisioned node UUID */
    uint16_t     leased_addr;       /* Assigned starting unicast address */
    uint8_t      status;            /* 0x00 = OK, 0x01 = ALREADY_ASSIGNED, 0x02 = EXHAUSTED, 0x03 = CONGESTION_RETRY */
} meshx_uvp_infill_lease_rsp_t;

typedef struct {
    uint32_t     claim_epoch;       /* Master claim epoch */
    uint16_t     master_addr;       /* Master unicast address */
    uint16_t     next_free_addr;    /* Monotonic allocation high-water mark */
    uint16_t     reserved;          /* Reserved alignment padding */
} meshx_uvp_redundancy_claim_t;

typedef struct {
    meshx_uuid_t candidate_uuid;    /* Candidate node UUID accepting Standby */
    uint16_t     candidate_addr;    /* Candidate unicast address */
} meshx_uvp_redundancy_claim_ack_t;

#pragma pack(pop)
```
