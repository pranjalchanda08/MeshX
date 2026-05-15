# Requirements — Memory Reduction (ELF-based TLV Logging)

This document captures the requirements for a high-efficiency logging system that stores format strings in the ELF file but excludes them from the device's program memory.

| ID | Title | Description | Acceptance Criteria | Priority |
|----|-------|-------------|---------------------|----------|
| REQ-001 | Non-loadable Log Section | Store all logging format strings and `__FILE__` metadata in a custom ELF section that is not loaded into flash memory. | Strings appear in the `.elf` but NOT in the generated `.bin` or `.hex`. | P0 |
| REQ-002 | TLV Registration | Replace `meshx_log_printf` with a macro that captures the string pointer (ELF address), timestamp, file/line info, and raw arguments into a TLV packet. | Logs must capture metadata and variadic arguments without local formatting. | P0 |
| REQ-003 | TLV UART Transmission | Transmit encoded TLV packets over UART. | UART output should be binary/encoded data, not human-readable strings. | P0 |
| REQ-004 | Host-side Log Decoder | Develop a Python tool to read UART, lookup addresses in the ELF, and format the output for humans. | The tool must correctly match the address to the string in the ELF, verify the XOR parity, and apply `stdio`-like formatting. | P0 |
| REQ-005 | Legacy Fallback | Allow for a compile-time toggle to revert to standard `printf` logging if needed. | System can be compiled with or without ELF-based logging. | P1 |
| REQ-006 | Packet Integrity (XOR Parity) | Each TLV packet must include an XOR-based parity byte to ensure transmission reliability. | Host tool must discard packets with invalid parity. | P0 |
| REQ-007 | L0 Format Matrix | L0 validation must cover all standard `printf` format specifiers (`%d`, `%s`, `%f`, `%p`, `%x`, etc.) to ensure host-side reconstruction accuracy. | All tested format types must be correctly rendered by the host decoder. | P0 |

## Architecture Concept
1. **Compile-time**: Linker script puts specific strings in a `NOLOAD` section.
2. **Runtime**: `MESHX_LOG(fmt, ...)` sends `[HEADER][TIMESTAMP][ELF_ADDR][FILE_ADDR][LINE][ARGS_LEN][ARGS_DATA][CRC]`.
3. **Host**: `decoder.py --elf build/meshx.elf` reads UART and prints logs.

## User Notes
- strings are part of the main ELF in a const space which loads to the flash.
- I have a idea where I would like to save all the logging strings at a non-loadable ELF space out of the memory structure region.
- When the Log is registered in the logging buffer I shall be registered in a TLV format that specifies the pointer value of the string in the ELF + relative printf arguments to process the strings.
- These TLV encoded strings are sent out of the Log UART channel.
- On the Host we need to write a python tool that reads the UART port and process the encoded TLV right from the relative .elf file.
