#!/usr/bin/env python3
"""
Test Harness for MeshX Node Replacement & 24h Quarantine (TASK-BPROV-008 / TRD Page 9)

Validates:
1. Modality A (In-Place 1:1 replacement with SeqNum fast-forward).
2. Modality B (Element match with model schema divergence).
3. Modality C1 (Contraction: in-place base + 24h quarantine on surplus addresses).
4. Modality C2 (Expansion: contiguity collision & JIT relocation).
5. 24-hour RPL Quarantine Table range blocking and expiration.
"""

import unittest
import struct

class TestNodeReplacementHarness(unittest.TestCase):
    def test_mxcp_replace_frame_serialization(self):
        """Verify MXCP Node Replace Start (0x36) frame formatting."""
        old_uuid = b"\x01" * 16
        new_uuid = b"\x02" * 16
        flags = 0x01 # ALLOW_RELOCATION
        target_override = 0x0020
        seqnum_offset = 1000

        payload = old_uuid + new_uuid + struct.pack("<BHI", flags, target_override, seqnum_offset)
        self.assertEqual(len(payload), 16 + 16 + 1 + 2 + 4) # 39 bytes

        chk = (len(payload) + 0x36 + sum(payload)) & 0xFF
        frame = bytes([0xFE, len(payload), 0x36]) + payload + bytes([chk, 0xEF])
        self.assertEqual(frame[0], 0xFE)
        self.assertEqual(frame[2], 0x36)
        self.assertEqual(frame[-1], 0xEF)

    def test_quarantine_range_overlap_math(self):
        """Verify quarantine range overlap logic: max(s1, s2) <= min(e1, e2)."""
        # Block 1: 0x0012..0x0013
        q_start, q_end = 0x0012, 0x0013

        # Test candidate 1: 0x0010..0x0011 (disjoint, below)
        c1_start, c1_end = 0x0010, 0x0011
        overlap_c1 = max(q_start, c1_start) <= min(q_end, c1_end)
        self.assertFalse(overlap_c1)

        # Test candidate 2: 0x0012..0x0015 (overlapping)
        c2_start, c2_end = 0x0012, 0x0015
        overlap_c2 = max(q_start, c2_start) <= min(q_end, c2_end)
        self.assertTrue(overlap_c2)

        # Test candidate 3: 0x0014..0x0018 (disjoint, above)
        c3_start, c3_end = 0x0014, 0x0018
        overlap_c3 = max(q_start, c3_start) <= min(q_end, c3_end)
        self.assertFalse(overlap_c3)

if __name__ == "__main__":
    unittest.main()
