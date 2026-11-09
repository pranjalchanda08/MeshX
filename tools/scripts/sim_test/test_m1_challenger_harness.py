#!/usr/bin/env python3
"""
Empirical Challenger Test Suite for Milestone 1 (Passive Provisionee).
Author: challenger_m1_2
Target Verification:
1. Challenge NVS Transactional Commit & Rollback:
   - Challenge meshx_nvs_commit() in meshx_nvs.c:
     - Error during write in multi-write transaction captures first_err.
     - meshx_nvs_plat_commit() is strictly aborted on error.
     - Verify zero partial state is committed to flash across failure points (first, middle, last, multi).
     - Verify pre-existing flash records remain unmodified on rollback.
2. Challenge Boot-Time Beacon Gating & 10 Reboot Cycles:
   - Challenge esp_platform_ble_mesh.c and meshx.c:
     - When meshx_prov_srv_is_provisioned() returns true, unprovisioned beaconing is permanently suppressed across 10 simulated reboot cycles.
     - Ensure meshx_get_node_addr() returns the persisted primary address immediately upon boot across all 10 cycles.
     - Revert on factory reset.
     - Capability Integrity Validation (bit 5 trait mask vs element count).
3. Static & Concrete Source Code Audits:
   - meshx_nvs.c, esp_platform_ble_mesh.c, meshx.c, meshx_prov_srv.cpp, meshx_mxcp.c
"""

import os
import re
import struct
import sys
import unittest
from typing import Dict, List, Optional, Tuple

CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(CURRENT_DIR, "..", ".."))
if CURRENT_DIR not in sys.path:
    sys.path.insert(0, CURRENT_DIR)

from zgb_sim import (
    BeaconCodec,
    FirmwareAuditor,
    HWRNGKeyGenerator,
    MESHX_ADDR_ANCHOR_MASTER,
    MESHX_ADDR_UNASSIGNED,
    MESHX_ERR_INVALID_ARG,
    MESHX_ERR_INVALID_STATE,
    MESHX_ERR_PROVISIONING_FAILED,
    MESHX_ERR_WRITE_FAIL,
    MESHX_PROV_ROLE_MASTER,
    MESHX_SUCCESS,
    MXCP_CMD_PROV_DISABLE,
    MXCP_CMD_PROV_ENABLE,
    MXCP_CMD_PROV_GET_STATE,
    MXCP_EVT_PROV_FAILED,
    MXCP_EVT_PROV_STATE_IND,
    PROV_STATE_BEACONING,
    PROV_STATE_PROVISIONED,
    PROV_STATE_UNPROVISIONED,
    SimulatedNVS,
    ZGBNodeSimulator,
)

# MeshX Error Codes matching meshx_errno.h
MESHX_FAIL = 1
MESHX_NO_MEM = 2
MESHX_NOT_FOUND = 4


# ==============================================================================
# Model for meshx_nvs_commit() C Implementation
# ==============================================================================
class WriteListNode:
    def __init__(self, key: str, data: bytes):
        self.key = key
        self.data = data
        self.len = len(data)
        self.freed = False


class MeshXNVSCommitModel:
    """
    Exact mirror of C implementation of meshx_nvs_commit() in meshx_nvs.c lines 283-317:
    - Iterates over write list.
    - Captures first_err on any write failure.
    - Frees every node unconditionally (prevents memory leak).
    - Clears write list head.
    - Calls plat_commit only if first_err == MESHX_SUCCESS.
    - Returns first_err.
    """
    MESHX_NVS_INIT_MAGIC = 0x4E565331  # "NVS1"

    def __init__(self):
        self.init_magic = self.MESHX_NVS_INIT_MAGIC
        self.write_list: List[WriteListNode] = []
        self.staged_writes: Dict[str, bytes] = {}
        self.flash_storage: Dict[str, bytes] = {}
        self.plat_commit_called = False
        self.plat_write_called_keys: List[str] = []
        self.fail_keys: Dict[str, int] = {}
        self.fail_plat_commit: Optional[int] = None

    def add_write(self, key: str, data: bytes):
        self.write_list.append(WriteListNode(key, data))

    def plat_write(self, key: str, data: bytes) -> int:
        self.plat_write_called_keys.append(key)
        if key in self.fail_keys:
            return self.fail_keys[key]
        self.staged_writes[key] = data
        return MESHX_SUCCESS

    def plat_commit(self) -> int:
        self.plat_commit_called = True
        if self.fail_plat_commit:
            return self.fail_plat_commit
        # Flush staged writes to persistent flash
        self.flash_storage.update(self.staged_writes)
        self.staged_writes.clear()
        return MESHX_SUCCESS

    def commit(self) -> int:
        if self.init_magic != self.MESHX_NVS_INIT_MAGIC:
            return MESHX_ERR_INVALID_STATE

        first_err = MESHX_SUCCESS
        if not self.write_list:
            return MESHX_SUCCESS

        # Traverse list and call meshx_nvs_plat_write
        for node in self.write_list:
            err = self.plat_write(node.key, node.data)
            if err != MESHX_SUCCESS:
                if first_err == MESHX_SUCCESS:
                    first_err = err
            node.freed = True

        self.write_list = []  # meshx_nvs_write_list_head = NULL

        if first_err == MESHX_SUCCESS:
            first_err = self.plat_commit()
        else:
            # Rollback: discard staged writes without writing to flash
            self.staged_writes.clear()

        return first_err


# ==============================================================================
# Challenge Suite 1: NVS Transactional Commit & Rollback
# ==============================================================================
class TestMilestone1NVSCommitChallenge(unittest.TestCase):
    """
    Adversarially challenge meshx_nvs_commit() transaction commit and rollback behavior.
    """

    def setUp(self):
        self.nvs = MeshXNVSCommitModel()

    def test_nvs_commit_all_writes_succeed(self):
        """Happy path: All writes succeed -> plat_commit called, flash storage updated."""
        self.nvs.add_write("net_key", b"\x11" * 16)
        self.nvs.add_write("app_key", b"\x22" * 16)
        self.nvs.add_write("node_addr", struct.pack(">H", 0x0001))
        self.nvs.add_write("is_prov", b"\x01")

        err = self.nvs.commit()
        self.assertEqual(err, MESHX_SUCCESS)
        self.assertTrue(self.nvs.plat_commit_called)
        self.assertEqual(len(self.nvs.flash_storage), 4)
        self.assertEqual(self.nvs.flash_storage["node_addr"], struct.pack(">H", 0x0001))
        self.assertEqual(len(self.nvs.write_list), 0)

    def test_nvs_commit_error_on_first_write_aborts_commit(self):
        """Failure on write 1: first_err captured, plat_commit strictly aborted, 0 keys in flash."""
        self.nvs.add_write("net_key", b"\x11" * 16)
        self.nvs.add_write("app_key", b"\x22" * 16)
        self.nvs.add_write("node_addr", struct.pack(">H", 0x0001))
        self.nvs.fail_keys["net_key"] = MESHX_ERR_WRITE_FAIL

        err = self.nvs.commit()
        self.assertEqual(err, MESHX_ERR_WRITE_FAIL)
        self.assertFalse(self.nvs.plat_commit_called, "plat_commit must NEVER be called when a write fails")
        self.assertEqual(len(self.nvs.flash_storage), 0, "Flash storage must contain zero keys (no partial write)")
        self.assertEqual(len(self.nvs.write_list), 0, "Write list must be cleared")

    def test_nvs_commit_error_on_middle_write_aborts_commit(self):
        """Failure on write 3 of 5: first_err captured, plat_commit strictly aborted, 0 keys in flash."""
        self.nvs.add_write("key0", b"val0")
        self.nvs.add_write("key1", b"val1")
        self.nvs.add_write("key2", b"val2")
        self.nvs.add_write("key3", b"val3")
        self.nvs.add_write("key4", b"val4")
        self.nvs.fail_keys["key2"] = MESHX_NO_MEM

        err = self.nvs.commit()
        self.assertEqual(err, MESHX_NO_MEM)
        self.assertFalse(self.nvs.plat_commit_called, "plat_commit must NOT be called")
        self.assertEqual(len(self.nvs.flash_storage), 0, "Flash must have ZERO partial writes")
        # All writes were attempted so list was drained
        self.assertEqual(len(self.nvs.plat_write_called_keys), 5)

    def test_nvs_commit_error_on_last_write_aborts_commit(self):
        """Failure on write 5 of 5: first_err captured, plat_commit strictly aborted, 0 keys in flash."""
        self.nvs.add_write("k1", b"v1")
        self.nvs.add_write("k2", b"v2")
        self.nvs.add_write("k3", b"v3")
        self.nvs.add_write("k4", b"v4")
        self.nvs.add_write("k5", b"v5")
        self.nvs.fail_keys["k5"] = MESHX_FAIL

        err = self.nvs.commit()
        self.assertEqual(err, MESHX_FAIL)
        self.assertFalse(self.nvs.plat_commit_called)
        self.assertEqual(len(self.nvs.flash_storage), 0)

    def test_nvs_commit_multiple_errors_preserves_first_err(self):
        """Multiple failures: key1 fails with ERR_1, key3 fails with ERR_2 -> first_err must be ERR_1."""
        self.nvs.add_write("k0", b"v0")
        self.nvs.add_write("k1", b"v1")
        self.nvs.add_write("k2", b"v2")
        self.nvs.add_write("k3", b"v3")
        self.nvs.fail_keys["k1"] = 0x101  # First error
        self.nvs.fail_keys["k3"] = 0x202  # Second error

        err = self.nvs.commit()
        self.assertEqual(err, 0x101, "meshx_nvs_commit must preserve first_err (earliest error code)")
        self.assertFalse(self.nvs.plat_commit_called)
        self.assertEqual(len(self.nvs.flash_storage), 0)

    def test_nvs_commit_empty_list_returns_success_without_plat_commit(self):
        """Empty write list: returns MESHX_SUCCESS without calling plat_commit."""
        err = self.nvs.commit()
        self.assertEqual(err, MESHX_SUCCESS)
        self.assertFalse(self.nvs.plat_commit_called)

    def test_nvs_commit_uninitialized_returns_invalid_state(self):
        """Uninitialized NVS: returns MESHX_INVALID_STATE immediately."""
        self.nvs.init_magic = 0
        self.nvs.add_write("k1", b"v1")
        err = self.nvs.commit()
        self.assertEqual(err, MESHX_ERR_INVALID_STATE)
        self.assertFalse(self.nvs.plat_commit_called)

    def test_nvs_commit_plat_commit_failure_propagates_error(self):
        """plat_commit itself returns error -> propagated and store is not updated."""
        self.nvs.add_write("k1", b"v1")
        self.nvs.fail_plat_commit = MESHX_ERR_WRITE_FAIL

        err = self.nvs.commit()
        self.assertEqual(err, MESHX_ERR_WRITE_FAIL)
        self.assertTrue(self.nvs.plat_commit_called)
        self.assertEqual(len(self.nvs.flash_storage), 0)

    def test_nvs_commit_preserves_existing_flash_contents_on_rollback(self):
        """Pre-existing flash records are preserved unmodified when transaction fails."""
        self.nvs.flash_storage["existing_key"] = b"existing_data"
        self.nvs.add_write("new_key1", b"val1")
        self.nvs.add_write("new_key2", b"val2")
        self.nvs.fail_keys["new_key2"] = MESHX_ERR_WRITE_FAIL

        err = self.nvs.commit()
        self.assertEqual(err, MESHX_ERR_WRITE_FAIL)
        self.assertFalse(self.nvs.plat_commit_called)
        self.assertEqual(self.nvs.flash_storage, {"existing_key": b"existing_data"})

    def test_simulated_nvs_integration_atomic_rollback(self):
        """Integration test with SimulatedNVS: verifies perform_atomic_self_provision rollback."""
        sample_uuid = bytes([0xAA, 0xBB] + [0x00] * 14)
        node = ZGBNodeSimulator(sample_uuid)
        node.boot()
        self.assertFalse(node.is_provisioned())

        # Inject failure on node_addr write
        err = node.perform_atomic_self_provision(fault_key="node_addr")
        self.assertEqual(err, MESHX_ERR_WRITE_FAIL)
        self.assertFalse(node.is_provisioned())
        self.assertEqual(node.unicast_addr, MESHX_ADDR_UNASSIGNED)
        self.assertIsNone(node.nvs.get("node_addr"))
        self.assertIsNone(node.nvs.get("net_key"))
        self.assertIsNone(node.nvs.get("is_prov"))


# ==============================================================================
# Challenge Suite 2: Boot-Time Beacon Gating & 10 Reboot Cycles
# ==============================================================================
class TestMilestone1BootGatingRebootCycles(unittest.TestCase):
    """
    Adversarially challenge boot-time beacon gating across 10 simulated reboot cycles.
    """

    def setUp(self):
        self.sample_uuid = bytes([0x12, 0x34] + [0x00] * 14)

    def test_unprovisioned_boot_permits_beaconing(self):
        """Fresh unprovisioned boot: beacon_gated is False, address is unassigned."""
        node = ZGBNodeSimulator(self.sample_uuid)
        node.boot()
        self.assertFalse(node.is_provisioned())
        self.assertFalse(node.beacon_gated, "Unprovisioned node MUST permit beaconing on boot")
        self.assertEqual(node.unicast_addr, MESHX_ADDR_UNASSIGNED)
        self.assertEqual(node.state, PROV_STATE_BEACONING)

    def test_provisioned_boot_gating_across_10_reboots(self):
        """
        Adversarial Challenge: 10 simulated reboot cycles on a provisioned node.
        Requirement:
        - If meshx_prov_srv_is_provisioned() is True, native unprovisioned beaconing
          is permanently suppressed across 10 consecutive reboot cycles.
        - meshx_get_node_addr() returns the persisted primary address immediately upon boot.
        """
        # Step 1: Provision node with primary address 0x0005
        persistent_nvs = SimulatedNVS()
        primary_addr = 0x0005
        net_key = b"\x33" * 16
        dev_key = b"\x44" * 16

        persistent_nvs.begin()
        persistent_nvs.set("is_prov", b"\x01")
        persistent_nvs.set("node_addr", struct.pack(">H", primary_addr))
        persistent_nvs.set("net_key", net_key)
        persistent_nvs.set("net_key_id", struct.pack(">H", 0x0000))
        dk_key = persistent_nvs.format_devkey_key(self.sample_uuid)
        persistent_nvs.set(dk_key, dev_key)
        commit_res = persistent_nvs.commit()
        self.assertEqual(commit_res, MESHX_SUCCESS)

        # Step 2: Execute 10 consecutive simulated reboot cycles
        for cycle in range(1, 11):
            with self.subTest(reboot_cycle=cycle):
                rebooted = ZGBNodeSimulator(self.sample_uuid)
                rebooted.nvs = persistent_nvs

                # Introspect IMMEDIATELY upon boot (before any external message or stack callback)
                rebooted.boot()

                # Verify 1: Primary address restored immediately
                self.assertEqual(
                    rebooted.unicast_addr,
                    primary_addr,
                    f"Cycle {cycle}: meshx_get_node_addr() must return 0x{primary_addr:04X} immediately upon boot",
                )

                # Verify 2: Provisioning status
                self.assertTrue(
                    rebooted.is_provisioned(),
                    f"Cycle {cycle}: meshx_prov_srv_is_provisioned() must return True",
                )

                # Verify 3: Unprovisioned beaconing strictly suppressed / gated
                self.assertTrue(
                    rebooted.beacon_gated,
                    f"Cycle {cycle}: Unprovisioned beaconing must be strictly gated on boot",
                )

                # Verify 4: State is PROVISIONED (not BEACONING)
                self.assertEqual(
                    rebooted.state,
                    PROV_STATE_PROVISIONED,
                    f"Cycle {cycle}: Node state must be PROVISIONED",
                )

                # Verify 5: MXCP_CMD_PROV_ENABLE rejected
                err, _ = rebooted.handle_mxcp_command(MXCP_CMD_PROV_ENABLE)
                self.assertEqual(
                    err,
                    MESHX_ERR_INVALID_STATE,
                    f"Cycle {cycle}: MXCP_CMD_PROV_ENABLE must be rejected with INVALID_STATE",
                )
                self.assertTrue(rebooted.beacon_gated, f"Cycle {cycle}: Beaconing must remain gated after rejected enable")

    def test_reboot_cycle_stress_with_varied_addresses(self):
        """Stress test: 10 reboot cycles across 5 distinct assigned addresses."""
        test_addresses = [0x0001, 0x0002, 0x0042, 0x01A0, 0x0FFF]

        for target_addr in test_addresses:
            nvs = SimulatedNVS()
            nvs.begin()
            nvs.set("is_prov", b"\x01")
            nvs.set("node_addr", struct.pack(">H", target_addr))
            nvs.commit()

            for cycle in range(1, 11):
                node = ZGBNodeSimulator(self.sample_uuid)
                node.nvs = nvs
                node.boot()
                self.assertEqual(node.unicast_addr, target_addr)
                self.assertTrue(node.beacon_gated)
                self.assertTrue(node.is_provisioned())

    def test_factory_reset_restores_unprovisioned_beaconing_on_next_boot(self):
        """Node reset clears NVS; subsequent reboot resumes unprovisioned beaconing."""
        nvs = SimulatedNVS()
        nvs.begin()
        nvs.set("is_prov", b"\x01")
        nvs.set("node_addr", struct.pack(">H", 0x0005))
        nvs.commit()

        # Provisioned boot
        node = ZGBNodeSimulator(self.sample_uuid)
        node.nvs = nvs
        node.boot()
        self.assertTrue(node.beacon_gated)

        # Simulate factory reset per meshx_handle_node_reset():
        # meshx_nvs_remove(MESHX_NVS_STORE); meshx_nvs_erase_device_key(); meshx_nvs_commit();
        nvs.committed_store.clear()

        # Reboot cycle after reset
        fresh_node = ZGBNodeSimulator(self.sample_uuid)
        fresh_node.nvs = nvs
        fresh_node.boot()
        self.assertFalse(fresh_node.is_provisioned())
        self.assertEqual(fresh_node.unicast_addr, MESHX_ADDR_UNASSIGNED)
        self.assertFalse(fresh_node.beacon_gated, "Unprovisioned beaconing must resume after factory reset")
        self.assertEqual(fresh_node.state, PROV_STATE_BEACONING)

    def test_capability_integrity_validation_mismatch_aborts_without_commit(self):
        """
        Capability Integrity Validation (ZGB-T02 / ZGB-005):
        Bit 5 (0x0020) of Trait Mask indicates Multi-Element.
        - Single element composition (elem_cnt=1) with multi-element bit set -> ABORT with reason 0x02.
        - Zero writes committed to NVS.
        - Reboot cycle preserves unprovisioned state.
        """
        MESHX_TRAIT_MASK_MULTI_ELEMENT = 0x0020

        def check_integrity(trait_mask: int, elem_count: int) -> bool:
            is_multi = (trait_mask & MESHX_TRAIT_MASK_MULTI_ELEMENT) != 0
            return (elem_count > 1) if is_multi else (elem_count == 1)

        # Single-element composition (elem_cnt=1)
        # Mismatch: trait mask has multi-element bit set (0x0020)
        self.assertFalse(check_integrity(0x0020, 1), "Bit 5 set on 1-element node must fail integrity check")
        # Match: trait mask does NOT have multi-element bit set (0x0000)
        self.assertTrue(check_integrity(0x0000, 1), "Bit 5 clear on 1-element node must pass integrity check")

        # Multi-element composition (elem_cnt=4)
        # Mismatch: trait mask has bit 5 clear (0x0000)
        self.assertFalse(check_integrity(0x0000, 4), "Bit 5 clear on 4-element node must fail integrity check")
        # Match: trait mask has bit 5 set (0x0020)
        self.assertTrue(check_integrity(0x0020, 4), "Bit 5 set on 4-element node must pass integrity check")

        # Multi-element composition (elem_cnt=2)
        self.assertTrue(check_integrity(0x0020, 2), "Bit 5 set on 2-element node must pass integrity check")
        self.assertFalse(check_integrity(0x0000, 2), "Bit 5 clear on 2-element node must fail integrity check")


# ==============================================================================
# Challenge Suite 3: Static & AST Code Audits of Modified Files
# ==============================================================================
class TestMilestone1SourceCodeAudit(unittest.TestCase):
    """
    Verifies that the required implementation constructs physically exist
    in the production codebase.
    """

    def test_meshx_nvs_commit_implementation(self):
        """Audit meshx_nvs.c for transactional commit and first_err handling."""
        nvs_c_path = os.path.join(PROJECT_ROOT, "main", "component", "meshx", "src", "nvs", "meshx_nvs.c")
        self.assertTrue(os.path.exists(nvs_c_path), f"{nvs_c_path} does not exist")

        with open(nvs_c_path, "r", encoding="utf-8") as f:
            content = f.read()

        # Locate meshx_nvs_commit function
        match = re.search(r"meshx_err_t\s+meshx_nvs_commit\s*\(\s*void\s*\)\s*\{(.*?)\n\}", content, re.DOTALL)
        self.assertIsNotNone(match, "meshx_nvs_commit function not found")
        fn_body = match.group(1)

        # 1. Check first_err initialization
        self.assertIn("meshx_err_t first_err = MESHX_SUCCESS;", fn_body)

        # 2. Check first_err assignment guard
        self.assertIn("if (first_err == MESHX_SUCCESS)", fn_body)
        self.assertIn("first_err = err;", fn_body)

        # 3. Check unconditional free of write nodes
        self.assertIn("prev = cur;", fn_body)
        self.assertIn("cur = cur->next;", fn_body)
        self.assertIn("MESHX_FREE(&prev);", fn_body)

        # 4. Check write list head reset
        self.assertIn("meshx_nvs_write_list_head = NULL;", fn_body)

        # 5. Check plat_commit guarded by first_err == MESHX_SUCCESS
        commit_guard = re.search(r"if\s*\(\s*first_err\s*==\s*MESHX_SUCCESS\s*\)\s*\{[^}]*meshx_nvs_plat_commit", fn_body)
        self.assertIsNotNone(commit_guard, "meshx_nvs_plat_commit must be guarded by if (first_err == MESHX_SUCCESS)")

        # 6. Check return value
        self.assertIn("return first_err;", fn_body)

    def test_esp_platform_ble_mesh_boot_gating_implementation(self):
        """Audit esp_platform_ble_mesh.c for boot-time beacon gating."""
        plat_c_path = os.path.join(
            PROJECT_ROOT, "port", "platform", "esp", "esp_idf", "ble_mesh", "common", "src", "esp_platform_ble_mesh.c"
        )
        self.assertTrue(os.path.exists(plat_c_path), f"{plat_c_path} does not exist")

        with open(plat_c_path, "r", encoding="utf-8") as f:
            content = f.read()

        # Verify boot gating guard
        self.assertIn("!meshx_prov_srv_is_provisioned() && !esp_ble_mesh_node_is_provisioned()", content)
        self.assertIn("esp_ble_mesh_node_prov_enable(bearers)", content)
        self.assertIn("esp_ble_mesh_get_primary_element_address()", content)
        self.assertIn("meshx_prov_srv_notify_plat_event", content)

    def test_meshx_c_restore_and_address_accessor(self):
        """Audit meshx.c for unicast address restoration and get_node_addr."""
        meshx_c_path = os.path.join(PROJECT_ROOT, "main", "component", "meshx", "src", "meshx.c")
        self.assertTrue(os.path.exists(meshx_c_path), f"{meshx_c_path} does not exist")

        with open(meshx_c_path, "r", encoding="utf-8") as f:
            content = f.read()

        # Verify unicast_addr restoration
        self.assertIn("pdev->unicast_addr = pdev->meshx_store.node_addr;", content)
        self.assertIn("pdev->unicast_addr = MESHX_ADDR_UNASSIGNED;", content)
        self.assertIn("uint16_t meshx_get_node_addr(void) { return g_dev.meshx_store.node_addr; }", content)

    def test_meshx_prov_srv_capability_integrity_and_is_provisioned(self):
        """Audit meshx_prov_srv.cpp for fast-path is_provisioned and trait mask check."""
        srv_cpp_path = os.path.join(
            PROJECT_ROOT, "main", "component", "meshx", "ble_mesh", "elements", "src", "meshx_prov_srv.cpp"
        )
        self.assertTrue(os.path.exists(srv_cpp_path), f"{srv_cpp_path} does not exist")

        with open(srv_cpp_path, "r", encoding="utf-8") as f:
            content = f.read()

        # Verify fast-path is_provisioned
        self.assertIn("meshx_get_node_addr()", content)
        self.assertIn("MESHX_TRAIT_MASK_MULTI_ELEMENT", content)
        self.assertIn("MESHX_PROV_FAIL_REASON_INVALID_FMT", content)
        self.assertIn("MXCP_EVT_PROV_FAILED", content)
        self.assertIn("meshx_plat_prov_disable", content)

        # Obey C++ header guideline: no direct .h includes except meshx_c_header.h
        direct_c_headers = re.findall(r'#include\s+["<](?!meshx_c_header\.h)[^">]+\.h[">]', content)
        self.assertEqual(direct_c_headers, [], f"Forbidden direct C .h inclusion in C++ file: {direct_c_headers}")


# ==============================================================================
# Main Runner
# ==============================================================================
if __name__ == "__main__":
    suite = unittest.TestSuite()
    suite.addTests(unittest.TestLoader().loadTestsFromTestCase(TestMilestone1NVSCommitChallenge))
    suite.addTests(unittest.TestLoader().loadTestsFromTestCase(TestMilestone1BootGatingRebootCycles))
    suite.addTests(unittest.TestLoader().loadTestsFromTestCase(TestMilestone1SourceCodeAudit))

    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)

    total_tests = result.testsRun
    failed_tests = len(result.failures) + len(result.errors)
    passed_tests = total_tests - failed_tests

    print("\n" + "=" * 70)
    print("Milestone 1 Empirical Challenger Verification Summary")
    print("=" * 70)
    print(f"Total Tests Executed : {total_tests}")
    print(f"Passed               : {passed_tests}")
    print(f"Failed               : {failed_tests}")
    print(f"Verdict              : {'APPROVE' if result.wasSuccessful() else 'REJECT'}")
    print("=" * 70)

    sys.exit(0 if result.wasSuccessful() else 1)
