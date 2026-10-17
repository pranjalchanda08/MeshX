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
        # Wait for MXCP_EVT_EL_DATA_RX_NOTIFY (0x90)
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


class CompositionInterface:
    def __init__(self, node):
        self.node = node
        self.logger = logging.getLogger(f"{node.name}.Composition")

    def set_composition(self, element_types, reboot_delay_ms=500):
        count = len(element_types)
        fmt = f"<BH{count}H"
        payload = struct.pack(fmt, count, reboot_delay_ms, *element_types)
        return self.node.send_mxcp_frame(0x06, payload)

    def wait_for_set_composition_rsp(self, timeout=3.0):
        frame = self.node.wait_for_mxcp_frame(0x8B, timeout=timeout)
        if frame and len(frame) >= 4:
            status, count, delay = struct.unpack("<BBH", frame[:4])
            return {"status": status, "count": count, "reboot_delay_ms": delay}
        return None

    def get_composition(self):
        return self.node.send_mxcp_frame(0x03, b"")

    def wait_for_composition_rsp(self, timeout=3.0):
        frame = self.node.wait_for_mxcp_frame(0x86, timeout=timeout)
        return frame

    def reset_node(self):
        return self.node.send_mxcp_frame(0x02, b"")


class PairingInterface:
    def __init__(self, node):
        self.node = node
        self.logger = logging.getLogger(f"{node.name}.Pairing")

    def bind_elements(self, src_elem, dst_addr, dst_elem, func_id, src_addr=0):
        # MXCP_CMD_BIND_ELEMENTS (0x71)
        # struct mxcp_cmd_bind_elements_t { uint16_t src_addr; uint8_t src_elem; uint16_t dst_addr; uint8_t dst_elem; uint16_t func_id; }
        payload = struct.pack("<HBHBH", src_addr, src_elem, dst_addr, dst_elem, func_id)
        return self.node.send_mxcp_frame(0x71, payload)

    def wait_for_bind_status(self, timeout=3.0):
        # MXCP_EVT_BIND_STATUS (0x80 | 0x71 = 0xF1)
        # struct mxcp_evt_bind_status_t { uint8_t status; uint16_t group_addr; }
        frame = self.node.wait_for_mxcp_frame(0xF1, timeout=timeout)
        if frame and len(frame) >= 3:
            status, group_addr = struct.unpack("<BH", frame[:3])
            return {"status": status, "group_addr": group_addr}
        return None

    def unbind_elements(self, src_elem, dst_addr, dst_elem, func_id, src_addr=0):
        # MXCP_CMD_UNBIND_ELEMENTS (0x72)
        # struct mxcp_cmd_unbind_elements_t { uint16_t src_addr; uint8_t src_elem; uint16_t dst_addr; uint8_t dst_elem; uint16_t func_id; }
        payload = struct.pack("<HBHBH", src_addr, src_elem, dst_addr, dst_elem, func_id)
        return self.node.send_mxcp_frame(0x72, payload)

    def wait_for_unbind_status(self, timeout=3.0):
        # MXCP_EVT_UNBIND_STATUS (0x80 | 0x72 = 0xF2)
        # struct mxcp_evt_unbind_status_t { uint8_t status; }
        frame = self.node.wait_for_mxcp_frame(0xF2, timeout=timeout)
        if frame and len(frame) >= 1:
            status = struct.unpack("<B", frame[:1])[0]
            return {"status": status}
        return None

    def get_link_table(self):
        # MXCP_CMD_GET_LINK_TABLE (0x73)
        return self.node.send_mxcp_frame(0x73, b"")

    def wait_for_link_table_rsp(self, timeout=3.0):
        # MXCP_EVT_LINK_TABLE_RSP (0x80 | 0x73 = 0xF3)
        # meshx_nvs_link_table_t (packed 171 bytes)
        frame = self.node.wait_for_mxcp_frame(0xF3, timeout=timeout)
        if frame and len(frame) >= 9:
            magic, version, link_epoch, link_count, reserved = struct.unpack("<IBHBB", frame[:9])
            links = []
            offset = 9
            for _ in range(16):
                if offset + 10 <= len(frame):
                    el, variant, unicast, group, func, active = struct.unpack("<BHHHHB", frame[offset:offset+10])
                    links.append({
                        "local_element_idx": el,
                        "bound_variant": variant,
                        "target_unicast_addr": unicast,
                        "target_group_addr": group,
                        "func_id": func,
                        "active": active
                    })
                    offset += 10
            return {
                "magic": magic,
                "version": version,
                "link_epoch": link_epoch,
                "link_count": link_count,
                "links": links
            }
        return None

    def get_link_epoch(self):
        # MXCP_CMD_GET_LINK_EPOCH (0x74)
        return self.node.send_mxcp_frame(0x74, b"")

    def wait_for_link_epoch_rsp(self, timeout=3.0):
        # MXCP_EVT_LINK_EPOCH_RSP (0x80 | 0x74 = 0xF4)
        # struct mxcp_evt_link_epoch_rsp_t { uint16_t link_epoch; uint16_t node_addr; }
        frame = self.node.wait_for_mxcp_frame(0xF4, timeout=timeout)
        if frame and len(frame) >= 2:
            epoch = struct.unpack("<H", frame[:2])[0]
            node_addr = struct.unpack("<H", frame[2:4])[0] if len(frame) >= 4 else 0
            return {"link_epoch": epoch, "node_addr": node_addr}
        return None

    def discover_capabilities(self, target_unicast, filter_role=0):
        # MXCP_CMD_DISCOVER_CAPABILITIES (0x70)
        # struct mxcp_cmd_discover_capabilities_t { uint16_t target_unicast; uint8_t filter_role; }
        payload = struct.pack("<HB", target_unicast, filter_role)
        return self.node.send_mxcp_frame(0x70, payload)

    def wait_for_capability_report(self, timeout=3.0):
        # MXCP_EVT_CAPABILITY_REPORT (0x80 | 0x70 = 0xF0)
        frame = self.node.wait_for_mxcp_frame(0xF0, timeout=timeout)
        return frame


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
        self.composition = CompositionInterface(node)
        self.pairing = PairingInterface(node)

    def reboot(self):
        self.node.send_command("reboot", wait_for_prompt=False)
        time.sleep(2)
        return self.node.connect()



