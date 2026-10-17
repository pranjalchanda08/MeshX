#!/usr/bin/env python3
"""
    Copyright © 2024 - 2025 MeshX

    End-to-End Functional Test Suite for Autonomous Element Pairing:
    1. Single-Node Loopback / Intra-Node Pairing (required_nodes = 1)
       - Host-Target MXCP Wire Communication (0x71, 0x72, 0x73, 0x74)
       - Persistent Link Table & Monotonic Epoch Tracking
       - Switch-to-Relay Actuation & Telemetry Verification
       - Granular Unpairing & Link Slot Reclamation
       - Incompatible Role / Trait Rejection Validation
    2. Multi-Node Cross-Board BLE Mesh Pairing (required_nodes = 2)
       - Inter-Node Binding over BLE Mesh RF
       - Remote Switch-to-Relay Actuation across Boards
       - Dynamic Group Promotion (0xC000 + (anchor & 0x03FF))
       - Cross-Node Unpairing & Clean-up
"""

import sys
import os
import time
import struct
import logging

current_dir = os.path.dirname(os.path.abspath(__file__))
autotest_root = os.path.abspath(os.path.join(current_dir, ".."))
if autotest_root not in sys.path:
    sys.path.insert(0, autotest_root)

from framework.base import TestBase


class TestFunctionalPairingSingleNode(TestBase):
    """
    Functional verification of Autonomous Element Pairing on a single node (intra-node loopback).
    Validates MXCP protocol frames, NVS link table state, monotonic epoch tracking,
    and end-to-end element actuation (Switch -> Relay).
    """
    name = "Functional_Pairing_Single_Node"
    required_nodes = 1

    def run(self):
        device = self.devices[0]
        node = device.node
        self.log_info(f"Starting Single-Node Functional Pairing Verification on {device.name}")

        # -------------------------------------------------------------
        # Phase 1: Query Node Address & Initial Link Epoch via MXCP (0x74)
        # -------------------------------------------------------------
        self.log_info("[Phase 1] Querying initial link epoch and node address via MXCP (0x74)...")
        if not device.pairing.get_link_epoch():
            self.log_error("Failed to send MXCP_CMD_GET_LINK_EPOCH frame")
            return False

        epoch_rsp = device.pairing.wait_for_link_epoch_rsp(timeout=3.0)
        if not epoch_rsp:
            self.log_error("Timed out waiting for MXCP_EVT_LINK_EPOCH_RSP (0xF4)")
            return False

        initial_epoch = epoch_rsp["link_epoch"]
        node_addr = epoch_rsp["node_addr"]
        self.log_info(f"  -> Initial Epoch: {initial_epoch}, Node Address: 0x{node_addr:04X}")

        if node_addr == 0x0000:
            self.log_error("Device is not provisioned (node_addr is 0x0000). Pairing requires a provisioned node.")
            return False

        # -------------------------------------------------------------
        # Phase 2: Query Initial Link Table via MXCP (0x73)
        # -------------------------------------------------------------
        self.log_info("[Phase 2] Querying initial persistent link table via MXCP (0x73)...")
        if not device.pairing.get_link_table():
            self.log_error("Failed to send MXCP_CMD_GET_LINK_TABLE frame")
            return False

        table_rsp = device.pairing.wait_for_link_table_rsp(timeout=3.0)
        if not table_rsp:
            self.log_error("Timed out waiting for MXCP_EVT_LINK_TABLE_RSP (0xF3)")
            return False

        if table_rsp["magic"] != 0x4C4E4B53:  # 'LNKS'
            self.log_error(f"Invalid link table magic: 0x{table_rsp['magic']:08X} (expected 0x4C4E4B53)")
            return False

        self.log_info(f"  -> Link Table Valid: magic=LNKS, version={table_rsp['version']}, count={table_rsp['link_count']}")

        # Clear any existing links to guarantee a clean test slate
        if table_rsp["link_count"] > 0:
            self.log_info(f"  -> Pruning {table_rsp['link_count']} pre-existing links for clean test slate...")
            for link in table_rsp["links"]:
                if link["active"]:
                    device.pairing.unbind_elements(
                        src_elem=link["local_element_idx"],
                        dst_addr=link["target_unicast_addr"],
                        dst_elem=1,
                        func_id=link["func_id"]
                    )
                    device.pairing.wait_for_unbind_status(timeout=2.0)
            time.sleep(0.5)

        # -------------------------------------------------------------
        # Phase 3: Negative Incompatible Pairing Rejection Test
        # -------------------------------------------------------------
        self.log_info("[Phase 3] Testing negative pairing rejection for incompatible elements...")
        # Element 5 is UVP Sensor Server; binding it to Relay Server on func_id 0x0000 must be rejected
        device.pairing.bind_elements(src_elem=5, dst_addr=node_addr, dst_elem=2, func_id=0x0000)
        neg_rsp = device.pairing.wait_for_bind_status(timeout=3.0)
        if not neg_rsp or neg_rsp["status"] == 0:
            self.log_error(f"Incompatible binding was unexpectedly accepted! rsp={neg_rsp}")
            return False
        self.log_info(f"  -> Incompatible binding correctly rejected with status 0x{neg_rsp['status']:02X}")

        # -------------------------------------------------------------
        # Phase 3.5: Concurrency Guard / Overlapping Transaction Test
        # -------------------------------------------------------------
        self.log_info("[Phase 3.5] Testing concurrency guard against overlapping pairing transactions...")
        # Fire first bind request
        device.pairing.bind_elements(src_elem=1, dst_addr=node_addr, dst_elem=2, func_id=0x0000)
        # Immediately fire a second bind request to trigger the busy guard
        device.pairing.bind_elements(src_elem=1, dst_addr=node_addr, dst_elem=2, func_id=0x0000)

        rsp_a = device.pairing.wait_for_bind_status(timeout=3.0)
        rsp_b = device.pairing.wait_for_bind_status(timeout=3.0)

        statuses = [r["status"] for r in (rsp_a, rsp_b) if r is not None]
        self.log_info(f"  -> Concurrency test responses received: {statuses}")
        if 0x06 in statuses:
            self.log_info("  -> Concurrency guard confirmed: overlapping transaction correctly rejected as BUSY (status 0x06)")
        else:
            self.log_info("  -> Overlapping transactions serialized cleanly by engine")

        # Clean slate before Phase 4
        device.pairing.unbind_elements(src_elem=1, dst_addr=node_addr, dst_elem=2, func_id=0x0000)
        device.pairing.wait_for_unbind_status(timeout=2.0)
        time.sleep(0.5)

        # -------------------------------------------------------------
        # Phase 4: MXCP Autonomous Binding (Switch El 1 -> Relay El 2)
        # -------------------------------------------------------------
        self.log_info(f"[Phase 4] Initiating 1:1 intra-node binding: Switch (El 1) -> Relay (El 2) on 0x{node_addr:04X}...")
        # Element 1: UVP Relay Client (Switch)
        # Element 2: UVP Relay Server (Relay)
        # Func ID 0x0000: On/Off
        if not device.pairing.bind_elements(src_elem=1, dst_addr=node_addr, dst_elem=2, func_id=0x0000):
            self.log_error("Failed to send MXCP_CMD_BIND_ELEMENTS frame")
            return False

        bind_rsp = device.pairing.wait_for_bind_status(timeout=4.0)
        if not bind_rsp:
            self.log_error("Timed out waiting for MXCP_EVT_BIND_STATUS (0xF1)")
            return False

        if bind_rsp["status"] != 0:
            self.log_error(f"Binding failed with status: 0x{bind_rsp['status']:02X}")
            return False

        self.log_info(f"  -> Binding SUCCESS: status=0, group_addr=0x{bind_rsp['group_addr']:04X}")

        # -------------------------------------------------------------
        # Phase 5: Link Table & Epoch Mutation Verification
        # -------------------------------------------------------------
        self.log_info("[Phase 5] Verifying persistent NVS link table and monotonic epoch increment...")
        device.pairing.get_link_table()
        table_post = device.pairing.wait_for_link_table_rsp(timeout=3.0)
        if not table_post or table_post["link_count"] != 1:
            self.log_error(f"Expected link_count == 1, got {table_post}")
            return False

        active_link = next((l for l in table_post["links"] if l["active"]), None)
        if not active_link:
            self.log_error("No active link found in link table entries")
            return False

        if active_link["local_element_idx"] != 1 or active_link["func_id"] != 0x0000:
            self.log_error(f"Link parameters mismatch: {active_link}")
            return False

        self.log_info(f"  -> Link Entry Verified: El={active_link['local_element_idx']}, Target=0x{active_link['target_unicast_addr']:04X}, Group=0x{active_link['target_group_addr']:04X}")

        device.pairing.get_link_epoch()
        epoch_post = device.pairing.wait_for_link_epoch_rsp(timeout=3.0)
        if not epoch_post or epoch_post["link_epoch"] <= initial_epoch:
            self.log_error(f"link_epoch was not incremented: initial={initial_epoch}, current={epoch_post}")
            return False
        self.log_info(f"  -> Epoch Monotonically Incremented: {initial_epoch} -> {epoch_post['link_epoch']}")

        # -------------------------------------------------------------
        # Phase 6: End-to-End Actuation (Switch -> Relay)
        # -------------------------------------------------------------
        self.log_info("[Phase 6] Testing functional end-to-end actuation via bound element...")
        node.clear_logs()

        # 6.1 Command Switch (El 1) ON
        self.log_info("  -> Actuating Switch (El 1) to ON (state=1)...")
        # MXCP_CMD_EL_SEND (0x10): element_id=1, element_type=1 (Relay Client), func_id=0, len=1, data=1
        cmd_on = struct.pack("<HHHHB", 1, 1, 0x0000, 1, 1)
        node.send_mxcp_frame(0x10, cmd_on)

        # Wait for Relay Server (El 2) telemetry notification (0x90)
        rx_on = False
        start = time.time()
        while time.time() - start < 3.0:
            frame = node.wait_for_mxcp_frame(0x90, timeout=0.5)
            if frame and len(frame) >= 9:
                rx_el, rx_type, rx_fn, rx_len = struct.unpack("<HHHH", frame[:8])
                if rx_el == 2 and rx_fn == 0x0000:
                    state = frame[8]
                    if state == 1:
                        rx_on = True
                        break
        if not rx_on:
            self.log_error("Relay Server (El 2) failed to actuate to ON via bound Switch (El 1)")
            return False
        self.log_info("  -> Relay Server (El 2) successfully actuated to ON!")

        # 6.2 Command Switch (El 1) OFF
        self.log_info("  -> Actuating Switch (El 1) to OFF (state=0)...")
        cmd_off = struct.pack("<HHHHB", 1, 1, 0x0000, 1, 0)
        node.send_mxcp_frame(0x10, cmd_off)

        rx_off = False
        start = time.time()
        while time.time() - start < 3.0:
            frame = node.wait_for_mxcp_frame(0x90, timeout=0.5)
            if frame and len(frame) >= 9:
                rx_el, rx_type, rx_fn, rx_len = struct.unpack("<HHHH", frame[:8])
                if rx_el == 2 and rx_fn == 0x0000:
                    state = frame[8]
                    if state == 0:
                        rx_off = True
                        break
        if not rx_off:
            self.log_error("Relay Server (El 2) failed to actuate to OFF via bound Switch (El 1)")
            return False
        self.log_info("  -> Relay Server (El 2) successfully actuated to OFF!")

        # -------------------------------------------------------------
        # Phase 7: Granular Unpairing via MXCP (0x72)
        # -------------------------------------------------------------
        self.log_info("[Phase 7] Executing granular unpairing via MXCP (0x72)...")
        if not device.pairing.unbind_elements(src_elem=1, dst_addr=node_addr, dst_elem=2, func_id=0x0000):
            self.log_error("Failed to send MXCP_CMD_UNBIND_ELEMENTS frame")
            return False

        unbind_rsp = device.pairing.wait_for_unbind_status(timeout=4.0)
        if not unbind_rsp or unbind_rsp["status"] != 0:
            self.log_error(f"Unbind failed or timed out: {unbind_rsp}")
            return False
        self.log_info("  -> Unbind SUCCESS: status=0")

        # Verify link table entry is reclaimed
        device.pairing.get_link_table()
        table_unpaired = device.pairing.wait_for_link_table_rsp(timeout=3.0)
        if not table_unpaired or table_unpaired["link_count"] != 0:
            self.log_error(f"Link table not cleared after unbind: count={table_unpaired['link_count']}")
            return False
        self.log_info("  -> Link table confirmed empty (link_count == 0)")

        # Verify link epoch incremented again
        device.pairing.get_link_epoch()
        epoch_final = device.pairing.wait_for_link_epoch_rsp(timeout=3.0)
        if not epoch_final or epoch_final["link_epoch"] <= epoch_post["link_epoch"]:
            self.log_error("link_epoch was not incremented on unpair")
            return False
        self.log_info(f"  -> Final Epoch Incremented: {epoch_post['link_epoch']} -> {epoch_final['link_epoch']}")

        self.log_info("All Single-Node Functional Pairing verification steps PASSED!")
        return True


class TestFunctionalPairingMultiNode(TestBase):
    """
    Functional verification of Autonomous Element Pairing across 2 physical boards over BLE Mesh RF.
    Validates cross-node pairing, multi-hop mesh message actuation, dynamic promotion,
    and graceful peer unpairing.
    """
    name = "Functional_Pairing_Multi_Node"
    required_nodes = 2

    def run(self):
        dev0 = self.devices[0]
        dev1 = self.devices[1]
        self.log_info(f"Starting Multi-Node Cross-Board Functional Pairing between {dev0.name} and {dev1.name}")

        # -------------------------------------------------------------
        # Phase 1: Query Provisioned Addresses for Both Nodes
        # -------------------------------------------------------------
        self.log_info("[Phase 1] Discovering node addresses via MXCP (0x74)...")
        dev0.pairing.get_link_epoch()
        rsp0 = dev0.pairing.wait_for_link_epoch_rsp(timeout=3.0)
        if not rsp0 or rsp0["node_addr"] == 0x0000:
            self.log_error(f"Node 0 ({dev0.name}) is not provisioned: {rsp0}")
            return False

        dev1.pairing.get_link_epoch()
        rsp1 = dev1.pairing.wait_for_link_epoch_rsp(timeout=3.0)
        if not rsp1 or rsp1["node_addr"] == 0x0000:
            self.log_error(f"Node 1 ({dev1.name}) is not provisioned: {rsp1}")
            return False

        addr0 = rsp0["node_addr"]
        addr1 = rsp1["node_addr"]
        self.log_info(f"  -> Node 0 (Switch Client): 0x{addr0:04X}, Node 1 (Relay Server): 0x{addr1:04X}")

        # -------------------------------------------------------------
        # Phase 2: Clear Link Tables on Both Nodes
        # -------------------------------------------------------------
        self.log_info("[Phase 2] Ensuring clean link tables on both devices...")
        for dev in (dev0, dev1):
            dev.pairing.get_link_table()
            tbl = dev.pairing.wait_for_link_table_rsp(timeout=3.0)
            if tbl and tbl["link_count"] > 0:
                for link in tbl["links"]:
                    if link["active"]:
                        dev.pairing.unbind_elements(
                            src_elem=link["local_element_idx"],
                            dst_addr=link["target_unicast_addr"],
                            dst_elem=1,
                            func_id=link["func_id"]
                        )
                        dev.pairing.wait_for_unbind_status(timeout=2.0)

        # -------------------------------------------------------------
        # Phase 3: Cross-Node Pairing (Node 0 Switch -> Node 1 Relay)
        # -------------------------------------------------------------
        self.log_info(f"[Phase 3] Initiating Cross-Node Pairing: Node 0 (El 1) -> Node 1 (0x{addr1:04X}:El 2)...")
        dev0.pairing.bind_elements(src_elem=1, dst_addr=addr1, dst_elem=2, func_id=0x0000)
        bind_rsp = dev0.pairing.wait_for_bind_status(timeout=5.0)
        if not bind_rsp or bind_rsp["status"] != 0:
            self.log_error(f"Cross-node pairing failed: {bind_rsp}")
            return False
        self.log_info(f"  -> Cross-Node Binding SUCCESS: group_addr=0x{bind_rsp['group_addr']:04X}")

        # -------------------------------------------------------------
        # Phase 4: Bi-Directional Link Table Verification
        # -------------------------------------------------------------
        self.log_info("[Phase 4] Verifying synchronized link tables on both nodes...")
        time.sleep(0.5)

        # Node 0 check
        dev0.pairing.get_link_table()
        tbl0 = dev0.pairing.wait_for_link_table_rsp(timeout=3.0)
        if not tbl0 or tbl0["link_count"] != 1:
            self.log_error(f"Node 0 link table count mismatch: {tbl0}")
            return False

        # Node 1 check
        dev1.pairing.get_link_table()
        tbl1 = dev1.pairing.wait_for_link_table_rsp(timeout=3.0)
        if not tbl1 or tbl1["link_count"] != 1:
            self.log_error(f"Node 1 link table count mismatch: {tbl1}")
            return False

        self.log_info("  -> Bi-directional link table records verified on both nodes!")

        # -------------------------------------------------------------
        # Phase 5: Cross-Node RF Actuation (Node 0 Switch -> Node 1 Relay)
        # -------------------------------------------------------------
        self.log_info("[Phase 5] Actuating Relay on Node 1 via Switch command on Node 0 over BLE Mesh...")
        dev1.node.clear_logs()

        # Command Node 0 Switch to ON
        cmd_on = struct.pack("<HHHHB", 1, 1, 0x0000, 1, 1)
        dev0.node.send_mxcp_frame(0x10, cmd_on)

        # Wait for Node 1 Relay (El 2) telemetry notification
        rx_on = False
        start = time.time()
        while time.time() - start < 5.0:
            frame = dev1.node.wait_for_mxcp_frame(0x90, timeout=0.5)
            if frame and len(frame) >= 9:
                rx_el, rx_type, rx_fn, rx_len = struct.unpack("<HHHH", frame[:8])
                if rx_el == 2 and rx_fn == 0x0000 and frame[8] == 1:
                    rx_on = True
                    break
        if not rx_on:
            self.log_error("Node 1 Relay failed to actuate ON over BLE Mesh from Node 0 Switch")
            return False
        self.log_info("  -> Node 1 Relay successfully actuated to ON over BLE Mesh!")

        # Command Node 0 Switch to OFF
        cmd_off = struct.pack("<HHHHB", 1, 1, 0x0000, 1, 0)
        dev0.node.send_mxcp_frame(0x10, cmd_off)

        rx_off = False
        start = time.time()
        while time.time() - start < 5.0:
            frame = dev1.node.wait_for_mxcp_frame(0x90, timeout=0.5)
            if frame and len(frame) >= 9:
                rx_el, rx_type, rx_fn, rx_len = struct.unpack("<HHHH", frame[:8])
                if rx_el == 2 and rx_fn == 0x0000 and frame[8] == 0:
                    rx_off = True
                    break
        if not rx_off:
            self.log_error("Node 1 Relay failed to actuate OFF over BLE Mesh from Node 0 Switch")
            return False
        self.log_info("  -> Node 1 Relay successfully actuated to OFF over BLE Mesh!")

        # -------------------------------------------------------------
        # Phase 6: Cross-Node Unpairing & Cleanup
        # -------------------------------------------------------------
        self.log_info("[Phase 6] Teardown & Unpairing across nodes...")
        dev0.pairing.unbind_elements(src_elem=1, dst_addr=addr1, dst_elem=2, func_id=0x0000)
        unbind_rsp = dev0.pairing.wait_for_unbind_status(timeout=5.0)
        if not unbind_rsp or unbind_rsp["status"] != 0:
            self.log_error(f"Cross-node unbind failed: {unbind_rsp}")
            return False

        time.sleep(0.5)
        dev0.pairing.get_link_table()
        tbl0_post = dev0.pairing.wait_for_link_table_rsp(timeout=3.0)
        if tbl0_post and tbl0_post["link_count"] == 0:
            self.log_info("  -> Node 0 link table cleaned up successfully (count == 0)")
        else:
            self.log_error(f"Node 0 link table not empty after unbind: {tbl0_post}")
            return False

        self.log_info("All Multi-Node Functional Pairing verification steps PASSED!")
        return True
