/*
 * Copyright (c) 2024 - 2026 MeshX
 * SPDX-License-Identifier: Apache-2.0
 *
 * Linker script snippet for MeshX on Zephyr.
 * Places packed log string dictionary (.meshx_log_str.*) in a NO-LOAD (INFO) section
 * so that log format strings are preserved in the ELF for off-target decoding
 * without consuming any flash (.text / .rodata) or RAM on the target device.
 */

.meshx_log_str 0 (INFO) :
{
    . = ALIGN(4);
    _meshx_log_str_start = .;
    KEEP(*(SORT(.meshx_log_str*)))
    KEEP(*(SORT(.meshx_log_str.*)))
    _meshx_log_str_end = .;
}
