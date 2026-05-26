"""
Created By: Pranjal Chanda
Last Modified: 2026-05-26

Usage:
    from meshx_api import MeshXSDK
    sdk = MeshXSDK(serial_port)
"""
import struct
from dataclasses import dataclass

# Unified Direction Flags
MESHX_MSG_DIR_CMD = 0x0000
MESHX_MSG_DIR_EVT = 0x8000

# Unified Data Path Message IDs
MESHX_MSG_DATA_CMD_SEND      = 0x10 | MESHX_MSG_DIR_CMD
MESHX_MSG_DATA_EVT_RX_NOTIFY = 0x10 | MESHX_MSG_DIR_EVT
MESHX_MSG_DATA_EVT_TX_NOTIFY = 0x11 | MESHX_MSG_DIR_EVT

# Unified Control Path Message IDs
MESHX_MSG_CTRL_CMD_HOSTED_MODE_ENABLE    = 0x01 | MESHX_MSG_DIR_CMD
MESHX_MSG_CTRL_CMD_NODE_RESET            = 0x02 | MESHX_MSG_DIR_CMD
MESHX_MSG_CTRL_CMD_GET_COMPOSITION       = 0x03 | MESHX_MSG_DIR_CMD
MESHX_MSG_CTRL_CMD_GET_ELEMENT_STATE     = 0x04 | MESHX_MSG_DIR_CMD
MESHX_MSG_CTRL_CMD_SET_CONSOLE_ROUTING   = 0x05 | MESHX_MSG_DIR_CMD

MESHX_MSG_CTRL_EVT_PROV_COMP             = 0x01 | MESHX_MSG_DIR_EVT
MESHX_MSG_CTRL_EVT_PROV_FAILED           = 0x02 | MESHX_MSG_DIR_EVT
MESHX_MSG_CTRL_EVT_IDENTIFY_START        = 0x03 | MESHX_MSG_DIR_EVT
MESHX_MSG_CTRL_EVT_IDENTIFY_STOP         = 0x04 | MESHX_MSG_DIR_EVT
MESHX_MSG_CTRL_EVT_COMPOSITION_RSP       = 0x05 | MESHX_MSG_DIR_EVT
MESHX_MSG_CTRL_EVT_ELEMENT_STATE_RSP     = 0x06 | MESHX_MSG_DIR_EVT
MESHX_MSG_CTRL_EVT_NODE_RESET_IND        = 0x07 | MESHX_MSG_DIR_EVT
MESHX_MSG_CTRL_EVT_HOSTED_MODE_RSP       = 0x08 | MESHX_MSG_DIR_EVT
MESHX_MSG_CTRL_EVT_CONSOLE_ROUTING_RSP   = 0x09 | MESHX_MSG_DIR_EVT

# GPIO Commands
MESHX_MSG_CTRL_CMD_GPIO_SET_LEVEL        = 0x21 | MESHX_MSG_DIR_CMD
MESHX_MSG_CTRL_CMD_GPIO_GET_LEVEL        = 0x22 | MESHX_MSG_DIR_CMD
MESHX_MSG_CTRL_CMD_GPIO_TOGGLE           = 0x23 | MESHX_MSG_DIR_CMD
MESHX_MSG_CTRL_CMD_GPIO_SET_PWM_DUTY     = 0x24 | MESHX_MSG_DIR_CMD
MESHX_MSG_CTRL_CMD_GPIO_SET_PWM_FREQ     = 0x25 | MESHX_MSG_DIR_CMD
MESHX_MSG_CTRL_CMD_GPIO_INTR_ENABLE      = 0x26 | MESHX_MSG_DIR_CMD
MESHX_MSG_CTRL_CMD_GPIO_INTR_DISABLE     = 0x27 | MESHX_MSG_DIR_CMD
MESHX_MSG_CTRL_CMD_GPIO_GET_CONFIG       = 0x28 | MESHX_MSG_DIR_CMD
MESHX_MSG_CTRL_CMD_GPIO_GET_STATE        = 0x29 | MESHX_MSG_DIR_CMD

# GPIO Events
MESHX_MSG_CTRL_EVT_GPIO_SET_LEVEL_RSP    = 0x21 | MESHX_MSG_DIR_EVT
MESHX_MSG_CTRL_EVT_GPIO_GET_LEVEL_RSP    = 0x22 | MESHX_MSG_DIR_EVT
MESHX_MSG_CTRL_EVT_GPIO_TOGGLE_RSP       = 0x23 | MESHX_MSG_DIR_EVT
MESHX_MSG_CTRL_EVT_GPIO_SET_PWM_DUTY_RSP = 0x24 | MESHX_MSG_DIR_EVT
MESHX_MSG_CTRL_EVT_GPIO_SET_PWM_FREQ_RSP = 0x25 | MESHX_MSG_DIR_EVT
MESHX_MSG_CTRL_EVT_GPIO_INTR_ENABLE_RSP  = 0x26 | MESHX_MSG_DIR_EVT
MESHX_MSG_CTRL_EVT_GPIO_INTR_DISABLE_RSP = 0x27 | MESHX_MSG_DIR_EVT
MESHX_MSG_CTRL_EVT_GPIO_GET_CONFIG_RSP   = 0x28 | MESHX_MSG_DIR_EVT
MESHX_MSG_CTRL_EVT_GPIO_GET_STATE_RSP    = 0x29 | MESHX_MSG_DIR_EVT
MESHX_MSG_CTRL_EVT_GPIO_ASYNC            = 0x3E | MESHX_MSG_DIR_EVT
MESHX_MSG_CTRL_EVT_GPIO_ERROR            = 0x3F | MESHX_MSG_DIR_EVT

@dataclass
class MeshXDataMsg:
    '''
        @dataclass: Data plane message wrapper
        struct meshx_msg_data_t {
            uint16_t msg_id;
            uint16_t element_id;
            uint16_t element_type;
            uint16_t func_id;
            uint16_t payload_len;
            uint8_t payload[];
        };
        @note: This is a wrapper around the data plane message.
    '''
    msg_id: int
    element_id: int
    element_type: int
    func_id: int
    payload: bytes

    def pack(self) -> bytes:
        '''
            Pack the data plane message.
            @return: Packed data plane message.
        '''
        # struct meshx_msg_data_t: uint16_t msg_id, uint16_t element_id, uint16_t element_type, uint16_t func_id, uint16_t payload_len, uint8_t payload[]
        header = struct.pack('<HHHHH', self.msg_id, self.element_id, self.element_type, self.func_id, len(self.payload))
        return header + self.payload

    @classmethod
    def unpack(cls, data: bytes) -> 'MeshXDataMsg':
        '''
            Unpack the data plane message.
            @param data: Packed data plane message.
            @return: Unpacked data plane message.
        '''
        header_len = struct.calcsize('<HHHHH')
        if len(data) < header_len:
            raise ValueError("Data too short")
        msg_id, element_id, element_type, func_id, payload_len = struct.unpack('<HHHHH', data[:header_len])
        payload = data[header_len:header_len+payload_len]
        return cls(msg_id, element_id, element_type, func_id, payload)

@dataclass
class MeshXCtrlMsg:
    '''
        @dataclass: Control plane message wrapper
        struct meshx_msg_ctrl_t {
            uint16_t msg_id;
            uint16_t payload_len;
            uint8_t payload[];
        };
        @note: This is a wrapper around the control plane message.
    '''
    msg_id: int
    payload: bytes

    def pack(self) -> bytes:
        '''
            Pack the control plane message.
            @return: Packed control plane message.
        '''
        # struct meshx_msg_ctrl_t: uint16_t msg_id, uint16_t payload_len, uint8_t payload[]
        header = struct.pack('<HH', self.msg_id, len(self.payload))
        return header + self.payload

    @classmethod
    def unpack(cls, data: bytes) -> 'MeshXCtrlMsg':
        '''
            Unpack the control plane message.
            @param data: Packed control plane message.
            @return: Unpacked control plane message.
        '''
        header_len = struct.calcsize('<HH')
        if len(data) < header_len:
            raise ValueError("Data too short")
        msg_id, payload_len = struct.unpack('<HH', data[:header_len])
        payload = data[header_len:header_len+payload_len]
        return cls(msg_id, payload)

class MeshXSDK:
    '''
        @class: MeshXSDK
        @brief: MeshX SDK for ESP BLE Mesh node.
        @note: This class is a wrapper around the data plane and control plane messages.
    '''
    def __init__(self, serial_port=None):
        '''
            @brief: Initialize the MeshX SDK.
            @param serial_port: Serial port for communication with the device.
        '''
        self.serial_port = serial_port
        self.data_cb = None
        self.ctrl_cb = None

    def data_send(self, msg: MeshXDataMsg) -> bool:
        '''
            @brief: Send a data plane message.
            @param msg: Data plane message to send.
            @return: True if message was sent successfully, False otherwise.
        '''
        if self.serial_port:
            self.serial_port.write(msg.pack())
            return True
        return False

    def ctrl_send(self, msg: MeshXCtrlMsg) -> bool:
        '''
            @brief: Send a control plane message.
            @param msg: Control plane message to send.
            @return: True if message was sent successfully, False otherwise.
        '''
        if self.serial_port:
            self.serial_port.write(msg.pack())
            return True
        return False

    def send_hosted_mode_enable(self, enable: bool) -> bool:
        '''
            @brief: Send a hosted mode enable message.
            @param enable: Enable or disable hosted mode.
            @return: True if message was sent successfully, False otherwise.
        '''
        payload = struct.pack('<B', 1 if enable else 0)
        return self.ctrl_send(MeshXCtrlMsg(MESHX_MSG_CTRL_CMD_HOSTED_MODE_ENABLE, payload))

    def send_node_reset(self) -> bool:
        '''
            @brief: Send a node reset message.
            @return: True if message was sent successfully, False otherwise.
        '''
        return self.ctrl_send(MeshXCtrlMsg(MESHX_MSG_CTRL_CMD_NODE_RESET, b''))

    def send_get_composition(self) -> bool:
        '''
            @brief: Send a get composition message.
            @return: True if message was sent successfully, False otherwise.
        '''
        return self.ctrl_send(MeshXCtrlMsg(MESHX_MSG_CTRL_CMD_GET_COMPOSITION, b''))

    def send_get_element_state(self, element_id: int) -> bool:
        '''
            @brief: Send a get element state message.
            @param element_id: Element ID to get state of.
            @return: True if message was sent successfully, False otherwise.
        '''
        payload = struct.pack('<H', element_id)
        return self.ctrl_send(MeshXCtrlMsg(MESHX_MSG_CTRL_CMD_GET_ELEMENT_STATE, payload))

    def send_set_console_routing(self, routing_mode: int) -> bool:
        '''
            @brief: Send a set console routing message.
            @param routing_mode: Routing mode to set.
            @return: True if message was sent successfully, False otherwise.
        '''
        payload = struct.pack('<B', routing_mode)
        return self.ctrl_send(MeshXCtrlMsg(MESHX_MSG_CTRL_CMD_SET_CONSOLE_ROUTING, payload))

    # GPIO Wrapper APIs
    def send_gpio_set_level(self, pin: int, level: int) -> bool:
        '''
            @brief: Send a gpio set level message.
            @param pin: Pin to set level of.
            @param level: Level to set.
            @return: True if message was sent successfully, False otherwise.
        '''
        payload = struct.pack('<BB', pin, level)
        return self.ctrl_send(MeshXCtrlMsg(MESHX_MSG_CTRL_CMD_GPIO_SET_LEVEL, payload))

    def send_gpio_get_level(self, pin: int) -> bool:
        '''
            @brief: Send a gpio get level message.
            @param pin: Pin to get level of.
            @return: True if message was sent successfully, False otherwise.
        '''
        payload = struct.pack('<B', pin)
        return self.ctrl_send(MeshXCtrlMsg(MESHX_MSG_CTRL_CMD_GPIO_GET_LEVEL, payload))

    def send_gpio_toggle(self, pin: int) -> bool:
        '''
            @brief: Send a gpio toggle message.
            @param pin: Pin to toggle.
            @return: True if message was sent successfully, False otherwise.
        '''
        payload = struct.pack('<B', pin)
        return self.ctrl_send(MeshXCtrlMsg(MESHX_MSG_CTRL_CMD_GPIO_TOGGLE, payload))

    def send_gpio_set_pwm_duty(self, pin: int, duty: int) -> bool:
        '''
            @brief: Send a gpio set pwm duty message.
            @param pin: Pin to set pwm duty of.
            @param duty: Duty to set.
            @return: True if message was sent successfully, False otherwise.
        '''
        payload = struct.pack('<BL', pin, duty)
        return self.ctrl_send(MeshXCtrlMsg(MESHX_MSG_CTRL_CMD_GPIO_SET_PWM_DUTY, payload))

    def send_gpio_set_pwm_freq(self, pin: int, freq: int) -> bool:
        '''
            @brief: Send a gpio set pwm frequency message.
            @param pin: Pin to set pwm frequency of.
            @param freq: Frequency to set.
            @return: True if message was sent successfully, False otherwise.
        '''
        payload = struct.pack('<BL', pin, freq)
        return self.ctrl_send(MeshXCtrlMsg(MESHX_MSG_CTRL_CMD_GPIO_SET_PWM_FREQ, payload))

    def send_gpio_intr_enable(self, pin: int, intr_type: int) -> bool:
        '''
            @brief: Send a gpio interrupt enable message.
            @param pin: Pin to enable interrupt on.
            @param intr_type: Interrupt type to enable.
            @return: True if message was sent successfully, False otherwise.
        '''
        payload = struct.pack('<BB', pin, intr_type)
        return self.ctrl_send(MeshXCtrlMsg(MESHX_MSG_CTRL_CMD_GPIO_INTR_ENABLE, payload))

    def send_gpio_intr_disable(self, pin: int) -> bool:
        '''
            @brief: Send a gpio interrupt disable message.
            @param pin: Pin to disable interrupt on.
            @return: True if message was sent successfully, False otherwise.
        '''
        payload = struct.pack('<B', pin)
        return self.ctrl_send(MeshXCtrlMsg(MESHX_MSG_CTRL_CMD_GPIO_INTR_DISABLE, payload))

    def send_gpio_get_config(self, pin: int) -> bool:
        '''
            @brief: Send a gpio get config message.
            @param pin: Pin to get config of.
            @return: True if message was sent successfully, False otherwise.
        '''
        payload = struct.pack('<B', pin)
        return self.ctrl_send(MeshXCtrlMsg(MESHX_MSG_CTRL_CMD_GPIO_GET_CONFIG, payload))

    def send_gpio_get_state(self, pin: int) -> bool:
        '''
            @brief: Send a gpio get state message.
            @param pin: Pin to get state of.
            @return: True if message was sent successfully, False otherwise.
        '''
        payload = struct.pack('<B', pin)
        return self.ctrl_send(MeshXCtrlMsg(MESHX_MSG_CTRL_CMD_GPIO_GET_STATE, payload))

    # Data Path Wrapper API
    def send_element_data(self, element_id: int, element_type: int, func_id: int, payload: bytes) -> bool:
        '''
            @brief: Send a element data message.
            @param element_id: Element ID to send data to.
            @param element_type: Element type to send data to.
            @param func_id: Function ID to send data to.
            @param payload: Payload to send.
            @return: True if message was sent successfully, False otherwise.
        '''
        return self.data_send(MeshXDataMsg(MESHX_MSG_DATA_CMD_SEND, element_id, element_type, func_id, payload))

    def register_data_cb(self, cb):
        '''
            @brief: Register a data callback.
            @param cb: Callback function to register.
        '''
        self.data_cb = cb

    def register_ctrl_cb(self, cb):
        '''
            @brief: Register a control callback.
            @param cb: Callback function to register.
        '''
        self.ctrl_cb = cb

    def handle_rx(self, mxcp_frame_payload: bytes):
        """
        @brief: Handle received MXCP frame payload.
        @param mxcp_frame_payload: MXCP frame payload to handle.
        """
        if len(mxcp_frame_payload) < 2:
            return

        # MXCP type dictates if it's Data or Ctrl
        # This is a simplification. The user port/demuxer should extract the meshx msg and pass it here,
        # or we just try to unpack based on first byte
        msg_id = struct.unpack('<H', mxcp_frame_payload[:2])[0]

        # Determine if it's data or ctrl based on your ID schema
        # For instance, 0x1X are data messages, 0x0X, 0x2X, 0x3X are ctrl messages
        if (msg_id & 0x7FFF) >= 0x10 and (msg_id & 0x7FFF) <= 0x1F:
            msg = MeshXDataMsg.unpack(mxcp_frame_payload)
            if self.data_cb:
                self.data_cb(msg)
        else:
            msg = MeshXCtrlMsg.unpack(mxcp_frame_payload)
            if self.ctrl_cb:
                self.ctrl_cb(msg)
