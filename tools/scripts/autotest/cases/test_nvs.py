from framework.base import TestBase
import time

class TestNVSBasic(TestBase):
    name = "NVS_Basic_Integrity"
    required_nodes = 1

    def run(self):
        device = self.devices[0]
        self.log_info(f"Testing NVS on {device.name}")

        # 1. Reset NVS state
        if not device.nvs.close():
             self.log_error("Failed to close NVS")
             return False
        
        time.sleep(0.5)
        
        if not device.nvs.open():
            self.log_error("Failed to open NVS")
            return False

        # 2. Erase and Set
        device.nvs.erase()
        if not device.nvs.set(key_id=1, value=100):
            self.log_error("Failed to set key 1")
            return False

        # 3. Verify
        if not device.nvs.get(key_id=1):
            self.log_error("Integrity check failed: could not retrieve key 1 correctly")
            return False

        self.log_info("NVS Integrity test passed")
        return True
