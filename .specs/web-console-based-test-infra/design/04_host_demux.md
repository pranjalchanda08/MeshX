# Design Page 04: Host-Side Stream Demultiplexing & Self-Healing Parser

This page defines the host-side stream demultiplexing formats, state machine, and sliding-window resynchronization algorithm.

---

## 1. Interleaved Stream Signature Specs [REQ-004]

The unified serial link carries three distinct classes of data multiplexed dynamically in the same byte stream:

### 1.1 Dynamic Binary Log Packet (`0xDEAD` SOF)
Used for high-efficiency firmware logging, replacing heavy standard library formatting strings with direct RAM addresses resolved via ELF tables.
*   **SOF Signature:** `0xDE 0xAD` (2 bytes)
*   **Header fields:** `Length` (2 bytes, Big-Endian), `Format String RAM Address` (4 bytes, Big-Endian)
*   **Argument Data:** Variadic length payload containing printf format arguments.
*   **Verification:** Parity byte check matching `xor(all bytes)`.

### 1.2 Binary MXSP Packet (`0xFE` SOF)
Used to pass BLE Mesh control messages, network topology events, and GPIO telemetry reports.
*   **SOF Signature:** `0xFE` (1 byte)
*   **Header fields:** `Length` (1 byte), `Message Type` (1 byte)
*   **Message Payload:** Dynamic telemetry/command data.
*   **Checksum Verification:** XOR of all fields from `Length` to payload.
*   **EOF Anchor:** `0xEF` (1 byte)

### 1.3 Raw Console Text Output
Any byte sequence that does not successfully pass frame parsing is routed immediately to the diagnostic standard out console.

---

## 2. Sliding-Window Self-Healing Parsing Algorithm [REQ-005]

To prevent stream loss during physical board resets or serial glitches, the parser does not discard the full stream on error. Instead, it utilizes a self-healing sliding-window algorithm:

```python
class StreamDemultiplexer:
    def __init__(self):
        self.buffer = bytearray()

    def feed(self, chunk: bytes, on_log, on_mxsp, on_text):
        self.buffer.extend(chunk)
        
        while len(self.buffer) > 0:
            # 1. Evaluate Dynamic Log Frame Signature (0xDEAD)
            if len(self.buffer) >= 2 and self.buffer[0] == 0xDE and self.buffer[1] == 0xAD:
                if len(self.buffer) < 8:
                    return # Wait for complete header
                
                length = int.from_bytes(self.buffer[2:4], byteorder='big')
                total_packet_len = 2 + 2 + length  # SOF + LenField + FormatAddr + Args + Parity
                
                if len(self.buffer) < total_packet_len:
                    return # Wait for complete payload
                
                # Extract potential packet
                packet = self.buffer[:total_packet_len]
                parity_calc = 0
                for b in packet[:-1]:
                    parity_calc ^= b
                
                if parity_calc == packet[-1]:
                    # Validation success! Extract formats and pass upstream
                    addr = int.from_bytes(packet[4:8], byteorder='big')
                    args = packet[8:-1]
                    on_log(addr, args)
                    del self.buffer[:total_packet_len]
                    continue
                else:
                    # Parity mismatch: discard the fake 0xDE signature, slide window by 1 byte
                    on_text(self.buffer[0])
                    del self.buffer[0]
                    continue
            
            # 2. Evaluate Binary MXSP Frame Signature (0xFE)
            elif self.buffer[0] == 0xFE:
                if len(self.buffer) < 3:
                    return # Wait for Length field
                
                payload_len = self.buffer[1]
                total_packet_len = 1 + 1 + 1 + payload_len + 1 + 1  # SOF + Len + Type + Payload + Checksum + EOF
                
                if len(self.buffer) < total_packet_len:
                    return # Wait for full packet
                
                packet = self.buffer[:total_packet_len]
                
                # Check EOF anchor
                if packet[-1] != 0xEF:
                    # EOF mismatch: slide window by 1 byte
                    on_text(self.buffer[0])
                    del self.buffer[0]
                    continue
                
                # Calculate XOR Checksum
                checksum_calc = 0
                for b in packet[1:-2]: # XOR fields from Length through Payload
                    checksum_calc ^= b
                
                if checksum_calc == packet[-2]:
                    # MXSP Frame Validated! Route upstream
                    msg_type = packet[2]
                    payload = packet[3:-2]
                    on_mxsp(msg_type, payload)
                    del self.buffer[:total_packet_len]
                    continue
                else:
                    # Checksum mismatch: slide window by 1 byte
                    on_text(self.buffer[0])
                    del self.buffer[0]
                    continue
            
            # 3. Fallback Raw Console Text Processing
            else:
                on_text(self.buffer[0])
                del self.buffer[0]
```

---

## 3. Benefits of Sliding Cursor Resynchronization

*   **Zero Resets Required:** If the ESP32 reboots in the middle of transmitting a binary frame, the host-side sliding window discards the partial junk bytes, automatically resynchronizes on the very next `0xDEAD` or `0xFE` boundaries, and resumes error-free parsing immediately.
*   **Lossless Raw Output:** Unaligned bytes are preserved as console characters, ensuring debug logs are never swallowed.
