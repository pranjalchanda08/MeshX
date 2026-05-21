import time
import logging
import struct

class RelayInterface:
    def __init__(self, node):
        self.node = node
        self.logger = logging.getLogger(f"{node.name}.Relay")

    def set_onoff(self, element_id, state, ack=True):
        # MXCP_CMD_EL_SEND (0x10)
        # MESHX_ELEMENT_TYPE_RELAY_SERVER = 0
        payload = struct.pack("<HHHHB", element_id, 0x0000, 0x0000, 1, state)
        return self.node.send_mxcp_frame(0x10, payload)

    def toggle(self, element_id):
        # Use a dummy state of 1 for toggle, actual tests should track state
        return self.set_onoff(element_id, 1)

    def check_state(self, element_id, expected_state):
        # Wait for MXCP_EVT_EL_DATA_NOTIFY (0x90)
        start_time = time.time()
        while time.time() - start_time < 2:
            frame = self.node.wait_for_mxcp_frame(0x90, timeout=0.5)
            if frame and len(frame) >= 9:
                rx_el_id, rx_el_type, rx_func, rx_len = struct.unpack("<HHHH", frame[:8])
                if rx_el_id == element_id and rx_el_type == 0x0000 and rx_func == 0x0000:
                    state = frame[8]
                    if state == expected_state:
                        return True
        return False


class CWWWInterface:
    def __init__(self, node):
        self.node = node
        self.logger = logging.getLogger(f"{node.name}.CWWW")

    def set_onoff(self, element_id, state, ack=True):
        # MESHX_ELEMENT_TYPE_LIGHT_CWWW_SERVER = 2
        payload = struct.pack("<HHHHB", element_id, 2, 0x0000, 1, state)
        return self.node.send_mxcp_frame(0x10, payload)

    def set_ctl(self, element_id, temp, lightness, delta_uv=0, ack=True):
        payload = struct.pack("<HHHHHHH", element_id, 2, 0x0001, 6, lightness, temp, delta_uv)
        return self.node.send_mxcp_frame(0x10, payload)

    def check_state_onoff(self, element_id, expected_state):
        start_time = time.time()
        while time.time() - start_time < 2:
            frame = self.node.wait_for_mxcp_frame(0x90, timeout=0.5)
            if frame and len(frame) >= 9:
                rx_el_id, rx_el_type, rx_func, rx_len = struct.unpack("<HHHH", frame[:8])
                if rx_el_id == element_id and rx_el_type == 2 and rx_func == 0x0000:
                    state = frame[8]
                    if state == expected_state:
                        return True
        return False

    def check_state_ctl(self, element_id, expected_lightness, expected_temp):
        start_time = time.time()
        while time.time() - start_time < 2:
            frame = self.node.wait_for_mxcp_frame(0x90, timeout=0.5)
            if frame and len(frame) >= 14:
                rx_el_id, rx_el_type, rx_func, rx_len = struct.unpack("<HHHH", frame[:8])
                if rx_el_id == element_id and rx_el_type == 2 and rx_func == 0x0001:
                    rx_lightness, rx_temp, rx_delta_uv = struct.unpack("<HHH", frame[8:14])
                    if rx_lightness == expected_lightness and rx_temp == expected_temp:
                        return True
        return False


class SensorInterface:
    def __init__(self, node):
        self.node = node
        self.logger = logging.getLogger(f"{node.name}.Sensor")

    def set_data(self, element_id, value, ack=True):
        # MESHX_ELEMENT_TYPE_SENSOR_SERVER = 6
        payload = struct.pack("<HHHHH", element_id, 6, 0x0000, 2, value)
        return self.node.send_mxcp_frame(0x10, payload)

    def check_state(self, element_id, expected_value):
        start_time = time.time()
        while time.time() - start_time < 2:
            frame = self.node.wait_for_mxcp_frame(0x90, timeout=0.5)
            if frame and len(frame) >= 10:
                rx_el_id, rx_el_type, rx_func, rx_len = struct.unpack("<HHHH", frame[:8])
                if rx_el_id == element_id and rx_el_type == 6 and rx_func == 0x0000:
                    val = struct.unpack("<H", frame[8:10])[0]
                    if val == expected_value:
                        return True
        return False


class HSLInterface:
    def __init__(self, node):
        self.node = node
        self.logger = logging.getLogger(f"{node.name}.HSL")

    def set_onoff(self, element_id, state, ack=True):
        # MESHX_ELEMENT_TYPE_LIGHT_HSL_SERVER = 4
        payload = struct.pack("<HHHHB", element_id, 4, 0x0000, 1, state)
        return self.node.send_mxcp_frame(0x10, payload)

    def set_hsl(self, element_id, lightness, hue, saturation, ack=True):
        payload = struct.pack("<HHHHHHH", element_id, 4, 0x0001, 6, lightness, hue, saturation)
        return self.node.send_mxcp_frame(0x10, payload)

    def check_state_onoff(self, element_id, expected_state):
        start_time = time.time()
        while time.time() - start_time < 2:
            frame = self.node.wait_for_mxcp_frame(0x90, timeout=0.5)
            if frame and len(frame) >= 9:
                rx_el_id, rx_el_type, rx_func, rx_len = struct.unpack("<HHHH", frame[:8])
                if rx_el_id == element_id and rx_el_type == 4 and rx_func == 0x0000:
                    state = frame[8]
                    if state == expected_state:
                        return True
        return False

    def check_state_hsl(self, element_id, expected_lightness, expected_hue, expected_saturation):
        start_time = time.time()
        while time.time() - start_time < 2:
            frame = self.node.wait_for_mxcp_frame(0x90, timeout=0.5)
            if frame and len(frame) >= 14:
                rx_el_id, rx_el_type, rx_func, rx_len = struct.unpack("<HHHH", frame[:8])
                if rx_el_id == element_id and rx_el_type == 4 and rx_func == 0x0001:
                    rx_lightness, rx_hue, rx_sat = struct.unpack("<HHH", frame[8:14])
                    if rx_lightness == expected_lightness and rx_hue == expected_hue and rx_sat == expected_saturation:
                        return True
        return False


class MeshXDevice:
    """
    High-level abstraction of a MeshX Device.
    Composes multiple functional interfaces communicating via MXCP.
    """
    def __init__(self, node):
        self.node = node
        self.name = node.name
        self.relay = RelayInterface(node)
        self.cwww = CWWWInterface(node)
        self.sensor = SensorInterface(node)
        self.hsl = HSLInterface(node)

    def reboot(self):
        self.node.send_command("reboot", wait_for_prompt=False)
        time.sleep(2)
        return self.node.connect()

