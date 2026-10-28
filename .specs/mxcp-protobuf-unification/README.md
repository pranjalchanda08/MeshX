# Specification: Unified MXCP Protocol Buffers Migration

**Directory:** `.specs/mxcp-protobuf-unification/`
**Subsystems:** `MeshX Firmware` (`d:\MeshX`), `MXCTF Testbed Framework` (`D:\MXCTF`)
**Status:** DRAFT / PROPOSED

---

## 1. Overview & Problem Statement

Currently, the MeshX Command Protocol (MXCP) uses manual C byte packing (`#pragma pack(push, 1)`) on the firmware side and manual Python `struct` strings (`"<16s6sHHBHIB"`, `"<IBHBB"`, etc.) in the MXCTF test framework (`D:\MXCTF`).

This manual dual-definition model leads to:
1. **Structural Desynchronization:** Any change in field order, integer width (e.g. `uint16_t` vs `uint32_t`), or struct padding breaks wire compatibility silently without build-time detection.
2. **Hardcoded Fallback Stubs:** When firmware lacks a field or command, the test framework introduces brittle fallback dictionaries (e.g., hardcoded MAC `00:00:00:00:00:00` and UUID `79080009000000000000000000000000`).
3. **High Maintenance Overhead:** Two separate implementations of serialization logic must be written and maintained for every new command or event.

### Solution
Unify MXCP payload serialization using **Protocol Buffers (Protobuf)** via **Nanopb** on firmware and standard `google.protobuf` on Python:
* **Single Source of Truth:** A unified `.proto` schema file shared between firmware and test framework.
* **Firmware:** Leverages the existing Nanopb toolchain (`third_party/nanopb` and `nanopb_generator.py`) already integrated into MeshX.
* **MXCTF:** Generates standard Python classes (`mxcp_pb2.py`) eliminating manual `struct.unpack` strings.
* **Lightweight Transport Envelope:** Retain the proven binary frame envelope (`[SOF][LEN][TYPE][PAYLOAD][CHK][EOF]`) with fast $O(1)$ opcode dispatch.

