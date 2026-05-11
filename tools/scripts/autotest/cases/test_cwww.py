from framework.base import TestBase
import time

class TestCWWWCommunication(TestBase):
    name = "CWWW_Client_to_Server"
    required_nodes = 1

    def run(self):
        device = self.devices[0]

        self.log_info(f"CWWW Test on {device.name}")

        # Configure the Client (element 4) to publish to 0xC000
        self.log_info("Configuring CWWW Client to publish to 0xC000...")
        device.cwww.configure_pub(element_id=4, pub_addr=0xC000, app_id=0)
        time.sleep(1)
        
        # 1. Test OnOff Toggle (Ack)
        self.log_info("Triggering CWWW OnOff Set (Ack) from Client...")
        device.cwww.set_onoff(element_id=4, state=1, ack=True)
        time.sleep(2)
        device.cwww.check_state_onoff(element_id=3, expected_state=1)

        # 2. Test OnOff Set (Unack)
        self.log_info("Triggering CWWW OnOff Set (Unack) from Client...")
        device.cwww.set_onoff(element_id=4, state=0, ack=False)
        time.sleep(2)
        device.cwww.check_state_onoff(element_id=3, expected_state=0)

        # 3. Test CTL Set (Ack)
        target_lightness = 10000
        target_temp = 4000
        self.log_info(f"Triggering CWWW CTL Set (Ack) from Client: light={target_lightness}, temp={target_temp}")
        device.cwww.set_ctl(element_id=4, temp=target_temp, lightness=target_lightness, ack=True)
        time.sleep(2)
        if not device.cwww.check_state_ctl(element_id=3, expected_lightness=target_lightness, expected_temp=target_temp):
            self.log_error("CWWW CTL Set (Ack) failed")
            return False

        # 4. Test CTL Set (Unack)
        target_lightness = 15000
        target_temp = 3000
        self.log_info(f"Triggering CWWW CTL Set (Unack) from Client: light={target_lightness}, temp={target_temp}")
        device.cwww.set_ctl(element_id=4, temp=target_temp, lightness=target_lightness, ack=False)
        time.sleep(2)
        if not device.cwww.check_state_ctl(element_id=3, expected_lightness=target_lightness, expected_temp=target_temp):
            self.log_error("CWWW CTL Set (Unack) failed")
            return False

        # 5. Test Individual Attribute Sets
        self.log_info("Triggering CWWW Individual Attribute Sets...")
        
        # Lightness
        device.cwww.set_lightness(element_id=4, lightness=25000)
        time.sleep(2)
        if not device.cwww.check_state_ctl(element_id=3, expected_lightness=25000, expected_temp=target_temp):
            self.log_error("CWWW Lightness Set failed")
            return False

        # Temperature
        device.cwww.set_temperature(element_id=4, temp=6000)
        time.sleep(2)
        if not device.cwww.check_state_ctl(element_id=3, expected_lightness=25000, expected_temp=6000):
            self.log_error("CWWW Temperature Set failed")
            return False

        return True

