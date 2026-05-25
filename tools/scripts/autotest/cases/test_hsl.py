from framework.base import TestBase
import time

class TestHSLCommunication(TestBase):
    name = "HSL_MXCP"
    required_nodes = 1

    def run(self):
        device = self.devices[0]
        self.log_info(f"HSL Test on {device.name}")
        
        self.log_info("Triggering HSL OnOff Set...")
        device.hsl.set_onoff(element_id=7, state=1)
        if not device.hsl.check_state_onoff(element_id=7, expected_state=1):
            self.log_error("HSL OnOff Set failed")
            return False

        lightness = 32000
        hue = 15000
        saturation = 50000
        self.log_info(f"Triggering HSL Set: light={lightness}, hue={hue}, sat={saturation}")
        device.hsl.set_hsl(element_id=7, lightness=lightness, hue=hue, saturation=saturation)
        if not device.hsl.check_state_hsl(element_id=7, expected_lightness=lightness, expected_hue=hue, expected_saturation=saturation):
            self.log_error("HSL Set failed")
            return False

        return True
