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

    def send_on(self, element_id=2):
        # ut 0 1 1 <elem>
        return self.node.send_command(f"ut 0 1 1 {element_id}")

    def send_off(self, element_id=2):
        # ut 0 1 0 <elem>
        return self.node.send_command(f"ut 0 1 0 {element_id}")

    def wait_for_state(self, state, timeout=10):
        pattern = f"State changed to {state}"
        return self.node.expect(pattern, timeout=timeout)

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
