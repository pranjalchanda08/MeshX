from framework.base import TestBase
import time

class TestRelayCommunication2Node(TestBase):
    name = "Relay_2Node"
    required_nodes = 2

    def run(self):
        server = self.devices[0]
        client = self.devices[1]

        self.log_info(f"Relay 2-Node Test: Server={server.name}, Client={client.name}")

        # Configure the Client (Node-1, element 2) to publish to 0xC000
        self.log_info(f"Configuring {client.name} Relay Client to publish to 0xC000...")
        client.relay.configure_pub(element_id=2, pub_addr=0xC000, app_id=0)
        time.sleep(1)

        # 1. Trigger First Toggle from Client
        self.log_info(f"Triggering Relay Toggle from {client.name}...")
        client.relay.toggle(element_id=2)

        # 2. Verify on Server
        self.log_info(f"Waiting for {server.name} to receive toggled state via radio...")
        time.sleep(3)
        
        # Let's check the state. It should be 1 if it started at 0, or 0 if it started at 1.
        self.log_info(f"Checking {server.name} state...")
        is_on = False
        if server.relay.check_state(element_id=1, expected_state=1):
             self.log_info("Relay ON received successfully")
             is_on = True
        elif server.relay.check_state(element_id=1, expected_state=0):
             self.log_info("Relay OFF received successfully")
             is_on = False
        else:
             self.log_error(f"Relay state mismatch on {server.name} after first toggle")
             return False

        # 3. Trigger Second Toggle from Client
        self.log_info(f"Triggering Relay Toggle again from {client.name}...")
        client.relay.toggle(element_id=2)
        time.sleep(3)

        # 4. Verify opposite state
        self.log_info(f"Checking {server.name} state again...")
        expected_second_state = 0 if is_on else 1
        
        if server.relay.check_state(element_id=1, expected_state=expected_second_state):
            self.log_info("Relay toggled successfully to opposite state")
        else:
            self.log_error(f"Relay state mismatch: expected {expected_second_state}")
            return False

        return True
