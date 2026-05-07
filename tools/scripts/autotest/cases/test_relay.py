from framework.base import TestBase
import time

class TestRelayCommunication(TestBase):
    name = "Relay_Client_to_Server"
    required_nodes = 2

    def run(self):
        client = self.devices[0]
        server = self.devices[1]
        
        self.log_info(f"Relay Test: Client={client.name}, Server={server.name}")

        # 1. Trigger ON from Client
        self.log_info("Triggering Relay ON from Client...")
        client.relay.send_on(element_id=1)

        # 2. Verify on Server
        self.log_info("Waiting for Server to receive ON state...")
        if server.relay.wait_for_state(state=1, timeout=10):
            self.log_info("Relay ON received successfully")
        else:
            self.log_error("Relay ON timed out or state mismatch")
            return False

        # 3. Trigger OFF from Client
        self.log_info("Triggering Relay OFF from Client...")
        client.relay.send_off(element_id=1)

        # 4. Verify on Server
        if server.relay.wait_for_state(state=0, timeout=10):
            self.log_info("Relay OFF received successfully")
        else:
            self.log_error("Relay OFF timed out or state mismatch")
            return False

        return True
