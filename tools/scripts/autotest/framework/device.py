import time
import logging

class NVSInterface:
    def __init__(self, node):
        self.node = node
        self.logger = logging.getLogger(f"{node.name}.NVS")

    def open(self):
        self.node.send_command("ut 3 0 0")
        return self.node.expect("Product ID match")

    def close(self):
        return self.node.send_command("ut 3 6 0")

    def erase(self):
        return self.node.send_command("ut 3 5 0")

    def set(self, key_id, value):
        # ut 3 1 <key> <val>
        return self.node.send_command(f"ut 3 1 {key_id} {value}")

    def get(self, key_id):
        # ut 3 2 <key>
        self.node.send_command(f"ut 3 2 {key_id}")
        # Note: In a real implementation, we would extract the value from logs
        return not self.node.expect("MESHX NVS Integrety Test Failed")

class RelayInterface:
    def __init__(self, node):
        self.node = node
        self.logger = logging.getLogger(f"{node.name}.Relay")

    def configure_pub(self, element_id, pub_addr, app_id=0):
        # ut 0 3 <argc> <elem> <pub_addr> <app_id>
        return self.node.send_command(f"ut 0 3 3 {element_id} {pub_addr} {app_id}")

    def toggle(self, element_id=2):
        # ut 0 1 <argc> <elem>
        return self.node.send_command(f"ut 0 1 1 {element_id}")

    def wait_for_state(self, element_id, state, timeout=10):
        # Wait for the machine readable log from the server
        pattern = f"Relay Server \\[{element_id}\\] state: {state}"
        return self.node.expect(pattern, timeout=timeout)

    def check_state(self, element_id, expected_state):
        # ut 4 1 <argc> <elem> <expected_state>
        self.node.send_command(f"ut 4 1 2 {element_id} {expected_state}")
        return self.node.expect(f"Relay Server \\[{element_id}\\] state: {expected_state}, expected: {expected_state}", timeout=2)

class MeshXDevice:
    """
    High-level abstraction of a MeshX Device.
    Composes multiple functional interfaces (NVS, Relay, etc.).
    """
    def __init__(self, node):
        self.node = node
        self.name = node.name
        self.nvs = NVSInterface(node)
        self.relay = RelayInterface(node)

    def reboot(self):
        self.node.send_command("reboot", wait_for_prompt=False)
        time.sleep(2)
        return self.node.connect()
