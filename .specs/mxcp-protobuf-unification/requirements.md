# Requirements: Unified MXCP Protocol Buffers Migration

## REQ-PB-001: Single Source of Truth Schema
* **Priority:** P0
* **Description:** A single `.proto` file (`mxcp.proto`) shall define all structured command payloads (Host → Engine) and event payloads (Engine → Host).
* **Acceptance Criteria:**
  1. No manual byte offsets or `struct.unpack` format strings shall be required in Python test drivers for messages defined in proto.
  2. Nanopb options (`mxcp.options`) shall define fixed static array sizes to prevent any dynamic heap allocation in firmware.

---

## REQ-PB-002: Preservation of Fast Framing Envelope
* **Priority:** P0
* **Description:** The physical framing envelope `[SOF][LEN][TYPE][PAYLOAD][CHK][EOF]` shall be preserved.
* **Acceptance Criteria:**
  1. `SOF` (0xFE) and `EOF` (0xEF) frame delimiters remain identical.
  2. `TYPE` byte (direction bit 7 + opcode ID) is maintained for $O(1)$ linear dispatch table matching prior to proto decoding.
  3. Commands with zero payload require no proto decoding overhead.
  4. CRC16-CCITT or XOR frame checksum integrity checking remains unchanged.

---

## REQ-PB-003: Zero Dynamic Heap Allocation (Nanopb Static Decoding)
* **Priority:** P0
* **Description:** Nanopb decoding and encoding in firmware shall use static stack-allocated buffers.
* **Acceptance Criteria:**
  1. All repeated fields (e.g., links in `LinkTable`, elements in `Composition`) shall specify `max_count` in `.options`.
  2. All string and byte fields (`uuid`, `mac`, `name`) shall specify `max_size` in `.options`.
  3. No `malloc`, `free`, or Nanopb dynamic pointer callbacks shall be utilized.

---

## REQ-PB-004: Dual-Target Code Generation Pipeline
* **Priority:** P0
* **Description:** Build systems in both repositories shall automatically generate bindings from `mxcp.proto`.
* **Acceptance Criteria:**
  1. **MeshX CMake:** Invokes `nanopb_generator.py` during firmware compilation to produce `mxcp.pb.h` and `mxcp.pb.c`.
  2. **MXCTF Setup:** Automatically exports/compiles `mxcp_pb2.py` into `D:\MXCTF\capi\port\target\proto\`.

---

## REQ-PB-005: Phased Protocol Migration
* **Priority:** P1
* **Description:** Migration shall occur in controlled phases to prevent breaking existing automated certification tests.
* **Acceptance Criteria:**
  1. **Phase 1 (Pilot):** Migrate `dev_get_info` (`0x75`) and `link_table_get` (`0x73`).
  2. **Phase 2 (Composition & Topo):** Migrate `dev_get_composition` (`0x03`), `dev_set_composition` (`0x06`), and `virtual_topo` (`0x38`).
  3. **Phase 3 (Provisioning & UVP):** Migrate remaining provisioning and telemetry payloads.

