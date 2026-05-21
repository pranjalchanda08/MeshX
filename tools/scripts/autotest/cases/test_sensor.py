from framework.base import TestBase
import time

class TestSensorCommunication(TestBase):
    name = "Sensor_MXCP"
    required_nodes = 1

    def run(self):
        device = self.devices[0]
        self.log_info(f"Sensor Test on {device.name}")
        
        # Trigger Sensor Data Set
        val = 1234
        self.log_info(f"Triggering Sensor Data Set: {val}")
        device.sensor.set_data(element_id=5, value=val)
        if not device.sensor.check_state(element_id=5, expected_value=val):
            self.log_error("Sensor Data Set failed")
            return False

        return True
