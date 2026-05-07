import os
import logging
from datetime import datetime

class Reporter:
    """
    Handles test session reporting and logging.
    """
    def __init__(self, log_dir):
        self.log_dir = log_dir
        self.results = []
        self.session_log = os.path.join(log_dir, "session_summary.txt")
        
    def add_result(self, result):
        self.results.append(result)
        
    def generate_summary(self):
        total = len(self.results)
        passed = sum(1 for r in self.results if r.success)
        failed = total - passed
        
        with open(self.session_log, "w") as f:
            f.write("=== MeshX Test Session Summary ===\n")
            f.write(f"Timestamp: {datetime.now().isoformat()}\n")
            f.write(f"Total Tests:  {total}\n")
            f.write(f"Passed:       {passed}\n")
            f.write(f"Failed:       {failed}\n\n")
            
            for r in self.results:
                status = "PASS" if r.success else "FAIL"
                f.write(f"[{status}] {r.name}\n")
                if r.error:
                    f.write(f"      Error: {r.error}\n")
        
        print(f"\nResults: {passed} passed, {failed} failed out of {total} total")
        print(f"Detailed summary saved to {self.session_log}")
