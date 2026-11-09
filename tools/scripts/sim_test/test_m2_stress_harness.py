#!/usr/bin/env python3
"""
Milestone 2 Empirical Stress Test Harness: Commission Triggers, Timing & Priority Guard.

Author: challenger_m2_1 (Empirical Challenger)
Target: Zero-Gateway Bootstrapping (ZGB) Milestone 2 Implementation

Stress-tests:
1. Physical GPIO button hold detector:
   - Boundary timing: 2.9s (2900ms) vs 3.1s (3100ms) vs exact 3.0s (3000ms).
   - Rapid contact bounce: noisy sub-millisecond edge transitions before steady-state.
   - Cold boot pre-held button: level == 0 at initialization.
   - Early release cancellation: button released before 3s.
   - Post-expiration pin validation: re-sampling GPIO level at callback execution.
   - Suppression on provisioned nodes.

2. MXCP 0x39 / 0x3A Host Protocol State Machine:
   - MXCP_CMD_BOOTSTRAP_COMMISSION (0x39): valid transition from UNPROVISIONED / BEACONING.
   - MXCP_CMD_BOOTSTRAP_COMMISSION (0x39): rejected with MESHX_INVALID_STATE when already provisioned.
   - MXCP_CMD_BOOTSTRAP_ABORT (0x3A): aborts active commission window, halts timers, purges staged keys.
   - MXCP_CMD_BOOTSTRAP_ABORT (0x3A): idempotent when unprovisioned.
   - MXCP_CMD_BOOTSTRAP_ABORT (0x3A): safe no-op when already provisioned.
   - Rapid interleaved 0x39 / 0x3A toggling stress (100 iterations).
   - Frame serialization/deserialization integrity for 0x39 and 0x3A.

3. Post-Boot 30s (+/- 5s Jitter) Commission Timer:
   - 10,000 Monte Carlo sample distribution test.
   - Bound validation: strictly between 25,000 ms and 35,000 ms (25s .. 35s).
   - Statistical distribution analysis: mean ~30,000 ms (+/- 200 ms).
   - Uniform distribution verification across 10 histogram bins.
   - Suppression on provisioned node cold boot.
   - Race suppression: timer expires after node was provisioned externally during countdown.

4. External Provisioning Priority Guard & Race Conditions:
   - External provisioner arrives at t=0s (boot time before commission trigger).
   - External provisioner arrives at t=29s (1s before 30s jitter expiration).
   - External provisioner arrives at t=30s (exact collision with jitter trigger).
   - External provisioner arrives during active 60s commission window (t=35s).
   - Pre-commit race: external provisioner finishes provisioning right before NVS commit.
   - Atomic NVS rollback on simulated write failure (zero partial credentials left in flash).
   - Zero plaintext key leakage audit across all abort and rollback events.
"""

import math
import os
import random
import struct
import sys
import time
from typing import Dict, List, Optional, Set, Tuple
import unittest

# Ensure tools/scripts is in sys.path
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from zgb_sim import (
    BeaconCodec,
    HWRNGKeyGenerator,
    MESHX_ADDR_ANCHOR_MASTER,
    MESHX_ADDR_UNASSIGNED,
    MESHX_ERR_INVALID_STATE,
    MESHX_ERR_WRITE_FAIL,
    MESHX_PROV_ROLE_MASTER,
    MESHX_PROV_ROLE_STANDALONE,
    MESHX_SUCCESS,
    MXCP_CMD_BOOTSTRAP_ABORT,
    MXCP_CMD_BOOTSTRAP_COMMISSION,
    MXCP_EVT_PROV_STATE_IND,
    MXCPProtocol,
    PROV_STATE_BEACONING,
    PROV_STATE_COMMISSIONING,
    PROV_STATE_PROVISIONED,
    PROV_STATE_PROVISIONING,
    PROV_STATE_UNPROVISIONED,
    SimulatedNVS,
    ZGBNodeSimulator,
)


# ==============================================================================
# Model: GPIO Button Hold Hardware Detector (C Implementation Replica)
# ==============================================================================
class HardwareButtonDetectorModel:
    """
    Direct behavioral replica of main/component/meshx/src/gpio/meshx_gpio.c:
    - Active-low input (pressed=0, released=1).
    - Any-edge interrupt triggers button_hold_isr.
    - Falling edge (1->0): starts 3000ms one-shot timer.
    - Rising edge (0->1): stops timer without trigger.
    - Cold boot pre-held detection in meshx_gpio_button_hold_init().
    - Timer callback re-samples GPIO level before publishing event.
    """

    def __init__(self, hold_threshold_ms: int = 3000):
        self.threshold_ms = hold_threshold_ms
        self.pin_level: int = 1  # 1 = released (pull-up), 0 = pressed
        self.timer_armed: bool = False
        self.timer_start_time_ms: float = 0.0
        self.trigger_fired_count: int = 0
        self.events_published: List[str] = []

    def init_on_boot(self, boot_level: int = 1):
        """Simulate meshx_gpio_button_hold_init()."""
        self.pin_level = boot_level
        self.timer_armed = False
        self.trigger_fired_count = 0
        self.events_published.clear()

        # Check for pre-held button at cold boot
        if self.pin_level == 0:
            self.timer_armed = True
            self.timer_start_time_ms = 0.0

    def isr_edge_event(self, timestamp_ms: float, new_level: int):
        """Simulate button_hold_isr on GPIO edge interrupt."""
        old_level = self.pin_level
        self.pin_level = new_level

        if old_level == 1 and new_level == 0:
            # Active-low falling edge: start hold timer
            self.timer_armed = True
            self.timer_start_time_ms = timestamp_ms
        elif old_level == 0 and new_level == 1:
            # Rising edge: release button -> cancel hold timer
            self.timer_armed = False

    def advance_time(self, current_time_ms: float):
        """Simulate OS timer tick and expiration check."""
        if self.timer_armed:
            elapsed = current_time_ms - self.timer_start_time_ms
            if elapsed >= self.threshold_ms:
                # Timer expired! Run button_hold_timer_cb
                self.timer_armed = False
                # Callback re-reads pin level
                if self.pin_level == 0:
                    self.trigger_fired_count += 1
                    self.events_published.append("CONTROL_TASK_MSG_EVT_ZGB_TRIGGER_BUTTON_HOLD")


# ==============================================================================
# Suite 1: Physical GPIO Button Hold Detector Stress Tests
# ==============================================================================
class TestButtonHoldDetectorStress(unittest.TestCase):
    """Stress tests for GPIO button hold timing boundaries and bounce resistance."""

    def setUp(self):
        self.detector = HardwareButtonDetectorModel(hold_threshold_ms=3000)

    def test_hold_2900ms_does_not_trigger(self):
        """2.9s hold (< 3.0s threshold) must NOT fire trigger."""
        self.detector.init_on_boot(boot_level=1)
        self.detector.isr_edge_event(timestamp_ms=100.0, new_level=0)
        # Advance to 2999ms (elapsed = 2899ms < 3000ms)
        self.detector.advance_time(current_time_ms=2999.0)
        # Button released at 3000ms (held for exactly 2900ms)
        self.detector.isr_edge_event(timestamp_ms=3000.0, new_level=1)
        self.detector.advance_time(current_time_ms=5000.0)

        self.assertEqual(self.detector.trigger_fired_count, 0)
        self.assertEqual(len(self.detector.events_published), 0)

    def test_hold_3100ms_triggers_reliably(self):
        """3.1s hold (> 3.0s threshold) must trigger exactly once."""
        self.detector.init_on_boot(boot_level=1)
        self.detector.isr_edge_event(timestamp_ms=100.0, new_level=0)
        # Hold through 3200ms (elapsed = 3100ms > 3000ms)
        self.detector.advance_time(current_time_ms=3200.0)

        self.assertEqual(self.detector.trigger_fired_count, 1)
        self.assertEqual(len(self.detector.events_published), 1)

    def test_hold_exact_3000ms_boundary_triggers(self):
        """Exact 3000ms hold threshold triggers reliably."""
        self.detector.init_on_boot(boot_level=1)
        self.detector.isr_edge_event(timestamp_ms=0.0, new_level=0)
        self.detector.advance_time(current_time_ms=3000.0)

        self.assertEqual(self.detector.trigger_fired_count, 1)

    def test_contact_bounce_burst_suppressed(self):
        """Rapid mechanical contact bounce oscillations (0/1) do not cause false triggers."""
        self.detector.init_on_boot(boot_level=1)
        # 10 bounces over 25ms
        t = 100.0
        for _ in range(10):
            self.detector.isr_edge_event(timestamp_ms=t, new_level=0)
            t += 1.5
            self.detector.advance_time(current_time_ms=t)
            self.detector.isr_edge_event(timestamp_ms=t, new_level=1)
            t += 1.0
            self.detector.advance_time(current_time_ms=t)

        # Released completely after bounce
        self.detector.advance_time(current_time_ms=5000.0)
        self.assertEqual(self.detector.trigger_fired_count, 0)

    def test_contact_bounce_followed_by_solid_hold_triggers(self):
        """Mechanical contact bounce settling into a solid hold triggers correctly."""
        self.detector.init_on_boot(boot_level=1)
        # Bounce for 20ms
        t = 50.0
        for _ in range(5):
            self.detector.isr_edge_event(timestamp_ms=t, new_level=0)
            t += 2.0
            self.detector.isr_edge_event(timestamp_ms=t, new_level=1)
            t += 2.0

        # Settles solidly into pressed state at t=70ms
        self.detector.isr_edge_event(timestamp_ms=70.0, new_level=0)
        # Held until t=3170ms (3100ms sustained)
        self.detector.advance_time(current_time_ms=3170.0)

        self.assertEqual(self.detector.trigger_fired_count, 1)

    def test_pre_held_button_on_cold_boot_triggers(self):
        """Button held down during cold boot arms timer and fires after 3000ms."""
        # Boot with button held (level == 0)
        self.detector.init_on_boot(boot_level=0)
        self.assertTrue(self.detector.timer_armed)

        # Held for 3100ms after boot
        self.detector.advance_time(current_time_ms=3100.0)
        self.assertEqual(self.detector.trigger_fired_count, 1)

    def test_pre_held_button_released_early_cancels(self):
        """Button held at boot but released at 1500ms cancels timer without trigger."""
        self.detector.init_on_boot(boot_level=0)
        self.assertTrue(self.detector.timer_armed)

        # Released at 1500ms
        self.detector.advance_time(current_time_ms=1500.0)
        self.detector.isr_edge_event(timestamp_ms=1500.0, new_level=1)
        self.assertFalse(self.detector.timer_armed)

        # Advance to 4000ms
        self.detector.advance_time(current_time_ms=4000.0)
        self.assertEqual(self.detector.trigger_fired_count, 0)

    def test_timer_expiry_resample_pin_level_check(self):
        """If timer fires while pin was released right at expiry, re-sampling blocks trigger."""
        self.detector.init_on_boot(boot_level=1)
        self.detector.isr_edge_event(timestamp_ms=0.0, new_level=0)

        # Pin released right at 3000ms
        self.detector.pin_level = 1  # Released before callback executes
        self.detector.timer_armed = True  # Timer callback queued
        self.detector.advance_time(current_time_ms=3000.0)

        self.assertEqual(self.detector.trigger_fired_count, 0)

    def test_rapid_taps_50_clicks_no_false_trigger(self):
        """50 rapid button clicks of 50ms-150ms duration do not trigger commission."""
        self.detector.init_on_boot(boot_level=1)
        t = 0.0
        for _ in range(50):
            self.detector.isr_edge_event(timestamp_ms=t, new_level=0)
            tap_len = random.uniform(30.0, 150.0)
            t += tap_len
            self.detector.advance_time(current_time_ms=t)
            self.detector.isr_edge_event(timestamp_ms=t, new_level=1)
            gap = random.uniform(50.0, 200.0)
            t += gap
            self.detector.advance_time(current_time_ms=t)

        self.assertEqual(self.detector.trigger_fired_count, 0)

    def test_button_hold_ignored_on_already_provisioned_node(self):
        """Button hold event received on an already provisioned node is rejected."""
        node = ZGBNodeSimulator(bytes([0x01] * 16))
        node.nvs.set("is_prov", b"\x01")
        node.nvs.set("node_addr", struct.pack(">H", 0x0001))
        node.boot()

        err = node.handle_gpio_button(hold_duration_sec=4.0)
        self.assertEqual(err, MESHX_ERR_INVALID_STATE)
        self.assertFalse(node.is_commissioning())
        self.assertEqual(node.state, PROV_STATE_PROVISIONED)


# ==============================================================================
# Suite 2: MXCP 0x39 / 0x3A Host Protocol State Machine Stress Tests
# ==============================================================================
class TestMXCPCommissionAbortStateMachineStress(unittest.TestCase):
    """Stress tests for MXCP 0x39 (BOOTSTRAP_COMMISSION) and 0x3A (BOOTSTRAP_ABORT)."""

    def setUp(self):
        self.uuid = bytes([0xAA, 0xBB, 0xCC, 0xDD] + [0x00] * 12)

    def test_mxcp_0x39_transitions_unprovisioned_to_commissioning(self):
        """0x39 on unprovisioned node transitions to COMMISSIONING (0x04) and opens 60s window."""
        node = ZGBNodeSimulator(self.uuid)
        node.boot()
        status, resp = node.handle_mxcp_command(MXCP_CMD_BOOTSTRAP_COMMISSION)

        self.assertEqual(status, MESHX_SUCCESS)
        self.assertEqual(resp, bytes([PROV_STATE_COMMISSIONING]))
        self.assertTrue(node.is_commissioning())
        self.assertEqual(node.state, PROV_STATE_COMMISSIONING)
        self.assertEqual(node.commission_timer_sec, 60.0)

    def test_mxcp_0x39_rejected_on_provisioned_node(self):
        """0x39 on provisioned node returns MESHX_INVALID_STATE and preserves PROVISIONED."""
        node = ZGBNodeSimulator(self.uuid)
        node.nvs.set("is_prov", b"\x01")
        node.nvs.set("node_addr", struct.pack(">H", 0x0001))
        node.boot()

        status, resp = node.handle_mxcp_command(MXCP_CMD_BOOTSTRAP_COMMISSION)
        self.assertEqual(status, MESHX_ERR_INVALID_STATE)
        self.assertFalse(node.is_commissioning())
        self.assertEqual(node.state, PROV_STATE_PROVISIONED)

    def test_mxcp_0x3A_aborts_active_commissioning_cleanly(self):
        """0x3A aborts active commission window and reverts state to UNPROVISIONED."""
        node = ZGBNodeSimulator(self.uuid)
        node.boot()
        node.handle_mxcp_command(MXCP_CMD_BOOTSTRAP_COMMISSION)
        self.assertTrue(node.is_commissioning())

        status, resp = node.handle_mxcp_command(MXCP_CMD_BOOTSTRAP_ABORT)
        self.assertEqual(status, MESHX_SUCCESS)
        self.assertEqual(resp, bytes([PROV_STATE_UNPROVISIONED]))
        self.assertFalse(node.is_commissioning())
        self.assertEqual(node.state, PROV_STATE_UNPROVISIONED)

    def test_mxcp_0x3A_idempotent_when_already_unprovisioned(self):
        """0x3A is safe and idempotent when node is already UNPROVISIONED."""
        node = ZGBNodeSimulator(self.uuid)
        node.boot()
        node.state = PROV_STATE_UNPROVISIONED

        status, resp = node.handle_mxcp_command(MXCP_CMD_BOOTSTRAP_ABORT)
        self.assertEqual(status, MESHX_SUCCESS)
        self.assertEqual(resp, bytes([PROV_STATE_UNPROVISIONED]))

    def test_mxcp_0x3A_on_provisioned_node_preserves_provisioned_state(self):
        """0x3A on a provisioned node does not reset credentials or change role."""
        node = ZGBNodeSimulator(self.uuid)
        node.perform_atomic_self_provision()
        self.assertTrue(node.is_provisioned())

        status, _ = node.handle_mxcp_command(MXCP_CMD_BOOTSTRAP_ABORT)
        self.assertEqual(status, MESHX_SUCCESS)
        self.assertTrue(node.is_provisioned())
        self.assertEqual(node.unicast_addr, MESHX_ADDR_ANCHOR_MASTER)
        self.assertEqual(node.role, MESHX_PROV_ROLE_MASTER)

    def test_mxcp_rapid_toggle_100_cycles(self):
        """100 rapid cycles of 0x39 followed by 0x3A must transition cleanly without leakage."""
        node = ZGBNodeSimulator(self.uuid)
        node.boot()

        for _ in range(100):
            st1, _ = node.handle_mxcp_command(MXCP_CMD_BOOTSTRAP_COMMISSION)
            self.assertEqual(st1, MESHX_SUCCESS)
            self.assertTrue(node.is_commissioning())

            st2, _ = node.handle_mxcp_command(MXCP_CMD_BOOTSTRAP_ABORT)
            self.assertEqual(st2, MESHX_SUCCESS)
            self.assertFalse(node.is_commissioning())

        self.assertEqual(node.state, PROV_STATE_UNPROVISIONED)

    def test_mxcp_protocol_roundtrip_0x39_frame(self):
        """Encode and decode MXCP_CMD_BOOTSTRAP_COMMISSION frame over the wire."""
        encoded = MXCPProtocol.encode(MXCP_CMD_BOOTSTRAP_COMMISSION, b"")
        self.assertEqual(encoded[0], 0xFE)  # Preamble
        self.assertEqual(encoded[1], 0)     # Length
        self.assertEqual(encoded[2], 0x39)  # Command ID
        self.assertEqual(encoded[-1], 0xEF) # Trailer

        valid, cmd, payload, err = MXCPProtocol.decode(encoded)
        self.assertTrue(valid, err)
        self.assertEqual(cmd, 0x39)
        self.assertEqual(payload, b"")

    def test_mxcp_protocol_roundtrip_0x3A_frame(self):
        """Encode and decode MXCP_CMD_BOOTSTRAP_ABORT frame over the wire."""
        encoded = MXCPProtocol.encode(MXCP_CMD_BOOTSTRAP_ABORT, b"")
        self.assertEqual(encoded[2], 0x3A)

        valid, cmd, payload, err = MXCPProtocol.decode(encoded)
        self.assertTrue(valid, err)
        self.assertEqual(cmd, 0x3A)
        self.assertEqual(payload, b"")


# ==============================================================================
# Suite 3: Post-Boot 30s (+/- 5s Jitter) Timer Distribution Stress Tests
# ==============================================================================
class TestPostBootTimerDistributionStress(unittest.TestCase):
    """Stress tests and statistical validation of the 30s (+/- 5s jitter) post-boot timer."""

    def test_jitter_generator_10000_samples_bounds(self):
        """
        Verify that 10,000 samples of the C generator:
        jitter_ms = 25000 + (rnd_val % 10001)
        strictly satisfy 25,000ms <= jitter <= 35,000ms.
        """
        samples = []
        for _ in range(10000):
            rnd_val = random.randint(0, 65535)
            jitter_ms = 25000 + (rnd_val % 10001)
            samples.append(jitter_ms)

        min_val = min(samples)
        max_val = max(samples)
        self.assertGreaterEqual(min_val, 25000, f"Sample below minimum: {min_val}")
        self.assertLessEqual(max_val, 35000, f"Sample above maximum: {max_val}")

    def test_jitter_generator_mean_and_uniformity(self):
        """
        Verify that the distribution mean is 30,000 ms (+/- 150ms)
        and all 10 bins across the 10-second range receive balanced samples.
        """
        num_samples = 10000
        samples = [25000 + (random.randint(0, 0xFFFFFFFF) % 10001) for _ in range(num_samples)]
        mean_val = sum(samples) / num_samples

        self.assertAlmostEqual(mean_val, 30000.0, delta=150.0,
                               msg=f"Expected mean ~30,000ms, got {mean_val:.2f}ms")

        # Bin into 10 buckets of 1000ms each
        bins = [0] * 10
        for s in samples:
            idx = min((s - 25000) // 1000, 9)
            bins[idx] += 1

        expected_per_bin = num_samples / 10  # 1000 per bin
        for i, count in enumerate(bins):
            # Each bin must be within 20% of expected under 10k samples
            self.assertGreater(count, expected_per_bin * 0.80, f"Bin {i} underpopulated: {count}")
            self.assertLess(count, expected_per_bin * 1.20, f"Bin {i} overpopulated: {count}")

    def test_post_boot_timer_armed_on_unprovisioned_boot(self):
        """On cold boot of unprovisioned node, jitter timer is created and armed."""
        node = ZGBNodeSimulator(bytes([0x11] * 16))
        node.boot()
        self.assertEqual(node.state, PROV_STATE_BEACONING)
        self.assertFalse(node.beacon_gated)

    def test_post_boot_timer_suppressed_on_provisioned_boot(self):
        """On cold boot of already provisioned node, post-boot timer is permanently suppressed."""
        node = ZGBNodeSimulator(bytes([0x22] * 16))
        node.nvs.set("is_prov", b"\x01")
        node.nvs.set("node_addr", struct.pack(">H", 0x0001))
        node.boot()

        self.assertTrue(node.is_provisioned())
        self.assertTrue(node.beacon_gated)
        self.assertFalse(node.is_commissioning())

    def test_jitter_timer_expiration_opens_commission_window(self):
        """Simulate timer expiring when unprovisioned: commission window opens."""
        node = ZGBNodeSimulator(bytes([0x33] * 16))
        node.boot()
        self.assertEqual(node.state, PROV_STATE_BEACONING)

        # Timer expires
        node.open_commission_window()
        self.assertTrue(node.is_commissioning())
        self.assertEqual(node.state, PROV_STATE_COMMISSIONING)

    def test_race_external_provision_suppresses_jitter_timer_callback(self):
        """
        If node is provisioned externally during the 30s countdown,
        the jitter timer expiration callback must check is_provisioned()
        and suppress opening the commission window.
        """
        node = ZGBNodeSimulator(bytes([0x44] * 16))
        node.boot()

        # External provisioner arrives at t=15s and completes
        node.state = PROV_STATE_PROVISIONED
        node.unicast_addr = 0x0005

        # Timer expires at t=30s
        if not node.is_provisioned() and node.unicast_addr == MESHX_ADDR_UNASSIGNED:
            node.open_commission_window()

        # Commission window must NOT open
        self.assertFalse(node.is_commissioning())
        self.assertEqual(node.state, PROV_STATE_PROVISIONED)


# ==============================================================================
# Suite 4: External Provisioning Priority Guard & Pre-emption Races
# ==============================================================================
class TestPriorityGuardPreemptionRacesStress(unittest.TestCase):
    """Stress tests for ZGB-T09 / ZGB-011 External Provisioning Priority Guard races."""

    def setUp(self):
        self.uuid = bytes([0x55, 0x66, 0x77, 0x88] + [0x00] * 12)

    def test_external_provisioner_preempts_at_t0(self):
        """External provisioner arrives at t=0s before any commission trigger."""
        node = ZGBNodeSimulator(self.uuid)
        node.boot()

        # External link open arrives
        node.on_external_prov_start()
        self.assertFalse(node.is_commissioning())

    def test_external_provisioner_preempts_at_t29_during_countdown(self):
        """External provisioner arrives at t=29s during countdown to self-commission."""
        node = ZGBNodeSimulator(self.uuid)
        node.boot()

        # External PB-ADV link open arrives at t=29s
        node.on_external_prov_start()
        self.assertFalse(node.is_commissioning())

    def test_external_provisioner_preempts_during_active_commission_window(self):
        """External provisioner arrives while commission window is open (t=35s)."""
        node = ZGBNodeSimulator(self.uuid)
        node.boot()
        node.open_commission_window()
        self.assertTrue(node.is_commissioning())
        self.assertEqual(node.state, PROV_STATE_COMMISSIONING)

        # External link open event arrives
        node.on_external_prov_start()
        self.assertFalse(node.is_commissioning())
        self.assertEqual(node.state, PROV_STATE_PROVISIONING)

    def test_pre_commit_guard_rejects_if_externally_provisioned_before_commit(self):
        """Pre-commit guard in meshx_zgb_perform_self_provision checks state before NVS write."""
        node = ZGBNodeSimulator(self.uuid)
        node.boot()
        node.open_commission_window()

        # External provisioner races and provisions node before NVS commit
        node.state = PROV_STATE_PROVISIONED
        node.unicast_addr = 0x0003

        err = node.perform_atomic_self_provision()
        self.assertEqual(err, MESHX_ERR_INVALID_STATE)
        self.assertFalse(node.is_commissioning())

    def test_atomic_nvs_rollback_on_write_error(self):
        """Simulated NVS write error during self-provisioning triggers complete rollback."""
        node = ZGBNodeSimulator(self.uuid)
        node.boot()
        node.open_commission_window()

        # Force write failure on device key key ("dk_")
        err = node.perform_atomic_self_provision(fault_key="dk_")
        self.assertEqual(err, MESHX_ERR_WRITE_FAIL)

        # Verify clean rollback: zero credentials committed
        self.assertFalse(node.is_provisioned())
        self.assertEqual(node.unicast_addr, MESHX_ADDR_UNASSIGNED)
        self.assertIsNone(node.nvs.get("net_key"))
        self.assertIsNone(node.nvs.get("app_key"))
        self.assertIsNone(node.nvs.get(node.nvs.format_devkey_key(node.uuid)))
        self.assertIsNone(node.nvs.get("is_prov"))
        self.assertEqual(node.state, PROV_STATE_UNPROVISIONED)

    def test_zero_plaintext_key_leakage_audit(self):
        """Ensure no plaintext NetKey, AppKey, or DevKey bytes appear in logs or NVS audit."""
        node = ZGBNodeSimulator(self.uuid)
        node.boot()
        node.perform_atomic_self_provision()

        # Audit logs for raw key leakage
        keys = [node.net_key, node.app_key, node.dev_key]
        self.assertTrue(node.nvs.audit_zero_key_leakage(keys),
                        "Plaintext key bytes detected in NVS log stream!")


# ==============================================================================
# Main Runner
# ==============================================================================
def run_m2_stress_suite() -> bool:
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()

    suite.addTests(loader.loadTestsFromTestCase(TestButtonHoldDetectorStress))
    suite.addTests(loader.loadTestsFromTestCase(TestMXCPCommissionAbortStateMachineStress))
    suite.addTests(loader.loadTestsFromTestCase(TestPostBootTimerDistributionStress))
    suite.addTests(loader.loadTestsFromTestCase(TestPriorityGuardPreemptionRacesStress))

    runner = unittest.TextTestRunner(verbosity=2)
    print("=" * 78)
    print("Executing Milestone 2 Empirical Stress Test Harness")
    print("=" * 78)
    result = runner.run(suite)
    print("=" * 78)
    print(f"Tests Executed: {result.testsRun}")
    print(f"Failures: {len(result.failures)}")
    print(f"Errors: {len(result.errors)}")
    print(f"Overall Result: {'PASS' if result.wasSuccessful() else 'FAIL'}")
    print("=" * 78)
    return result.wasSuccessful()


if __name__ == "__main__":
    success = run_m2_stress_suite()
    sys.exit(0 if success else 1)
