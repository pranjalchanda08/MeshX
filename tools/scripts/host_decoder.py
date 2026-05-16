#!/usr/bin/env python3

import os
import sys
import serial
import struct
import argparse
import re
import threading
import time
from elftools.elf.elffile import ELFFile

# ANSI Color Codes
COLORS = {
    'RESET': '\033[0m',
    'RED': '\033[31m',
    'GREEN': '\033[32m',
    'YELLOW': '\033[33m',
    'BLUE': '\033[34m',
    'MAGENTA': '\033[35m',
    'CYAN': '\033[36m',
    'BOLD': '\033[1m'
}

LEVEL_MAP = {
    0: ('N', COLORS['BOLD']),    # NONE
    1: ('E', COLORS['RED']),     # ERROR
    2: ('W', COLORS['YELLOW']),  # WARN
    3: ('I', COLORS['GREEN']),   # INFO
    4: ('D', COLORS['CYAN']),    # DEBUG
    5: ('V', COLORS['RESET'])    # VERBOSE
}

BANNER = r"""
*********************************************************************************************************************
* MMMMMMMM               MMMMMMMM                                     hhhhhhh                 XXXXXXX       XXXXXXX *
* M:::::::M             M:::::::M                                     h:::::h                 X:::::X       X:::::X *
* M::::::::M           M::::::::M                                     h:::::h                 X:::::X       X:::::X *
* M:::::::::M         M:::::::::M                                     h:::::h                 X::::::X      X:::::X *
* M::::::::::M       M::::::::::M    eeeeeeeeeeee        ssssssssss   h:::: hhhhhh            XX:::::X     X:::::XX *
* M:::::::::::M     M:::::::::::M  ee::::::::::::ee    ss::::::::::s  h::::::::::hhh            X:::::X   X:::::X   *
* M:::::::M::::M   M::::M:::::::M e::::::eeeee:::::eess:::::::::::::s h::::::::::::::hh           X:::::X:::::X     *
* M::::::M M::::M M::::M M::::::Me::::::e     e:::::es::::::ssss:::::sh:::::::hhh::::::h           X:::::::::X      *
* M::::::M  M::::M::::M  M::::::Me:::::::eeeee::::::e s:::::s  ssssss h::::::h   h::::::h          X:::::::::X      *
* M::::::M   M:::::::M   M::::::Me:::::::::::::::::e    s::::::s      h:::::h     h:::::h         X:::::X:::::X     *
* M::::::M    M:::::M    M::::::Me::::::eeeeeeeeeee        s::::::s   h:::::h     h:::::h        X:::::X X:::::X    *
* M::::::M     MMMMM     M::::::Me:::::::e           ssssss   s:::::s h:::::h     h:::::h     XXX:::::X   X:::::XXX *
* M::::::M               M::::::Me::::::::e          s:::::ssss::::::sh:::::h     h:::::h     X::::::X     X::::::X *
* M::::::M               M::::::M e::::::::eeeeeeee  s::::::::::::::s h:::::h     h:::::h     X:::::X       X:::::X *
* M::::::M               M::::::M  ee:::::::::::::e   s:::::::::::ss  h:::::h     h:::::h     X:::::X       X:::::X *
* MMMMMMMM               MMMMMMMM    eeeeeeeeeeeeee    sssssssssss    hhhhhhh     hhhhhhh     XXXXXXX       XXXXXXX *
*********************************************************************************************************************
"""

class MeshXLogDecoder:
    SYNC_WORD = 0xDEAD

    def __init__(self, elf_path):
        self.elf_path = elf_path
        self.strings_cache = {}
        self._load_elf_strings()

    def _load_elf_strings(self):
        """Pre-cache strings from the .meshx_log_str section if possible,
        otherwise we will resolve addresses dynamically from the whole ELF."""
        if not os.path.exists(self.elf_path):
            print(f"Warning: ELF file {self.elf_path} not found. String resolution will fail.")
            return

        with open(self.elf_path, 'rb') as f:
            elffile = ELFFile(f)
            # We don't necessarily need to pre-cache everything,
            # but we need a way to read strings at specific addresses.
            self.elffile = elffile
            self.image_data = f.read() # Load entire ELF into memory for fast access

    def resolve_string(self, addr):
        if addr in self.strings_cache:
            return self.strings_cache[addr]

        # Find which segment contains this address
        with open(self.elf_path, 'rb') as f:
            elffile = ELFFile(f)
            for segment in elffile.iter_segments():
                if segment['p_vaddr'] <= addr < segment['p_vaddr'] + segment['p_filesz']:
                    offset = segment['p_offset'] + (addr - segment['p_vaddr'])
                    f.seek(offset)
                    s = b""
                    while True:
                        c = f.read(1)
                        if c == b'\0' or not c:
                            break
                        s += c
                    res = s.decode('utf-8', errors='replace')
                    self.strings_cache[addr] = res
                    return res
        return f"<addr: 0x{addr:08x}>"

    def calculate_parity(self, data):
        parity = 0
        for b in data:
            parity ^= b
        return parity

    def decode_packet(self, raw_data):
        # raw_data includes sync(2), len(1), and payload(len)
        if len(raw_data) < 3:
            return None

        sync, length = struct.unpack("<HB", raw_data[:3])
        if sync != self.SYNC_WORD:
            return None

        payload = raw_data[3:]
        # Verify parity (last byte of payload)
        calc_parity = self.calculate_parity(raw_data[:-1])
        if calc_parity != raw_data[-1]:
            return f"Parity mismatch: calc=0x{calc_parity:02x}, got=0x{raw_data[-1]:02x}"

        # Header: level(1), module(1), timestamp(4), fmt(4), file(4), line(2) = 16 bytes
        header = struct.unpack("<BBIIIH", payload[:16])
        level, module, ts, fmt_addr, file_addr, line = header

        # Args: up to 16 words (64 bytes)
        args_raw = struct.unpack("<IIIIIIIIIIIIIIII", payload[16:16+64])

        # Inlined string: 32 bytes
        inline_str_raw = payload[16+64:16+64+32]
        inline_str = inline_str_raw.split(b'\0')[0].decode('utf-8', errors='replace')

        fmt_str = self.resolve_string(fmt_addr)
        file_str = self.resolve_string(file_addr)

        # Basic cleanup of file path
        file_str = os.path.basename(file_str)

        # Format the string with arguments
        # We need to be careful with %s as those are addresses that need resolution
        try:
            # Count placeholders to know how many args to use
            # Improved regex to handle modifiers and more types
            placeholders = re.findall(r'%(?:[-+ #0]*)(?:[0-9]*|\*)(?:\.(?:[0-9]*|\*))?(?:[lhjzL]*)([diuoxXfcspeEgG])', fmt_str)
            final_args = []
            arg_idx = 0
            for i, p in enumerate(placeholders):
                if arg_idx >= len(args_raw):
                    break
                val = args_raw[arg_idx]
                if p == 's':
                    resolved = self.resolve_string(val)
                    if resolved.startswith("<addr:") and inline_str:
                        final_args.append(inline_str)
                    else:
                        final_args.append(resolved)
                else:
                    # In Python, we can just pass the value
                    final_args.append(val)
                arg_idx += 1

            # Remove C-specific length modifiers for Python compatibility
            py_fmt = re.sub(r'(%[-+ #0]*[0-9.]*)[lhjzL]+([diuoxXfcspeEgG])', r'\1\2', fmt_str)

            # Apply coloring and tagging
            lvl_tag, lvl_color = LEVEL_MAP.get(level, ('?', COLORS['RESET']))

            log_content = py_fmt % tuple(final_args)
            return f"{lvl_color}[{lvl_tag}][{ts:08d}][{module:03x}][{file_str:>25}:{line:04d}]\t{log_content}{COLORS['RESET']}"
        except Exception as e:
            return f"[{ts:08d}][{module:03x}][{file_str:>25}:{line:04d}]\tDecode Error: {e} | Raw: {fmt_str} | Args: {args_raw}"

def main():
    try:
        parser = argparse.ArgumentParser(description="MeshX TLV Log Decoder")
        parser.add_argument("--elf", required=True, help="Path to project ELF file")
        parser.add_argument("--port", help="Serial port to read from")
        parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
        parser.add_argument("--file", help="Binary log file to decode (instead of serial)")

        args = parser.parse_args()

        decoder = MeshXLogDecoder(args.elf)

        if args.file:
            decoder = MeshXLogDecoder(args.elf)
            with open(args.file, 'rb') as f:
                stream = f.read()
                i = 0
                while i < len(stream) - 2:
                    if stream[i:i+2] == b'\xAD\xDE':
                        length = stream[i+2]
                        packet_len = 3 + length
                        if i + packet_len <= len(stream):
                            res = decoder.decode_packet(stream[i:i+packet_len])
                            if res:
                                print(res)
                            i += packet_len
                            continue
                    i += 1
        elif args.port:
            print(f"Connecting to {args.port} at {args.baud}...")
            print(COLORS['CYAN'] + BANNER + COLORS['RESET'])

            try:
                ser = serial.Serial(args.port, args.baud, timeout=0.1)
            except Exception as e:
                print(f"Error opening serial port: {e}")
                return

            decoder = MeshXLogDecoder(args.elf)

            def serial_writer():
                while True:
                    try:
                        cmd = input(f"{COLORS['BOLD']}MeshX> {COLORS['RESET']}")
                        if cmd.strip():
                            ser.write((cmd + '\n').encode())
                    except (EOFError, KeyboardInterrupt):
                        break
                    except Exception as e:
                        print(f"Writer error: {e}")
                        break

            writer_thread = threading.Thread(target=serial_writer, daemon=True)
            writer_thread.start()

            buffer = b""
            while True:
                try:
                    data = ser.read(1024)
                    if data:
                        buffer += data
                        while len(buffer) >= 3:
                            sync_idx = buffer.find(b'\xAD\xDE')

                            if sync_idx == -1:
                                # No sync word, just treat as text if possible
                                text = buffer.decode('utf-8', errors='replace')
                                if text:
                                    print(f"\r\x1b[K{text}", end='', flush=True)
                                    print(f"{COLORS['BOLD']}MeshX> {COLORS['RESET']}", end='', flush=True)
                                buffer = b""
                                break

                            if sync_idx > 0:
                                # Print everything before sync as text
                                text = buffer[:sync_idx].decode('utf-8', errors='replace')
                                if text:
                                    print(f"\r\x1b[K{text}", end='', flush=True)
                                    print(f"{COLORS['BOLD']}MeshX> {COLORS['RESET']}", end='', flush=True)
                                buffer = buffer[sync_idx:]

                            if len(buffer) < 3:
                                break

                            length = buffer[2]
                            packet_len = 3 + length
                            if len(buffer) >= packet_len:
                                pkt_data = buffer[:packet_len]
                                res = decoder.decode_packet(pkt_data)
                                if res:
                                    print(f"\r\x1b[K{res}")
                                    print(f"{COLORS['BOLD']}MeshX> {COLORS['RESET']}", end='', flush=True)
                                    buffer = buffer[packet_len:]
                                else:
                                    # Invalid packet, skip sync word
                                    buffer = buffer[2:]
                            else:
                                # Wait for more data
                                break
                    time.sleep(0.01)
                except serial.SerialException as e:
                    print(f"Serial error: {e}")
                    break
                except Exception as e:
                    print(f"Error: {e}")
                    break
        else:
            print("Please specify --port or --file")
    except KeyboardInterrupt:
        print("\nExiting MeshX Shell...")
        os._exit(0)

if __name__ == "__main__":
    main()
