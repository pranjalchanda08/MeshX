# Tasks — ELF-based TLV Logging

This document breaks down the implementation of the memory-reduction logging feature into discrete tasks.

## Wave 1: Device-side Infrastructure
| Task ID | Title | Description | Linked Reqs | Complexity | Dependencies | Wave |
|---------|-------|-------------|-------------|------------|--------------|------|
| TASK-001 | Linker Config Update | Add `.meshx_log_str` to `meshx.lf` and ensure it is treated as NOLOAD. | REQ-001 | S | None | 1 |
| TASK-002 | TLV Buffer Management | Implement binary buffer management for UART transmission with `0xDEAD` sync word. | REQ-003 | M | None | 1 |

## Wave 2: Core Implementation
| Task ID | Title | Description | Linked Reqs | Complexity | Dependencies | Wave |
|---------|-------|-------------|-------------|------------|--------------|------|
| TASK-003 | TLV Packer Function | Implement `meshx_log_tlv_send` that packs addresses, timestamp, file/line, and variadic args. | REQ-002 | L | TASK-002 | 2 |
| TASK-004 | XOR Parity Integration | Implement XOR-based checksum and append to each TLV packet. | REQ-006 | S | TASK-003 | 2 |
| TASK-005 | Enhanced MESHX_LOG Macro | Implement the macro with static trapping for `fmt` and `__FILE__`. | REQ-002 | M | TASK-003 | 2 |

## Wave 3: Host-side Tooling
| Task ID | Title | Description | Linked Reqs | Complexity | Dependencies | Wave |
|---------|-------|-------------|-------------|------------|--------------|------|
| TASK-006 | host_decoder.py | (NEW) Implement Python script for ELF parsing, address lookup, and stdio formatting. | REQ-004 | L | None | 3 |
| TASK-007 | meshx.py Integration | Add `-R` argument to `meshx.py` to pipe UART data through `host_decoder.py`. | REQ-004 | M | TASK-006 | 3 |

## Wave 4: Validation & Migration
| Task ID | Title | Description | Linked Reqs | Complexity | Dependencies | Wave |
|---------|-------|-------------|-------------|------------|--------------|------|
| TASK-008 | Logging Backend Switch | Update `meshx_log.c/h` to remove local formatting and use the TLV backend. | REQ-005 | M | Wave 2, 3 | 4 |
| TASK-009 | Size Verification | Compare binary size before and after to verify flash savings. | REQ-001 | S | TASK-008 | 4 |
