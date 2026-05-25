# Requirements

## REQ-001
- **Title:** Unify MXCP and App API Structures
- **Description:** Consolidate the structures used by MXCP and `meshx_api` (App APIs) for sending data and control events, ensuring both utilize a unified interface instead of disparate structures.
- **Acceptance Criteria:** Both MXCP and `meshx_api` use the same foundational data structures and API contracts for event handling and transmission.
- **Priority:** P0

## REQ-002
- **Title:** Separate Data and Control Paths (Single Source of Truth)
- **Description:** Divide the communication paths into distinct Data and Control paths using a Single Source of Truth for both the MXCP and internal App layers. 
  - **Single Enums:** `meshx_api.h` defines one set of Event IDs for Data (`meshx_msg_data_id_t`) and Control (`meshx_msg_ctrl_id_t`), eliminating separate MXCP IDs.
  - **Single Payloads:** `meshx_api.h` defines packed structures (`meshx_msg_data_t` and `meshx_msg_ctrl_t`) that are used directly by internal Apps and serialized directly over the wire by MXCP.
  - **Unified APIs:** `meshx_api_data_send()` and `meshx_api_ctrl_send()` accept these unified structs and distribute them to both internal App callbacks and the external MXCP host.
- **Acceptance Criteria:** `meshx_mxcp.h` contains no duplicate payload structs or event IDs. Developers use exactly one API schema to interface with MeshX regardless of whether they are writing an internal App or an external Host application.
- **Priority:** P0

## REQ-003
- **Title:** Enhance `meshx_api.h` Structures
- **Description:** Refactor and improve the APIs in `meshx_api.h` to provide better, more logically structured interfaces that align with the newly unified data and control paths.
- **Acceptance Criteria:** `meshx_api.h` provides explicitly structured functions/structs for the data and control paths.
- **Priority:** P1

## REQ-004
- **Title:** Manage GPIO APIs for Production
- **Description:** Retain the current GPIO APIs for testing purposes but ensure they are explicitly disabled or conditionally compiled out in production builds.
- **Acceptance Criteria:** A configuration flag or macro correctly disables GPIO APIs in production builds while allowing them during testing.
- **Priority:** P2

## REQ-005
- **Title:** Host SDK for Unified APIs
- **Description:** Develop an SDK (supporting Python and/or JavaScript) that implements the unified Data and Control path APIs. This SDK must allow external UI hosts (like the Web Console) or testing scripts to encode and decode the packed `meshx_msg_data_t` and `meshx_msg_ctrl_t` structures effortlessly over the MXCP serial link.
- **Acceptance Criteria:** A Python/JS SDK library is available that abstracts the byte-level packing/unpacking and provides high-level function calls (e.g., `send_data_msg()`, `send_ctrl_msg()`) mirroring the C API.
- **Priority:** P1
