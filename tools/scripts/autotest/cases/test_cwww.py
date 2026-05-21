from framework.base import TestBase
import time

class TestCWWWCommunication(TestBase):
    name = "CWWW_MXCP"
    required_nodes = 1

    def run(self):
        device = self.devices[0]

        self.log_info(f"CWWW Test on {device.name}")
        
        # 1. Test OnOff Set
        self.log_info("Triggering CWWW OnOff Set...")
        device.cwww.set_onoff(element_id=3, state=1)
        if not device.cwww.check_state_onoff(element_id=3, expected_state=1):
            self.log_error("CWWW OnOff Set failed")
            return False

        self.log_info("Triggering CWWW OnOff Set OFF...")
        device.cwww.set_onoff(element_id=3, state=0)
        if not device.cwww.check_state_onoff(element_id=3, expected_state=0):
            self.log_error("CWWW OnOff Set OFF failed")
            return False

        # 2. Test CTL Set
        target_lightness = 10000
        target_temp = 4000
        self.log_info(f"Triggering CWWW CTL Set: light={target_lightness}, temp={target_temp}")
        device.cwww.set_ctl(element_id=3, temp=target_temp, lightness=target_lightness)
        if not device.cwww.check_state_ctl(element_id=3, expected_lightness=target_lightness, expected_temp=target_temp):
            self.log_error("CWWW CTL Set failed")
            return False

        return True

