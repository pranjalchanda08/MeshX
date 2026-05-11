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
        # ut 0 3 3 <elem> <pub_addr> <app_id>
        return self.node.send_command(f"ut 0 3 3 {element_id} {pub_addr} {app_id}")

    def set_onoff(self, element_id, state, ack=True):
        # ut 0 {1|2} 2 <el_id> <state>
        return self.node.send_command(f"ut 0 {1 if ack else 2} 2 {element_id} {state}")

    def toggle(self, element_id):
        # Keep toggle for backward compatibility if needed, but it should just use set_onoff
        # Actually, let's just make it call the old command if we haven't updated firmware everywhere
        # But here we HAVE updated it.
        # For now, let's just implement it as a SET to 0/1 based on some logic? 
        # No, let's just use set_onoff in tests.
        return self.set_onoff(element_id, 1) # Dummy toggle

    def wait_for_state(self, element_id, state, timeout=10):
        # Wait for the machine readable log from the server
        pattern = f"Relay Server \\[{element_id}\\] state: {state}"
        return self.node.expect(pattern, timeout=timeout)

    def check_state(self, element_id, expected_state):
        # ut 4 1 2 <elem> <expected_state>
        self.node.send_command(f"ut 4 1 2 {element_id} {expected_state}")
        return self.node.expect(f"Relay Server \\[{element_id}\\] state: {expected_state}, expected: {expected_state}", timeout=2)

class CWWWInterface:
    def __init__(self, node):
        self.node = node
        self.logger = logging.getLogger(f"{node.name}.CWWW")

    def configure_pub(self, element_id, pub_addr, app_id=0):
        # ut 1 3 3 <elem> <pub_addr> <app_id>
        return self.node.send_command(f"ut 1 3 3 {element_id} {pub_addr} {app_id}")

    def set_onoff(self, element_id, state, ack=True):
        # ut 1 {1|2} 2 <el_id> <state>
        return self.node.send_command(f"ut 1 {1 if ack else 2} 2 {element_id} {state}")

    def set_ctl(self, element_id, temp, lightness, delta_uv=0, ack=True):
        # ut 1 4 4 <el_id> <temp> <lightness> <delta_uv>
        return self.node.send_command(f"ut 1 {4 if ack else 5} 4 {element_id} {temp} {lightness} {delta_uv}")

    def set_lightness(self, element_id, lightness, ack=True):
        # ut 1 6 2 <el_id> <lightness> or ut 1 7 2
        return self.node.send_command(f"ut 1 {6 if ack else 7} 2 {element_id} {lightness}")

    def set_temperature(self, element_id, temp, ack=True):
        # ut 1 8 2 <el_id> <temp> or ut 1 9 2
        return self.node.send_command(f"ut 1 {8 if ack else 9} 2 {element_id} {temp}")

    def set_delta_uv(self, element_id, delta_uv, ack=True):
        # ut 1 10 2 <el_id> <delta_uv> or ut 1 11 2
        return self.node.send_command(f"ut 1 {10 if ack else 11} 2 {element_id} {delta_uv}")

    def check_state_onoff(self, element_id, expected_state):

        # ut 5 1 2 <el_id> <expected_state>
        self.node.send_command(f"ut 5 1 2 {element_id} {expected_state}")
        return self.node.expect(f"CWWW Server \\[{element_id}\\] ONOFF state: {expected_state}, expected: {expected_state}", timeout=2)

    def check_state_ctl(self, element_id, expected_lightness, expected_temp):
        # ut 5 2 3 <el_id> <lightness> <temp>
        self.node.send_command(f"ut 5 2 3 {element_id} {expected_lightness} {expected_temp}")
        return self.node.expect(f"CWWW Server \\[{element_id}\\] CTL state: light {expected_lightness}, temp {expected_temp}", timeout=2)

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
        self.cwww = CWWWInterface(node)


    def reboot(self):
        self.node.send_command("reboot", wait_for_prompt=False)
        time.sleep(2)
        return self.node.connect()
