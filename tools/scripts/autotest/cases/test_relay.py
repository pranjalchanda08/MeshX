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
        
        # 1. Trigger First Toggle from Client
        self.log_info("Triggering Relay Toggle from Client...")
        device.relay.toggle(element_id=2)

        # 2. Verify on Server
        self.log_info("Waiting for Server to receive toggled state...")
        time.sleep(2)
        
        # Let's check the state. It should be 1 if it started at 0, or 0 if it started at 1.
        self.log_info("Checking Server state...")
        is_on = False
        if device.relay.check_state(element_id=1, expected_state=1):
             self.log_info("Relay ON received successfully")
             is_on = True
        elif device.relay.check_state(element_id=1, expected_state=0):
             self.log_info("Relay OFF received successfully")
             is_on = False
        else:
             self.log_error("Relay state mismatch on first toggle")
             return False

        # 3. Trigger Second Toggle from Client
        self.log_info("Triggering Relay Toggle again from Client...")
        device.relay.toggle(element_id=2)
        time.sleep(2)

        # 4. Verify opposite state
        self.log_info("Checking Server state again...")
        expected_second_state = 0 if is_on else 1
        
        if device.relay.check_state(element_id=1, expected_state=expected_second_state):
            self.log_info("Relay toggled successfully to opposite state")
        else:
            self.log_error(f"Relay state mismatch: expected {expected_second_state}")
            return False

        return True

