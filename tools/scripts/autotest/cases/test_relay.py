from framework.base import TestBase
import time

class TestRelayCommunication(TestBase):
    name = "Relay_MXCP"
    required_nodes = 1

    def run(self):
        device = self.devices[0]

        self.log_info(f"Relay Test on {device.name}")

        # 1. Trigger ON
        self.log_info("Triggering Relay ON...")
        device.relay.set_onoff(element_id=1, state=1)

        # 2. Verify ON
        self.log_info("Waiting for telemetry for ON state...")
        if not device.relay.check_state(element_id=1, expected_state=1):
             self.log_error("Relay ON failed")
             return False

        # 3. Trigger OFF
        self.log_info("Triggering Relay OFF...")
        device.relay.set_onoff(element_id=1, state=0)

        # 4. Verify OFF
        self.log_info("Checking telemetry for OFF...")
        if not device.relay.check_state(element_id=1, expected_state=0):
            self.log_error("Relay OFF failed")
            return False

        return True

