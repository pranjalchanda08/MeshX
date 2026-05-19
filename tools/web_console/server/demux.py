import sys
import os

class StreamDemultiplexer:
    """
    Sliding-window self-healing stream demultiplexer as specified in Design Page 04.
    Dynamically isolates 0xDEAD binary logs, 0xFE MXSP packets, and raw console text.
    """
    def __init__(self):
        self.buffer = bytearray()

    def feed(self, chunk: bytes, on_log, on_mxsp, on_text):
        if chunk:
            print(f"[DEMUX FEED] Received chunk size {len(chunk)}: {chunk.hex()}", flush=True)
        self.buffer.extend(chunk)
        if len(self.buffer) > 0:
            print(f"[DEMUX BUFFER] Buffer size {len(self.buffer)}: {bytes(self.buffer).hex()}", flush=True)
        
        while len(self.buffer) > 0:
            # 1. Evaluate Dynamic Log Frame Signature (0xAD 0xDE in Little-Endian)
            if len(self.buffer) >= 2 and self.buffer[0] == 0xAD and self.buffer[1] == 0xDE:
                if len(self.buffer) < 3:
                    return # Wait for SOF + Len byte
                
                length = self.buffer[2]
                total_packet_len = 3 + length  # sync(2) + len(1) + payload_and_parity(length)
                
                if len(self.buffer) < total_packet_len:
                    return # Wait for complete payload
                
                # Extract potential packet
                packet = bytes(self.buffer[:total_packet_len])
                parity_calc = 0
                for b in packet[:-1]:
                    parity_calc ^= b
                
                if parity_calc == packet[-1]:
                    # Validation success! Route the entire intact packet upstream
                    on_log(packet)
                    del self.buffer[:total_packet_len]
                    continue
                else:
                    # Parity mismatch: slide window by 1 byte
                    on_text(bytes([self.buffer[0]]))
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
                    on_text(bytes([self.buffer[0]]))
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
                    on_text(bytes([self.buffer[0]]))
                    del self.buffer[0]
                    continue
            
            # 3. Fallback Raw Console Text Processing
            else:
                on_text(bytes([self.buffer[0]]))
                del self.buffer[0]
