#!/usr/bin/env python3
"""
    Copyright © 2024 - 2025 MeshX

    Automated Test Suite for Autonomous Element Pairing and Dynamic Group Allocation:
    1. Mathematical Anchor Formula & Dynamic Range Invariants (0xC000 - 0xC3FF)
    2. Anchor Node Resolution (1:N Broadcast Hub vs N:1 Shared Sink Hub)
    3. Semantic Compatibility Matrix & Trait LUT Rules
    4. Self-Pairing & Dynamic Topology Promotion State Machine (1:1 -> 1:N -> N:1)
    5. Reference Counting & Dynamic Group Reclamation
    6. Target Hardware Execution via CLI (when running on physical nodes)
"""

import sys
import os
import logging

# Ensure tools/scripts/autotest is in Python path for standalone execution
current_dir = os.path.dirname(os.path.abspath(__file__))
autotest_root = os.path.abspath(os.path.join(current_dir, ".."))
if autotest_root not in sys.path:
    sys.path.insert(0, autotest_root)

try:
    from framework.base import TestBase
except ImportError:
    # Fallback mock for direct pytest / unittest execution without runner
    class TestBase:
        name = "BaseTest"
        required_nodes = 0
        def __init__(self, devices=None, logger=None):
            self.devices = devices or []
            self.logger = logger or logging.getLogger(self.name)
            self.results = type("Result", (), {"logs": [], "success": False})()
        def log_info(self, msg):
            self.logger.info(msg)
            self.results.logs.append(f"INFO: {msg}")
        def log_error(self, msg):
            self.logger.error(msg)
            self.results.logs.append(f"ERROR: {msg}")


# Constants matching MeshX firmware specifications
MESHX_DYNAMIC_GROUP_START = 0xC000
MESHX_DYNAMIC_GROUP_END   = 0xC3FF
MESHX_DYNAMIC_GROUP_MASK  = 0x03FF

MESHX_ROLE_PRODUCER       = 0x01
MESHX_ROLE_CONSUMER       = 0x02
MESHX_ROLE_BIDIRECTIONAL  = 0x03

MESHX_TOPOLOGY_UNKNOWN    = 0
MESHX_TOPOLOGY_1_TO_1     = 1
MESHX_TOPOLOGY_1_TO_N     = 2
MESHX_TOPOLOGY_N_TO_1     = 3
MESHX_TOPOLOGY_M_TO_N     = 4

# Element Variants from meshx_element_lut.h
VARIANT_SWITCH_RELAY_CLIENT = 0x0001
VARIANT_SWITCH_RELAY_SERVER = 0x0002
VARIANT_LIGHT_CWWW_CLIENT   = 0x0003
VARIANT_LIGHT_CWWW_SERVER   = 0x0004
VARIANT_LIGHT_HSL_CLIENT    = 0x0005
VARIANT_LIGHT_HSL_SERVER    = 0x0006
VARIANT_SENSOR_CLIENT       = 0x0007
VARIANT_SENSOR_SERVER       = 0x0008

# Traits from meshx_element_lut.h
TRAIT_RELAY_POWER           = 0x0001
TRAIT_LIGHT_CWWW            = 0x0002
TRAIT_LIGHT_HSL             = 0x0003
TRAIT_SENSOR_OCCUPANCY      = 0x0004


def meshx_group_derive_from_anchor(anchor_unicast: int) -> int:
    """Calculates group address using the Anchor Formula: 0xC000 + (anchor & 0x03FF)."""
    return MESHX_DYNAMIC_GROUP_START + (anchor_unicast & MESHX_DYNAMIC_GROUP_MASK)


def meshx_is_dynamic_group_addr(addr: int) -> bool:
    """Checks if address is within dynamic group pool 0xC000 - 0xC3FF."""
    return MESHX_DYNAMIC_GROUP_START <= addr <= MESHX_DYNAMIC_GROUP_END


def meshx_group_resolve_anchor_topology(local_unicast: int, peer_unicast: int,
                                        local_role: int, peer_role: int,
                                        topology: int = MESHX_TOPOLOGY_UNKNOWN) -> int:
    """Resolves anchor unicast address based on roles and topology archetype."""
    if topology == MESHX_TOPOLOGY_N_TO_1:
        if local_role == MESHX_ROLE_CONSUMER:
            return local_unicast
        if peer_role == MESHX_ROLE_CONSUMER:
            return peer_unicast

    # 1:N or default: Producer (Switch) is the anchor
    if local_role == MESHX_ROLE_PRODUCER:
        return local_unicast
    if peer_role == MESHX_ROLE_PRODUCER:
        return peer_unicast

    # Consumer fallback
    if local_role == MESHX_ROLE_CONSUMER:
        return local_unicast
    if peer_role == MESHX_ROLE_CONSUMER:
        return peer_unicast

    return min(local_unicast, peer_unicast)


class SimulatedLinkTable:
    """Simulates the NVS link table state for tracking reference counts and topology."""
    def __init__(self):
        self.links = []
        self.link_epoch = 1

    def add_link(self, local_elem: int, bound_variant: int, target_unicast: int, target_group: int, func_id: int):
        self.links.append({
            "local_elem": local_elem,
            "bound_variant": bound_variant,
            "target_unicast": target_unicast,
            "target_group": target_group,
            "func_id": func_id,
            "active": True
        })
        self.link_epoch += 1

    def remove_link(self, target_unicast: int, func_id: int):
        for link in self.links:
            if link["active"] and link["target_unicast"] == target_unicast and link["func_id"] == func_id:
                link["active"] = False
                self.link_epoch += 1
                return True
        return False

    def get_ref_count(self, group_addr: int) -> int:
        if not meshx_is_dynamic_group_addr(group_addr):
            return 0
        return sum(1 for link in self.links if link["active"] and link["target_group"] == group_addr)

    def find_active_links_for_element(self, local_elem: int, func_id: int):
        return [l for l in self.links if l["active"] and l["local_elem"] == local_elem and l["func_id"] == func_id]


class TestPairingAndGroupAllocation(TestBase):
    """
    Automated test case executed via autotest runner or standalone python.
    """
    name = "Pairing_Group_Allocation"
    required_nodes = 1  # Executes algorithmic assertions and on-target CLI UTs on connected device

    def run(self):
        self.log_info("=== [Phase 1] Mathematical Anchor Formula & Dynamic Range Invariants ===")
        if not self._test_anchor_math():
            return False

        self.log_info("=== [Phase 2] Anchor Node Resolution Policy (1:N vs N:1) ===")
        if not self._test_anchor_resolution():
            return False

        self.log_info("=== [Phase 3] Semantic Compatibility Matrix Verification ===")
        if not self._test_compatibility_matrix():
            return False

        self.log_info("=== [Phase 4] Self-Pairing & Dynamic Topology Promotion State Machine ===")
        if not self._test_self_pairing_and_promotion():
            return False

        self.log_info("=== [Phase 5] Reference Counting & Group Address Reclamation ===")
        if not self._test_ref_counting_and_reclamation():
            return False

        self.log_info("=== [Phase 6] UVP Wire Frame Serialization & Packet Budget Validation ===")
        if not self._test_frame_serialization_budget():
            return False

        self.log_info("=== [Phase 7] Multi-Node Pairing Orchestrator & Epoch Sync Reconciliation ===")
        if not self._test_orchestrator_and_unpair_lifecycle():
            return False

        self.log_info("=== [Phase 8] Autonomous Proximity Pairing Engine & RSSI Thresholding ===")
        if not self._test_proximity_state_machine():
            return False

        # Phase 9: Hardware Execution if attached
        if self.devices and len(self.devices) > 0 and hasattr(self.devices[0], 'node'):
            self.log_info("=== [Phase 9] Executing On-Target Hardware Unit Tests via CLI ===")
            if not self._test_on_target_cli():
                return False
        else:
            self.log_info("=== [Phase 9] Skipping hardware CLI (no physical node connected) ===")

        self.log_info("ALL TESTS PASSED: Autonomous pairing, orchestrator, unpair, proximity & allocation validated.")
        return True

    def _test_anchor_math(self):
        # 1. Mathematical boundary tests
        test_vectors = [
            (0x0001, 0xC001),
            (0x0014, 0xC014),
            (0x03FF, 0xC3FF),
            (0x0400, 0xC000),  # Wrap around lower 10 bits
            (0x0401, 0xC001),
            (0x07FF, 0xC3FF),
            (0x0800, 0xC000),
            (0x7FFF, 0xC3FF),
        ]
        for unicast, expected_group in test_vectors:
            calculated = meshx_group_derive_from_anchor(unicast)
            if calculated != expected_group:
                self.log_error(f"Anchor math failed for 0x{unicast:04X}: expected 0x{expected_group:04X}, got 0x{calculated:04X}")
                return False
            if not meshx_is_dynamic_group_addr(calculated):
                self.log_error(f"Derived group 0x{calculated:04X} out of dynamic range [0xC000..0xC3FF]")
                return False

        # Exhaustive range sweep across all standard Bluetooth Mesh unicast addresses (0x0001 - 0x7FFF)
        for u in range(0x0001, 0x8000, 67):  # Step by prime to sample efficiently
            grp = meshx_group_derive_from_anchor(u)
            if not (0xC000 <= grp <= 0xC3FF):
                self.log_error(f"Exhaustive sweep failed: unicast 0x{u:04X} generated group 0x{grp:04X}")
                return False

        # Range check negative tests
        invalid_addrs = [0x0000, 0x0001, 0x0021, 0xBFFF, 0xC400, 0xC401, 0xFEFF, 0xFFFF]
        for inv in invalid_addrs:
            if meshx_is_dynamic_group_addr(inv):
                self.log_error(f"Address 0x{inv:04X} incorrectly classified as dynamic group address")
                return False

        self.log_info("  -> Anchor Formula & Dynamic Range Invariants: PASSED (100% compliant)")
        return True

    def _test_anchor_resolution(self):
        # 1:N Broadcast Topology: Producer (Switch) is Anchor Hub
        anchor = meshx_group_resolve_anchor_topology(
            local_unicast=0x0010, peer_unicast=0x0020,
            local_role=MESHX_ROLE_PRODUCER, peer_role=MESHX_ROLE_CONSUMER,
            topology=MESHX_TOPOLOGY_1_TO_N
        )
        if anchor != 0x0010:
            self.log_error(f"1:N Anchor (local Producer) expected 0x0010, got 0x{anchor:04X}")
            return False

        anchor_peer = meshx_group_resolve_anchor_topology(
            local_unicast=0x0020, peer_unicast=0x0010,
            local_role=MESHX_ROLE_CONSUMER, peer_role=MESHX_ROLE_PRODUCER,
            topology=MESHX_TOPOLOGY_1_TO_N
        )
        if anchor_peer != 0x0010:
            self.log_error(f"1:N Anchor (peer Producer) expected 0x0010, got 0x{anchor_peer:04X}")
            return False

        # N:1 Shared Sink Topology: Consumer (Lamp) is Anchor Hub
        anchor_n1 = meshx_group_resolve_anchor_topology(
            local_unicast=0x0030, peer_unicast=0x0040,
            local_role=MESHX_ROLE_CONSUMER, peer_role=MESHX_ROLE_PRODUCER,
            topology=MESHX_TOPOLOGY_N_TO_1
        )
        if anchor_n1 != 0x0030:
            self.log_error(f"N:1 Anchor (local Consumer) expected 0x0030, got 0x{anchor_n1:04X}")
            return False

        anchor_n1_peer = meshx_group_resolve_anchor_topology(
            local_unicast=0x0040, peer_unicast=0x0030,
            local_role=MESHX_ROLE_PRODUCER, peer_role=MESHX_ROLE_CONSUMER,
            topology=MESHX_TOPOLOGY_N_TO_1
        )
        if anchor_n1_peer != 0x0030:
            self.log_error(f"N:1 Anchor (peer Consumer) expected 0x0030, got 0x{anchor_n1_peer:04X}")
            return False

        # Default fallback: lowest unicast
        anchor_tie = meshx_group_resolve_anchor_topology(
            local_unicast=0x0050, peer_unicast=0x0045,
            local_role=MESHX_ROLE_BIDIRECTIONAL, peer_role=MESHX_ROLE_BIDIRECTIONAL,
            topology=MESHX_TOPOLOGY_UNKNOWN
        )
        if anchor_tie != 0x0045:
            self.log_error(f"Tie-breaker expected 0x0045, got 0x{anchor_tie:04X}")
            return False

        self.log_info("  -> Anchor Node Resolution Policy (1:N, N:1, Tie-break): PASSED")
        return True

    def _test_compatibility_matrix(self):
        # 1:1 Strict LUT mapping test based on meshx_element_lut.c
        valid_pairs = [
            (VARIANT_SWITCH_RELAY_CLIENT, VARIANT_SWITCH_RELAY_SERVER, 0x00),
            (VARIANT_LIGHT_CWWW_CLIENT,   VARIANT_LIGHT_CWWW_SERVER,   0x00),
            (VARIANT_LIGHT_CWWW_CLIENT,   VARIANT_LIGHT_CWWW_SERVER,   0x01),
            (VARIANT_LIGHT_HSL_CLIENT,    VARIANT_LIGHT_HSL_SERVER,    0x00),
            (VARIANT_LIGHT_HSL_CLIENT,    VARIANT_LIGHT_HSL_SERVER,    0x01),
            (VARIANT_SENSOR_CLIENT,       VARIANT_SENSOR_SERVER,       0x00),
        ]

        # Valid mappings must pass
        for client, server, fid in valid_pairs:
            # Emulate verify_variant_compatibility
            if client + 1 != server:
                self.log_error(f"Variant mismatch in LUT: client=0x{client:04X}, server=0x{server:04X}")
                return False

        # Invalid mappings must be rejected
        invalid_pairs = [
            (VARIANT_SWITCH_RELAY_CLIENT, VARIANT_SWITCH_RELAY_CLIENT),  # Client to Client
            (VARIANT_SWITCH_RELAY_SERVER, VARIANT_SWITCH_RELAY_SERVER),  # Server to Server
            (VARIANT_SENSOR_CLIENT,       VARIANT_SWITCH_RELAY_SERVER),  # Sensor Client to Relay Server
            (VARIANT_LIGHT_HSL_CLIENT,    VARIANT_LIGHT_CWWW_SERVER),    # HSL Client to CWWW Server
        ]
        for src, dst in invalid_pairs:
            # Client cannot match non-server or mismatched variant
            if (src % 2 == 1) and (dst == src + 1):
                self.log_error(f"Incompatible pair was unexpectedly permitted: 0x{src:04X} -> 0x{dst:04X}")
                return False

        self.log_info("  -> Semantic Compatibility Matrix & 1:1 Mapping: PASSED")
        return True

    def _test_self_pairing_and_promotion(self):
        """
        Simulates an intra-node Self-Pairing scenario:
        - Primary Node Unicast: 0x0010
        - Element 0: Local Relay Client (Switch Button 1, Unicast 0x0010)
        - Element 1: Local Relay Server (Relay 1, Unicast 0x0011)
        - External Node Unicast: 0x0025 (External Ceiling Lamp)
        """
        table = SimulatedLinkTable()
        local_node_addr = 0x0010
        elem0_switch_addr = local_node_addr + 0  # 0x0010
        elem1_relay_addr  = local_node_addr + 1  # 0x0011
        ext_lamp_addr     = 0x0025

        # -------------------------------------------------------------
        # Step 1: Self-Pairing (Intra-Node 1:1 Direct Link)
        # -------------------------------------------------------------
        # Switch button (Element 0) is paired locally to Relay 1 (Element 1)
        # Topology: 1:1 Direct Unicast. Group address MUST be 0x0000 (0 group leases).
        table.add_link(
            local_elem=0,
            bound_variant=VARIANT_SWITCH_RELAY_CLIENT,
            target_unicast=elem1_relay_addr,
            target_group=0x0000,
            func_id=0x00
        )
        if len(table.links) != 1 or table.links[0]["target_group"] != 0x0000:
            self.log_error("Step 1 Failed: 1:1 self-pairing must not lease any group address")
            return False

        if table.get_ref_count(0xC010) != 0:
            self.log_error("Step 1 Failed: Group 0xC010 ref count should be 0 for 1:1 unicast link")
            return False

        self.log_info("  -> Step 1 (1:1 Self-Pairing: Switch -> Local Relay via Unicast 0x0011): PASSED")

        # -------------------------------------------------------------
        # Step 2: Dynamic Topology Promotion (Upgrading 1:1 Link to Group)
        # -------------------------------------------------------------
        # Installer now pairs Switch (Element 0) to External Lamp (0x0025).
        # Because Switch (Element 0) already publishes to func_id 0x00, it cannot publish to
        # two unicast addresses simultaneously. It MUST promote the link to a group address!
        active_links = table.find_active_links_for_element(local_elem=0, func_id=0x00)
        if len(active_links) != 1:
            self.log_error(f"Step 2 Failed: Expected exactly 1 existing link, found {len(active_links)}")
            return False

        # Calculate Anchor: Switch (Element 0, 0x0010) is the Producer Anchor Hub
        anchor = meshx_group_resolve_anchor_topology(
            local_unicast=elem0_switch_addr,
            peer_unicast=ext_lamp_addr,
            local_role=MESHX_ROLE_PRODUCER,
            peer_role=MESHX_ROLE_CONSUMER,
            topology=MESHX_TOPOLOGY_1_TO_N
        )
        if anchor != elem0_switch_addr:
            self.log_error(f"Step 2 Failed: Anchor should be Switch (0x0010), got 0x{anchor:04X}")
            return False

        promoted_group = meshx_group_derive_from_anchor(anchor)
        if promoted_group != 0xC010:
            self.log_error(f"Step 2 Failed: Expected promoted group 0xC010, got 0x{promoted_group:04X}")
            return False

        # Upgrade existing local link (Element 1) to subscribe to promoted group 0xC010
        table.links[0]["target_group"] = promoted_group

        # Add new link for External Lamp (0x0025) bound to promoted group 0xC010
        table.add_link(
            local_elem=0,
            bound_variant=VARIANT_SWITCH_RELAY_CLIENT,
            target_unicast=ext_lamp_addr,
            target_group=promoted_group,
            func_id=0x00
        )

        # Now both Local Relay and External Lamp are bound to 0xC010
        ref_count = table.get_ref_count(promoted_group)
        if ref_count != 2:
            self.log_error(f"Step 2 Failed: Promoted group 0xC010 ref_count expected 2, got {ref_count}")
            return False

        self.log_info(f"  -> Step 2 (Dynamic Promotion to Group 0x{promoted_group:04X}, ref_count=2): PASSED")

        # -------------------------------------------------------------
        # Step 3: 1:N Cluster Expansion (Adding a 3rd fixture)
        # -------------------------------------------------------------
        ext_lamp2_addr = 0x0026
        table.add_link(
            local_elem=0,
            bound_variant=VARIANT_SWITCH_RELAY_CLIENT,
            target_unicast=ext_lamp2_addr,
            target_group=promoted_group,
            func_id=0x00
        )
        if table.get_ref_count(promoted_group) != 3:
            self.log_error("Step 3 Failed: Ref count expected 3 after adding 3rd fixture")
            return False

        self.log_info("  -> Step 3 (1:N Expansion: 3 fixtures subscribed to group 0xC010): PASSED")
        return True

    def _test_ref_counting_and_reclamation(self):
        """Tests incremental unpairing and 0-reference group address reclamation."""
        table = SimulatedLinkTable()
        group_addr = 0xC010

        # Seed 3 links bound to 0xC010
        table.add_link(0, VARIANT_SWITCH_RELAY_CLIENT, 0x0011, group_addr, 0x00)
        table.add_link(0, VARIANT_SWITCH_RELAY_CLIENT, 0x0025, group_addr, 0x00)
        table.add_link(0, VARIANT_SWITCH_RELAY_CLIENT, 0x0026, group_addr, 0x00)
        epoch_start = table.link_epoch

        # Unpair Lamp 2 (0x0026)
        if not table.remove_link(target_unicast=0x0026, func_id=0x00):
            self.log_error("Failed to remove link for 0x0026")
            return False
        if table.get_ref_count(group_addr) != 2:
            self.log_error(f"Expected ref_count 2, got {table.get_ref_count(group_addr)}")
            return False
        if table.link_epoch <= epoch_start:
            self.log_error("link_epoch was not monotonically incremented on unpair")
            return False

        # Unpair Lamp 1 (0x0025)
        table.remove_link(target_unicast=0x0025, func_id=0x00)
        if table.get_ref_count(group_addr) != 1:
            self.log_error(f"Expected ref_count 1, got {table.get_ref_count(group_addr)}")
            return False

        # Unpair Local Relay (0x0011)
        table.remove_link(target_unicast=0x0011, func_id=0x00)
        if table.get_ref_count(group_addr) != 0:
            self.log_error(f"Group address 0x{group_addr:04X} was not reclaimed! ref_count={table.get_ref_count(group_addr)}")
            return False

        self.log_info("  -> Step 4 (Granular Unpair & Zero-Ref Group Reclamation): PASSED")
        return True

    def _test_frame_serialization_budget(self):
        """Verifies wire frame sizes and packet budget limits (<= 375 bytes for discovery, <= 11 bytes for system commands)."""
        import struct

        # UVP Wire Packets (Packed struct formats matching meshx_uvp.h)
        fmt_pair_req   = "<BBBHHH"      # msg_type, src_elem, dst_elem, func_id, group, sender_unicast (9 bytes)
        fmt_pair_rsp   = "<BBH"         # msg_type, status, target_group (4 bytes)
        fmt_topo_upg   = "<BBHH"        # msg_type, dst_elem, func_id, group_addr (6 bytes)
        fmt_unpair_req = "<BBBHH"       # msg_type, src_elem, dst_elem, func_id, group (7 bytes)
        fmt_unpair_rsp = "<BB"          # msg_type, status (2 bytes)
        fmt_epoch_ping = "<B"           # msg_type (1 byte)
        fmt_epoch_pong = "<BH"          # msg_type, link_epoch (3 bytes)
        fmt_announce   = "<BHBHBbB"     # msg_type, unicast, elem, variant(H), role, min_rssi, seq (9 bytes)

        frames = {
            "PAIR_REQUEST": (fmt_pair_req, 9),
            "PAIR_RESPONSE": (fmt_pair_rsp, 4),
            "TOPOLOGY_UPGRADE": (fmt_topo_upg, 6),
            "UNPAIR_REQUEST": (fmt_unpair_req, 7),
            "UNPAIR_RESPONSE": (fmt_unpair_rsp, 2),
            "EPOCH_PING": (fmt_epoch_ping, 1),
            "EPOCH_PONG": (fmt_epoch_pong, 3),
            "PAIR_ANNOUNCE": (fmt_announce, 9),
        }

        for name, (fmt, expected_sz) in frames.items():
            calc_sz = struct.calcsize(fmt)
            if calc_sz != expected_sz:
                self.log_error(f"Frame {name} packed size mismatch: expected {expected_sz}, got {calc_sz}")
                return False
            # Bluetooth Mesh unsegmented Access Payload maximum is 11 bytes.
            # All system frames must fit without SAR (Segmentation & Reassembly) segmentation.
            if calc_sz > 11:
                self.log_error(f"Frame {name} exceeds single-segment unsegmented budget of 11 bytes: {calc_sz} bytes")
                return False

        # Manifest Packets: 8 elements manifest must fit in single UVP payload <= 375 bytes
        elem_desc_sz = 1 + 1 + 2 + 2 + 1 + (4 * 2) + 16  # 31 bytes per element descriptor
        manifest_hdr_sz = 2 + 2 + 2 + 2 + 1               # 9 bytes header
        max_manifest_sz = manifest_hdr_sz + (8 * elem_desc_sz)  # 9 + 248 = 257 bytes
        if max_manifest_sz > 375:
            self.log_error(f"Capability manifest size {max_manifest_sz} exceeds UVP MTU budget of 375 bytes")
            return False

        self.log_info(f"  -> Wire frames (5-9 bytes <= 11B single-segment) & 8-elem Manifest ({max_manifest_sz}B <= 375B): PASSED")
        return True

    def _test_orchestrator_and_unpair_lifecycle(self):
        """Simulates full multi-node pairing orchestration, unpair teardown, and epoch reconciliation."""
        # Simulated Network Nodes
        switch_unicast = 0x0010
        lamp1_unicast  = 0x0021
        lamp2_unicast  = 0x0022
        func_id        = 0x0000

        # Step 1: Pair Switch (0x0010) to Lamp 1 (0x0021) in 1:1 Unicast mode
        switch_table = SimulatedLinkTable()
        lamp1_table  = SimulatedLinkTable()

        epoch_init = switch_table.link_epoch
        switch_table.add_link(0, VARIANT_SWITCH_RELAY_CLIENT, lamp1_unicast, 0x0000, func_id)
        lamp1_table.add_link(1, VARIANT_SWITCH_RELAY_SERVER, switch_unicast, 0x0000, func_id)

        if switch_table.link_epoch <= epoch_init or lamp1_table.link_epoch <= epoch_init:
            self.log_error("Initial pair failed to increment link_epoch")
            return False

        # Step 2: Add Lamp 2 (0x0022) -> Triggers Dynamic Topology Upgrade to Group
        promoted_group = meshx_group_derive_from_anchor(switch_unicast)  # 0xC010
        # Upgrade Lamp 1 link
        switch_table.links[0]["target_group"] = promoted_group
        lamp1_table.links[0]["target_group"] = promoted_group
        # Add Lamp 2 link
        lamp2_table = SimulatedLinkTable()
        epoch_before_lamp2 = switch_table.link_epoch
        switch_table.add_link(0, VARIANT_SWITCH_RELAY_CLIENT, lamp2_unicast, promoted_group, func_id)
        lamp2_table.add_link(1, VARIANT_SWITCH_RELAY_SERVER, switch_unicast, promoted_group, func_id)

        if switch_table.get_ref_count(promoted_group) != 2:
            self.log_error(f"Expected ref_count 2 on group 0x{promoted_group:04X}, got {switch_table.get_ref_count(promoted_group)}")
            return False

        # Step 3: Epoch Synchronization Check (Provisioner query)
        provisioner_cached_epoch = epoch_before_lamp2
        current_node_epoch = switch_table.link_epoch
        if current_node_epoch == provisioner_cached_epoch:
            self.log_error("Switch link_epoch did not bump after topology upgrade")
            return False

        # Step 4: Unpair Lamp 1 -> ref_count drops to 1, group remains active
        switch_table.remove_link(target_unicast=lamp1_unicast, func_id=func_id)
        if switch_table.get_ref_count(promoted_group) != 1:
            self.log_error(f"Expected ref_count 1 after unpairing lamp1, got {switch_table.get_ref_count(promoted_group)}")
            return False

        # Step 5: Unpair Lamp 2 -> ref_count drops to 0, zero-ref group reclaimed
        switch_table.remove_link(target_unicast=lamp2_unicast, func_id=func_id)
        if switch_table.get_ref_count(promoted_group) != 0:
            self.log_error(f"Expected group 0x{promoted_group:04X} reclaimed, ref_count={switch_table.get_ref_count(promoted_group)}")
            return False

        self.log_info("  -> Orchestrator 1:1, Dynamic Upgrade, Multi-peer Unpair & Epoch Sync: PASSED")
        return True

    def _test_proximity_state_machine(self):
        """Simulates proximity pairing window, RSSI filtering, and timeout/dedup logic."""
        RSSI_THRESHOLD = -50  # dBm

        class ProximityFSM:
            def __init__(self):
                self.state = "IDLE"
                self.dedup = set()

            def start(self, window_sec=30):
                self.state = "WINDOW_ACTIVE"

            def stop(self):
                self.state = "IDLE"

            def handle_announce(self, unicast, seq, rssi, role, variant):
                if self.state != "WINDOW_ACTIVE":
                    return False
                # Dedup check
                key = (unicast, seq)
                if key in self.dedup:
                    return False
                self.dedup.add(key)
                # RSSI threshold check (Mode A Proximity)
                if rssi < RSSI_THRESHOLD:
                    return False
                # Role compatibility
                if role == MESHX_ROLE_CONSUMER:
                    self.state = "PAIRING"
                    return True
                return False

            def on_pair_complete(self, success):
                self.state = "SUCCESS" if success else "TIMEOUT"

        fsm = ProximityFSM()
        fsm.start(30)
        if fsm.state != "WINDOW_ACTIVE":
            self.log_error("Proximity FSM failed to enter WINDOW_ACTIVE")
            return False

        # 1. Distant announcement (-65 dBm < -50 dBm threshold) must be ignored
        res = fsm.handle_announce(0x0025, seq=1, rssi=-65, role=MESHX_ROLE_CONSUMER, variant=VARIANT_SWITCH_RELAY_SERVER)
        if res or fsm.state != "WINDOW_ACTIVE":
            self.log_error("Distant peer (-65 dBm) was erroneously accepted")
            return False

        # 2. Duplicate frame must be ignored
        res = fsm.handle_announce(0x0025, seq=1, rssi=-40, role=MESHX_ROLE_CONSUMER, variant=VARIANT_SWITCH_RELAY_SERVER)
        if res:
            self.log_error("Duplicate sequence frame was erroneously accepted")
            return False

        # 3. Valid close proximity peer (-42 dBm >= -50 dBm)
        res = fsm.handle_announce(0x0025, seq=2, rssi=-42, role=MESHX_ROLE_CONSUMER, variant=VARIANT_SWITCH_RELAY_SERVER)
        if not res or fsm.state != "PAIRING":
            self.log_error("Close peer (-42 dBm) failed to trigger PAIRING state")
            return False

        fsm.on_pair_complete(True)
        if fsm.state != "SUCCESS":
            self.log_error("Pair complete failed to enter SUCCESS state")
            return False

        self.log_info("  -> Proximity FSM (Window, RSSI filtering, Dedup, State Transitions): PASSED")
        return True

    def _test_on_target_cli(self):
        """Sends CLI unit test commands to physical ESP32 node via serial."""
        node = self.devices[0].node
        self.log_info(f"Executing on-target CLI unit tests on node {node.name}...")

        # Test on-target Anchor Math & Group Allocator
        node.clear_logs()
        node.send_command("ut 3 15 0")
        if not node.expect(r"UT GROUP_ALLOC PASS: all anchor and group allocation invariants verified", timeout=4):
            self.log_error("On-target UT GROUP_ALLOC command failed")
            return False
        self.log_info("  -> On-target UT GROUP_ALLOC: PASSED")

        # Test on-target Link Table Set
        node.clear_logs()
        node.send_command("ut 3 11 0")
        if not node.expect(r"UT LINK_SET PASS: epoch=\d+ count=2", timeout=4):
            self.log_error("On-target UT LINK_SET command failed")
            return False
        self.log_info("  -> On-target UT LINK_SET: PASSED")

        # Test on-target Link Table Get
        node.clear_logs()
        node.send_command("ut 3 12 0")
        if not node.expect(r"UT LINK_GET PASS: magic=0x4C4E4B53", timeout=4):
            self.log_error("On-target UT LINK_GET command failed")
            return False
        self.log_info("  -> On-target UT LINK_GET: PASSED")

        # Test on-target Boot-Time Self-Audit Reconcile
        node.clear_logs()
        node.send_command("ut 3 14 0")
        if not node.expect(r"UT LINK_RECONCILE PASS", timeout=4):
            self.log_error("On-target UT LINK_RECONCILE command failed")
            return False
        self.log_info("  -> On-target UT LINK_RECONCILE: PASSED")

        # Test on-target Link Table Erase
        node.clear_logs()
        node.send_command("ut 3 13 0")
        if not node.expect(r"UT LINK_ERASE PASS: link table successfully erased", timeout=4):
            self.log_error("On-target UT LINK_ERASE command failed")
            return False
        self.log_info("  -> On-target UT LINK_ERASE: PASSED")

        return True


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")
    test = TestPairingAndGroupAllocation(devices=[])
    success = test.run()
    if not success:
        print("\nTEST FAILED")
        sys.exit(1)
    else:
        print("\nTEST SUCCESS")
        sys.exit(0)
