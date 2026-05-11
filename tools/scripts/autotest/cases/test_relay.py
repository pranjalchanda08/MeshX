from framework.base import TestBase
import time

class TestRelayCommunication(TestBase):
    name = "Relay_Client_to_Server"
    required_nodes = 1

    def run(self):
        device = self.devices[0]

        self.log_info(f"Relay Test on {device.name}")

        # Note: The test assumes the node is provisioned and the server element (1)
        # is subscribed to 0xC000.
        
        # Configure the Client (element 2) to publish to 0xC000
        self.log_info("Configuring Relay Client to publish to 0xC000...")
        device.relay.configure_pub(element_id=2, pub_addr=0xC000, app_id=0)
        time.sleep(1)
        
        # 1. Trigger ON from Client
        self.log_info("Triggering Relay ON from Client...")
        device.relay.set_onoff(element_id=2, state=1)

        # 2. Verify on Server
        self.log_info("Waiting for Server to receive ON state...")
        time.sleep(2)
        
        if not device.relay.check_state(element_id=1, expected_state=1):
             self.log_error("Relay ON failed")
             return False

        # 3. Trigger OFF from Client
        self.log_info("Triggering Relay OFF from Client...")
        device.relay.set_onoff(element_id=2, state=0)
        time.sleep(2)

        # 4. Verify OFF state
        self.log_info("Checking Server state for OFF...")
        if not device.relay.check_state(element_id=1, expected_state=0):
            self.log_error("Relay OFF failed")
            return False

        return True

