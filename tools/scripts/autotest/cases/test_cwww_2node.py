from framework.base import TestBase
import time

class TestCWWWCommunication2Node(TestBase):
    name = "CWWW_2Node"
    required_nodes = 2

    def run(self):
        server = self.devices[0]
        client = self.devices[1]

        self.log_info(f"CWWW 2-Node Test: Server={server.name}, Client={client.name}")

        # Configure the Client (Node-1, element 4) to publish to 0xC000
        self.log_info(f"Configuring {client.name} CWWW Client to publish to 0xC000...")
        client.cwww.configure_pub(element_id=4, pub_addr=0xC000, app_id=0)
        time.sleep(1)

        # 1. Test OnOff Set (Ack)
        self.log_info(f"Triggering CWWW OnOff Set (Ack) from {client.name}...")
        client.cwww.set_onoff(element_id=4, state=1, ack=True)
        time.sleep(3)
        server.cwww.check_state_onoff(element_id=3, expected_state=1)

        # 2. Test OnOff Set (Unack)
        self.log_info(f"Triggering CWWW OnOff Set (Unack) from {client.name}...")
        client.cwww.set_onoff(element_id=4, state=0, ack=False)
        time.sleep(3)
        server.cwww.check_state_onoff(element_id=3, expected_state=0)

        # 3. Test CTL Set (Ack)
        target_lightness = 20000
        target_temp = 5000
        self.log_info(f"Triggering CWWW CTL Set (Ack) from {client.name}: light={target_lightness}, temp={target_temp}")
        client.cwww.set_ctl(element_id=4, temp=target_temp, lightness=target_lightness, ack=True)
        time.sleep(3)
        server.cwww.check_state_ctl(element_id=3, expected_lightness=target_lightness, expected_temp=target_temp)

        # 4. Test CTL Set (Unack)
        target_lightness = 25000
        target_temp = 4000
        self.log_info(f"Triggering CWWW CTL Set (Unack) from {client.name}: light={target_lightness}, temp={target_temp}")
        client.cwww.set_ctl(element_id=4, temp=target_temp, lightness=target_lightness, ack=False)
        time.sleep(3)
        server.cwww.check_state_ctl(element_id=3, expected_lightness=target_lightness, expected_temp=target_temp)

        # 5. Test Individual Attribute Sets
        self.log_info(f"Triggering CWWW Individual Attribute Sets from {client.name}...")
        
        # Lightness (Ack)
        client.cwww.set_lightness(element_id=4, lightness=30000)
        time.sleep(3)
        server.cwww.check_state_ctl(element_id=3, expected_lightness=30000, expected_temp=target_temp)

        # Temperature (Unack)
        client.cwww.set_temperature(element_id=4, temp=3500, ack=False)
        time.sleep(3)
        server.cwww.check_state_ctl(element_id=3, expected_lightness=30000, expected_temp=3500)

        return True

