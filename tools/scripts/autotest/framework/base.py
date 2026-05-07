import logging

class TestResult:
    def __init__(self, name):
        self.name = name
        self.success = False
        self.error = None
        self.start_time = 0
        self.end_time = 0
        self.logs = []

class TestBase:
    """
    Base class for all MeshX automated tests.
    Subclasses should define 'required_nodes' and implement 'run()'.
    """
    name = "BaseTest"
    required_nodes = 1
    
    def __init__(self, devices, logger=None):
        self.devices = devices
        self.logger = logger or logging.getLogger(self.name)
        self.results = TestResult(self.name)

    def setup(self):
        """Prepare nodes for the test (e.g. clear logs, check connection)."""
        for device in self.devices:
            device.node.clear_logs()
        return True

    def run(self):
        """Main test logic. Return True on success, False on failure."""
        raise NotImplementedError("Tests must implement run()")

    def teardown(self):
        """Clean up after test."""
        pass

    def log_info(self, msg):
        self.logger.info(msg)
        self.results.logs.append(f"INFO: {msg}")

    def log_error(self, msg):
        self.logger.error(msg)
        self.results.logs.append(f"ERROR: {msg}")
