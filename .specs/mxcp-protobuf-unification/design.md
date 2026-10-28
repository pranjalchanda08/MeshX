# Technical Design: Unified MXCP Protocol Buffers Migration

## 1. Architecture & Protocol Layering

```
+-------------------------------------------------------------+
|                      Application Logic                      |
| Firmware Handlers (C)             MXCTF Test Steps (Python) |
+-------------------------------------------------------------+
                              |
+-------------------------------------------------------------+
|                 Serialization / Schema Layer                |
| Nanopb (mxcp.pb.h / .c)        google.protobuf (mxcp_pb2.py)|
|              Source of Truth: mxcp.proto                    |
+-------------------------------------------------------------+
                              |
+-------------------------------------------------------------+
|                      Framing Envelope                       |
|   [SOF: 0xFE] [LEN: 1B] [TYPE: 1B] [PROTO PAYLOAD] [CHK] [EOF]
+-------------------------------------------------------------+
                              |
+-------------------------------------------------------------+
|                     Physical Transport                      |
|                  UART / USB CDC Serial Port                 |
+-------------------------------------------------------------+
```

---

## 2. Shared Protocol Definition (`tools/scripts/proto/mxcp.proto`)

```protobuf
syntax = "proto3";

package meshx.mxcp;

enum ProvisioningState {
    PROV_STATE_UNPROVISIONED = 0;
    PROV_STATE_BEACONING     = 1;
    PROV_STATE_PROVISIONING  = 2;
    PROV_STATE_PROVISIONED   = 3;
}

// ------------------------------------------------------------------
// Device Information Messages (CMD 0x75 / EVT 0xF5)
// ------------------------------------------------------------------
message DeviceInfoReq {
    // Empty request payload
}

message DeviceInfoRsp {
    bytes uuid          = 1; // 16-byte Device UUID
    bytes mac_addr      = 2; // 6-byte Hardware MAC
    uint32 cid          = 3; // Company ID (e.g. 0x7908)
    uint32 pid          = 4; // Product ID (e.g. 0x0009)
    ProvisioningState prov_status = 5;
    uint32 node_addr    = 6; // Assigned primary unicast address
    uint32 link_epoch   = 7; // Monotonic NVS link table epoch
    uint32 element_count= 8; // Number of active elements
    string fw_version   = 9; // Firmware build version string
}

// ------------------------------------------------------------------
// Link Table Messages (CMD 0x73 / EVT 0xF3)
// ------------------------------------------------------------------
message LinkTableReq {
    // Empty request payload
}

message LinkEntry {
    uint32 local_element_idx    = 1;
    uint32 bound_variant        = 2;
    uint32 target_unicast_addr  = 3;
    uint32 target_group_addr    = 4;
    uint32 func_id              = 5;
    bool   active               = 6;
}

message LinkTableRsp {
    uint32 magic        = 1; // 0x4C4E4B53 ('LNKS')
    uint32 version      = 2; // 1
    uint32 link_epoch   = 3;
    uint32 link_count   = 4;
    repeated LinkEntry links = 5;
}

// ------------------------------------------------------------------
// Composition Data Messages (CMD 0x03 / EVT 0x86)
// ------------------------------------------------------------------
message CompositionReq {
    // Empty request payload
}

message ElementDescriptor {
    uint32 element_id   = 1;
    uint32 variant      = 2;
    uint32 type         = 3;
    string name         = 4;
    repeated uint32 funcs = 5;
}

message CompositionRsp {
    uint32 status       = 1;
    uint32 cid          = 2;
    uint32 pid          = 3;
    uint32 element_count= 4;
    repeated ElementDescriptor elements = 5;
}
```

---

## 3. Nanopb Generator Options (`tools/scripts/proto/mxcp.options`)

Ensures zero heap allocations in embedded firmware:
```properties
meshx.mxcp.DeviceInfoRsp.uuid            max_size:16, fixed_length:true
meshx.mxcp.DeviceInfoRsp.mac_addr        max_size:6,  fixed_length:true
meshx.mxcp.DeviceInfoRsp.fw_version      max_size:32

meshx.mxcp.LinkTableRsp.links            max_count:16
meshx.mxcp.CompositionRsp.elements       max_count:16
meshx.mxcp.ElementDescriptor.name        max_size:32
meshx.mxcp.ElementDescriptor.funcs       max_count:8
```

---

## 4. Firmware Implementation Pattern

### Encoding (Firmware -> Host)
```c
static meshx_err_t mxcp_cmd_fn_get_dev_info(const uint8_t *payload, uint8_t len)
{
    (void)payload;
    (void)len;
    uint8_t buffer[MXCP_PAYLOAD_MAX_SIZE];
    meshx_mxcp_DeviceInfoRsp rsp = meshx_mxcp_DeviceInfoRsp_init_zero;

    meshx_uuid_t uuid;
    if (meshx_get_device_uuid(&uuid) == MESHX_SUCCESS) {
        memcpy(rsp.uuid.bytes, uuid, 16);
        rsp.uuid.size = 16;
    }
    meshx_platform_get_mac(rsp.mac_addr.bytes);
    rsp.mac_addr.size = 6;
    rsp.cid = meshx_get_cid();
    rsp.pid = meshx_get_pid();
    rsp.node_addr = meshx_get_node_addr();
    rsp.prov_status = (rsp.node_addr != MESHX_ADDR_UNASSIGNED) ?
        meshx_mxcp_ProvisioningState_PROV_STATE_PROVISIONED :
        meshx_mxcp_ProvisioningState_PROV_STATE_UNPROVISIONED;
    rsp.link_epoch = meshx_nvs_get_link_epoch();
    rsp.element_count = meshx_comp_get_element_count();

    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    if (!pb_encode(&stream, meshx_mxcp_DeviceInfoRsp_fields, &rsp)) {
        return MESHX_FAIL;
    }

    return mxcp_send_event(MXCP_EVT_DEV_INFO_RSP, buffer, (uint8_t)stream.bytes_written);
}
```

---

## 5. MXCTF Implementation Pattern

### Declarative Schema with Proto Unpacker
```python
from capi.port.target.proto import mxcp_pb2

def _unpack_proto_dev_info(msg_type: int, payload: bytes) -> Dict[str, Any]:
    msg = mxcp_pb2.DeviceInfoRsp()
    msg.ParseFromString(payload)
    return {
        "uuid": msg.uuid.hex().upper(),
        "mac_addr": ":".join(f"{b:02X}" for b in msg.mac_addr),
        "cid": f"0x{msg.cid:04X}",
        "pid": f"0x{msg.pid:04X}",
        "prov_status": "provisioned" if msg.prov_status == mxcp_pb2.PROV_STATE_PROVISIONED else "unprovisioned",
        "node_addr": msg.node_addr,
        "link_epoch": msg.link_epoch,
        "element_count": msg.element_count,
        "fw_version": msg.fw_version,
    }
```

