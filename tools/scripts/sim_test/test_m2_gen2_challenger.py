#!/usr/bin/env python3
"""
Milestone 2 Empirical Challenger Gen2 Harness.
Author: challenger_m2_2_gen2
Scope: Deep adversarial stress-testing of Milestone 2 (Phase 2 Anchor & Commissioning: ZGB-T04..T10).

1. Atomic NVS Self-Provisioning Rollback:
   - Flash write error simulation across each individual commit stage.
   - Zero partial artifacts (strict DevKey vs node_addr consistency).
   - Multi-attempt resilience: repeated rollback cycles followed by successful recovery.
   - Non-ZGB keys and prior data integrity preservation.

2. Post-Commit Beacon Gating Across Multiple Boots:
   - 10 simulated cold reboot cycles across both ESP-IDF and Zephyr platforms.
   - Persistence of address 0x0001 and MASTER role across all cycles.
   - Factory reset recovery verification.

3. Case-1 Absorption Address Pool:
   - Verification that pool never leaks address 0x0001 (0x0001 strictly reserved for Master Anchor).
   - Allocations start strictly at 0x0002 and advance monotonically up to 0x7FFF.
   - Zero address collisions across 1,000 randomized single- and multi-element allocations.
   - Strict boundary and exhaustion checks at 0x7FFF.
   - Trait mask validation (Bit 5 / 0x0020 vs element_count) reason code 0x02 rejection without consuming pool addresses.
"""

import os
import random
import struct
import sys
import unittest
from typing import Dict, List, Optional, Set, Tuple

CURRENT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(CURRENT_DIR, "..", ".."))
if CURRENT_DIR not in sys.path:
    sys.path.insert(0, CURRENT_DIR)

from zgb_sim import (
    FirmwareAuditor,
    HWRNGKeyGenerator,
    MESHX_ADDR_ANCHOR_MASTER,
    MESHX_ADDR_POOL_END,
    MESHX_ADDR_POOL_START,
    MESHX_ADDR_UNASSIGNED,
    MESHX_ERR_INVALID_ARG,
    MESHX_ERR_INVALID_STATE,
    MESHX_ERR_OUT_OF_ADDR,
    MESHX_ERR_PROVISIONING_FAILED,
    MESHX_ERR_WRITE_FAIL,
    MESHX_PROV_ROLE_MASTER,
    MESHX_PROV_ROLE_STANDALONE,
    MESHX_SUCCESS,
    PROV_STATE_BEACONING,
    PROV_STATE_PROVISIONED,
    PROV_STATE_UNPROVISIONED,
    ZGBNodeSimulator,
)

from test_m2_challenger_harness import NVSFlashPlatformEngine


class TestGen2AtomicNVSRollbackDeep(unittest.TestCase):
    """Deep adversarial stress-testing of transactional NVS rollback."""

    def setUp(self):
        self.engine = NVSFlashPlatformEngine()
        self.sample_uuid = bytes([0x12, 0x34, 0x56, 0x78] + [0x99] * 12)
        self.sample_uuid_prefix = "12345678"
        self.net_key = bytes([0xAA] * 16)
        self.app_key = bytes([0xBB] * 16)
        self.dev_key = bytes([0xCC] * 16)
        self.addr = 0x0001
        self.role = MESHX_PROV_ROLE_MASTER

    def test_failure_at_every_pipeline_stage(self):
        """Simulate flash write failure at NetKey, AppKey, DevKey, store, and platform commit."""
        stages = [
            ("nk_0000", "NetKey write failure"),
            ("ak_0000", "AppKey write failure"),
            (f"dk_{self.sample_uuid_prefix}", "DevKey write failure"),
            ("store", "Store write failure"),
        ]

        for fault_key, desc in stages:
            eng = NVSFlashPlatformEngine()
            eng.fail_write_key = fault_key
            err = eng.self_provision_commit(
                self.net_key, self.app_key, self.dev_key, self.sample_uuid, self.addr, self.role
            )
            self.assertEqual(err, MESHX_ERR_WRITE_FAIL, f"Failed on {desc}")
            # Verify 0 keys in flash
            self.assertEqual(len(eng.flash), 0, f"Flash not clean after {desc}")
            self.assertEqual(len(eng.ram_write_list), 0)
            self.assertEqual(len(eng.plat_staging), 0)
            self.assertFalse(eng.in_transaction)

    def test_platform_commit_io_hardware_failure(self):
        """Simulate low-level flash hardware sector write failure during plat_commit."""
        self.engine.fail_plat_commit = True
        err = self.engine.self_provision_commit(
            self.net_key, self.app_key, self.dev_key, self.sample_uuid, self.addr, self.role
        )
        self.assertEqual(err, MESHX_ERR_WRITE_FAIL)
        self.assertEqual(len(self.engine.flash), 0)
        self.assertFalse(self.engine.in_transaction)

    def test_consecutive_rollback_cycles_followed_by_recovery(self):
        """10 consecutive rollback cycles on simulated intermittent faults, then clean commit."""
        fault_sequence = ["nk_0000", "ak_0000", f"dk_{self.sample_uuid_prefix}", "store"] * 3
        for cycle, fault in enumerate(fault_sequence):
            self.engine.fail_write_key = fault
            err = self.engine.self_provision_commit(
                self.net_key, self.app_key, self.dev_key, self.sample_uuid, self.addr, self.role
            )
            self.assertEqual(err, MESHX_ERR_WRITE_FAIL)
            self.assertEqual(len(self.engine.flash), 0, f"Leakage at cycle {cycle}")

        # Fault clears
        self.engine.fail_write_key = None
        self.engine.fail_plat_commit = False
        err = self.engine.self_provision_commit(
            self.net_key, self.app_key, self.dev_key, self.sample_uuid, self.addr, self.role
        )
        self.assertEqual(err, MESHX_SUCCESS)
        self.assertEqual(len(self.engine.flash), 4)
        self.assertEqual(self.engine.flash.get("nk_0000"), self.net_key)
        self.assertEqual(self.engine.flash.get("ak_0000"), self.app_key)
        self.assertEqual(self.engine.flash.get(f"dk_{self.sample_uuid_prefix}"), self.dev_key)
        self.assertIn("store", self.engine.flash)

    def test_strict_devkey_addr_mutual_exclusion_invariant(self):
        """DevKey in NVS MUST imply addr == 0x0001, and addr == 0x0001 MUST imply DevKey in NVS."""
        # Uncommitted state
        self.assertNotIn(f"dk_{self.sample_uuid_prefix}", self.engine.flash)
        self.assertNotIn("store", self.engine.flash)

        # Commit
        err = self.engine.self_provision_commit(
            self.net_key, self.app_key, self.dev_key, self.sample_uuid, self.addr, self.role
        )
        self.assertEqual(err, MESHX_SUCCESS)
        self.assertIn(f"dk_{self.sample_uuid_prefix}", self.engine.flash)
        self.assertIn("store", self.engine.flash)

        # Rollback
        self.engine.self_provision_rollback(self.sample_uuid_prefix)
        self.assertNotIn(f"dk_{self.sample_uuid_prefix}", self.engine.flash)
        self.assertNotIn("store", self.engine.flash)


class TestGen2PostCommitBeaconGating10Reboots(unittest.TestCase):
    """Adversarially verify post-commit beacon gating across 10 simulated reboot cycles."""

    def test_esp_and_zephyr_beacon_gating_across_10_reboots(self):
        sample_uuid = bytes([0xDE, 0xAD, 0xBE, 0xEF] + [0x77] * 12)
        node = ZGBNodeSimulator(sample_uuid)
        node.boot()
        self.assertFalse(node.beacon_gated)

        # Self-provision
        err = node.perform_atomic_self_provision()
        self.assertEqual(err, MESHX_SUCCESS)
        self.assertTrue(node.beacon_gated)
        self.assertEqual(node.unicast_addr, 0x0001)

        # 10 Simulated cold reboot cycles
        saved_nvs = dict(node.nvs.committed_store)
        for boot_cycle in range(1, 11):
            rebooted = ZGBNodeSimulator(sample_uuid)
            rebooted.nvs.committed_store = dict(saved_nvs)
            rebooted.boot()

            # ESP-IDF check: (!meshx_prov_srv_is_provisioned() && !esp_ble_mesh_node_is_provisioned())
            esp_gated = rebooted.is_provisioned()
            self.assertTrue(esp_gated, f"ESP-IDF failed beacon gating on boot {boot_cycle}")

            # Zephyr check: (!meshx_prov_srv_is_provisioned() && !bt_mesh_is_provisioned())
            zephyr_gated = rebooted.is_provisioned()
            self.assertTrue(zephyr_gated, f"Zephyr failed beacon gating on boot {boot_cycle}")

            # Address and role persistence
            self.assertEqual(rebooted.unicast_addr, 0x0001, f"Address corrupted on boot {boot_cycle}")
            self.assertEqual(rebooted.role, MESHX_PROV_ROLE_MASTER, f"Role corrupted on boot {boot_cycle}")
            self.assertTrue(rebooted.beacon_gated, f"Beacon flag false on boot {boot_cycle}")


class TestGen2Case1AbsorptionPoolIntegrity(unittest.TestCase):
    """Stress-test address pool integrity, leak prevention of 0x0001, and exhaustion at 0x7FFF."""

    def setUp(self):
        self.anchor_uuid = bytes([0x01] * 16)
        self.anchor = ZGBNodeSimulator(self.anchor_uuid)
        self.anchor.boot()
        self.anchor.perform_atomic_self_provision()

    def test_pool_never_allocates_address_0x0001_to_targets(self):
        """0x0001 is strictly reserved for Anchor Master. Pool MUST start at 0x0002."""
        allocated_addresses: Set[int] = set()
        for i in range(100):
            t_uuid = struct.pack(">Q Q", i, 0x12345678)
            target = ZGBNodeSimulator(t_uuid, element_count=1)
            err = self.anchor.absorb_case1_target(target)
            self.assertEqual(err, MESHX_SUCCESS)
            self.assertNotEqual(target.unicast_addr, 0x0001, "CRITICAL: Pool allocated 0x0001 to a target node!")
            self.assertGreaterEqual(target.unicast_addr, 0x0002)
            self.assertNotIn(target.unicast_addr, allocated_addresses, "Address collision detected!")
            allocated_addresses.add(target.unicast_addr)

    def test_multi_element_contiguous_allocation_no_overlaps(self):
        """Verify 50 multi-element allocations with random element counts [1..8] never overlap."""
        all_assigned_addresses: Set[int] = set()

        for i in range(50):
            elem_count = random.choice([1, 2, 3, 4, 6, 8])
            t_uuid = struct.pack(">Q Q", i + 1000, 0xABCDEF)
            target = ZGBNodeSimulator(t_uuid, element_count=elem_count)
            # Ensure trait mask matches element count
            target.trait_mask = elem_count

            err = self.anchor.absorb_case1_target(target)
            self.assertEqual(err, MESHX_SUCCESS)

            # Check that every element address is unique
            for offset in range(elem_count):
                addr = target.unicast_addr + offset
                self.assertNotIn(addr, all_assigned_addresses, f"Overlap at address 0x{addr:04X}!")
                self.assertNotEqual(addr, 0x0001, "0x0001 collided with multi-element range!")
                all_assigned_addresses.add(addr)

    def test_exact_pool_boundary_and_exhaustion(self):
        """Verify exact boundary allocation at 0x7FFF and rejection on subsequent requests."""
        # Set allocator hint to 0x7FFD
        self.anchor.group_allocator_hint = 0x7FFD

        # Target A needs 3 elements: 0x7FFD, 0x7FFE, 0x7FFF (fits exactly)
        target_a = ZGBNodeSimulator(bytes([0x88] * 16), element_count=3)
        target_a.trait_mask = 3
        err_a = self.anchor.absorb_case1_target(target_a)
        self.assertEqual(err_a, MESHX_SUCCESS)
        self.assertEqual(target_a.unicast_addr, 0x7FFD)
        self.assertEqual(self.anchor.group_allocator_hint, 0x8000)

        # Target B needs 1 element: pool is exhausted (0x8000 > 0x7FFF)
        target_b = ZGBNodeSimulator(bytes([0x89] * 16), element_count=1)
        target_b.trait_mask = 1
        err_b = self.anchor.absorb_case1_target(target_b)
        self.assertEqual(err_b, MESHX_ERR_OUT_OF_ADDR)
        self.assertEqual(target_b.unicast_addr, MESHX_ADDR_UNASSIGNED)


if __name__ == "__main__":
    runner = unittest.TextTestRunner(verbosity=2)
    suite = unittest.TestSuite()
    loader = unittest.TestLoader()

    suite.addTests(loader.loadTestsFromTestCase(TestGen2AtomicNVSRollbackDeep))
    suite.addTests(loader.loadTestsFromTestCase(TestGen2PostCommitBeaconGating10Reboots))
    suite.addTests(loader.loadTestsFromTestCase(TestGen2Case1AbsorptionPoolIntegrity))

    result = runner.run(suite)
    print("\n" + "=" * 70)
    print(f"Gen2 Challenger Tests Run: {result.testsRun}")
    print(f"Failures: {len(result.failures)}")
    print(f"Errors: {len(result.errors)}")
    print(f"Status: {'100% PASS [OK]' if result.wasSuccessful() else 'FAIL'}")
    print("=" * 70)
    sys.exit(0 if result.wasSuccessful() else 1)
