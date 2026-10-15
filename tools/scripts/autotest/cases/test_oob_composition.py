from framework.base import TestBase
import time

class TestOOBDynamicComposition(TestBase):
    """
    Functional verification for OOB Dynamic Personality Management:
    1. L0 NVS Schema Persistence & CRC Verification (via CLI)
    2. MXCP Remote Composition Configuration (MXCP_CMD_SET_COMPOSITION 0x06)
    3. Graceful Reset & Reconstitution from NVS
    4. Factory/Node Reset Cleanup (MXCP_CMD_NODE_RESET 0x02)
    """
    name = "OOB_Dynamic_Personality"
    required_nodes = 1

    def run(self):
        device = self.devices[0]
        node = device.node

        self.log_info(f"Starting OOB Dynamic Personality Verification on {device.name}")

        # ---------------------------------------------------------
        # Phase 1: L0 NVS Unit Tests (CLI)
        # ---------------------------------------------------------
        self.log_info("[Phase 1] Executing L0 NVS Unit Tests via CLI...")

        # 1.1 Set test schema (3 elements)
        node.clear_logs()
        node.send_command("ut 3 7 1 3")
        if not node.expect(r"UT SCHEMA_SET PASS: 3 elements", timeout=3):
            self.log_error("L0 UT SCHEMA_SET failed")
            return False
        self.log_info("  -> UT SCHEMA_SET PASSED")

        # 1.2 Get test schema and verify CRC
        node.clear_logs()
        node.send_command("ut 3 8 0")
        if not node.expect(r"UT SCHEMA_GET PASS", timeout=3):
            self.log_error("L0 UT SCHEMA_GET failed")
            return False
        self.log_info("  -> UT SCHEMA_GET PASSED")

        # 1.3 Verify CRC mismatch rejection
        node.clear_logs()
        node.send_command("ut 3 10 0")
        if not node.expect(r"UT SCHEMA_CORRUPT PASS", timeout=3):
            self.log_error("L0 UT SCHEMA_CORRUPT failed")
            return False
        self.log_info("  -> UT SCHEMA_CORRUPT PASSED")

        # 1.4 Erase schema
        node.clear_logs()
        node.send_command("ut 3 9 0")
        if not node.expect(r"UT SCHEMA_ERASE PASS", timeout=3):
            self.log_error("L0 UT SCHEMA_ERASE failed")
            return False
        self.log_info("  -> UT SCHEMA_ERASE PASSED")

        # ---------------------------------------------------------
        # Phase 2: Invalid Composition Request Rejection
        # ---------------------------------------------------------
        self.log_info("[Phase 2] Testing invalid composition rejection...")
        # Send invalid element type (99)
        device.composition.set_composition([99], reboot_delay_ms=500)
        rsp = device.composition.wait_for_set_composition_rsp(timeout=2.0)
        if rsp and rsp["status"] == 0:
            self.log_error("Device accepted invalid element type 99!")
            return False
        self.log_info("  -> Invalid composition correctly rejected")

        # ---------------------------------------------------------
        # Phase 3: MXCP Remote Dynamic Personality Configuration
        # ---------------------------------------------------------
        self.log_info("[Phase 3] Configuring 3 dynamic elements (Relay, Sensor, HSL) via MXCP...")
        # Elements: 0 = Relay Server, 6 = Sensor Server, 4 = Light HSL Server
        dynamic_types = [0, 6, 4]
        node.clear_logs()
        if not device.composition.set_composition(dynamic_types, reboot_delay_ms=500):
            self.log_error("Failed to send MXCP_CMD_SET_COMPOSITION frame")
            return False

        rsp = device.composition.wait_for_set_composition_rsp(timeout=3.0)
        if not rsp or rsp["status"] != 0:
            self.log_error(f"Set composition failed or timed out. rsp={rsp}")
            return False

        self.log_info(f"  -> MXCP_EVT_SET_COMPOSITION_RSP received: count={rsp['count']}, delay={rsp['reboot_delay_ms']}ms")

        # ---------------------------------------------------------
        # Phase 4: Verify Graceful Reset & Reconstitution
        # ---------------------------------------------------------
        self.log_info("[Phase 4] Waiting for graceful reboot and NVS reconstitution...")
        # Check reboot trigger
        if not node.expect(r"Rebooting MCU to apply new composition", timeout=4):
            self.log_info("Reboot trigger log missed (may have reset immediately), reconnecting...")

        time.sleep(2.0)

        # Confirm boot message reading composition from NVS
        if node.expect(r"Applying composition from NVS \(3 elements, override active\)", timeout=8):
            self.log_info("  -> NVS personality successfully restored and applied on boot!")
        else:
            self.log_error("Device did not restore composition from NVS on boot")
            return False

        # ---------------------------------------------------------
        # Phase 5: Factory / Node Reset Cleanup
        # ---------------------------------------------------------
        self.log_info("[Phase 5] Testing Factory Reset / Cleanup...")
        node.clear_logs()
        device.composition.reset_node()

        if not node.expect(r"Erasing NVS composition schema", timeout=4):
            self.log_error("Node reset did not trigger composition schema erasure")
            return False

        self.log_info("  -> Dynamic composition schema successfully erased on node reset")
        self.log_info("All OOB Dynamic Personality verification steps PASSED!")
        return True
