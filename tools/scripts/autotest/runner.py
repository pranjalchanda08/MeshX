import os
import sys
import argparse
import logging
import importlib
import inspect
from datetime import datetime

# Add the script directory to path so we can import framework
current_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.append(current_dir)

from framework.node import MeshXNode
from framework.device import MeshXDevice
from framework.base import TestBase
from framework.reporter import Reporter

def run_build(bsp, logger):
    """Builds the firmware for a specific BSP."""
    logger.info(f"Building firmware for {bsp}...")
    cmd = [
        sys.executable,
        os.path.abspath(os.path.join(current_dir, '..', 'meshx.py')),
        "-b",
        "-B", bsp,
        "-N", "all_in_one",
        "-D", "ENABLE_TESTS=1",
        "-D", "CONFIG_MESHX_DEFAULT_LOG_LEVEL=MESHX_LOG_DEBUG"
    ]
    try:
        import subprocess
        subprocess.run(cmd, check=True)
        logger.info(f"Successfully built {bsp}")
        return True
    except Exception as e:
        logger.error(f"Failed to build {bsp}: {e}")
        return False

def run_flash(port, bsp, logger):
    """Flashes the pre-built firmware to a specific port."""
    logger.info(f"Flashing {bsp} firmware to {port}...")
    cmd = [
        sys.executable,
        os.path.abspath(os.path.join(current_dir, '..', 'meshx.py')),
        "-F", "-P", port,
        "-B", bsp,
        "-N", "all_in_one",
        # Note: -D flags aren't strictly needed for flashing but good for context
    ]
    try:
        import subprocess
        subprocess.run(cmd, check=True)
        logger.info(f"Successfully flashed {port}")
        return True
    except Exception as e:
        logger.error(f"Failed to flash {port}: {e}")
        return False

def setup_logging(log_dir):
    logger = logging.getLogger()
    logger.setLevel(logging.INFO)
    
    # Console
    c_handler = logging.StreamHandler()
    c_handler.setFormatter(logging.Formatter('%(asctime)s [%(levelname)s] %(name)s: %(message)s'))
    logger.addHandler(c_handler)
    
    # File
    f_handler = logging.FileHandler(os.path.join(log_dir, "runner.log"))
    f_handler.setFormatter(logging.Formatter('%(asctime)s [%(levelname)s] %(name)s: %(message)s'))
    logger.addHandler(f_handler)
    return logger

def discover_tests(cases_dir):
    tests = []
    for filename in os.listdir(cases_dir):
        if filename.startswith("test_") and filename.endswith(".py"):
            module_name = f"cases.{filename[:-3]}"
            try:
                module = importlib.import_module(module_name)
                for name, obj in inspect.getmembers(module):
                    if inspect.isclass(obj) and issubclass(obj, TestBase) and obj is not TestBase:
                        tests.append(obj)
            except Exception as e:
                print(f"Error loading {module_name}: {e}")
    return tests

def main():
    parser = argparse.ArgumentParser(description="MeshX Grid Test Runner")
    parser.add_argument("-b", "--build", required=True, help="Nodes in format BSP:PORT,BSP:PORT")
    parser.add_argument("--skip-flash", action="store_true", help="Skip the build and flash step")
    parser.add_argument("--cases", default="tools/scripts/autotest/cases", help="Directory containing tests")
    parser.add_argument("--filter", help="Filter tests by name")
    parser.add_argument("--baud", type=int, default=115200)
    
    args = parser.parse_args()
    
    # Setup Log Dir
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    log_dir = os.path.join("tools/scripts/autotest/logs", f"session_{timestamp}")
    os.makedirs(log_dir, exist_ok=True)
    
    logger = setup_logging(log_dir)
    reporter = Reporter(log_dir)

    # Parse node configurations
    node_configs = []
    for item in args.build.split(','):
        bsp, port = item.split(':')
        node_configs.append({'bsp': bsp, 'port': port})

    # Build and Flash
    if not args.skip_flash:
        # 1. Unique BSPs to build
        unique_bsps = set(config['bsp'] for config in node_configs)
        for bsp in unique_bsps:
            if not run_build(bsp, logger):
                logger.error(f"Failing test suite due to build failure on {bsp}")
                sys.exit(1)
        
        # 2. Flash all nodes
        for config in node_configs:
            if not run_flash(config['port'], config['bsp'], logger):
                logger.error(f"Failing test suite due to flash failure on {config['port']}")
                sys.exit(1)
    
    # Connect to Nodes
    nodes = []
    devices = []
    for i, config in enumerate(node_configs):
        port = config['port']
        node_log = os.path.join(log_dir, f"serial_node_{i}.log")
        node = MeshXNode(port, args.baud, name=f"Node-{i}", log_file=node_log)
        if node.connect():
            # Reset target before starting tests to ensure clean state
            node.hard_reset(config['bsp'])
            import time
            time.sleep(2) # Delay to allow modules to boot properly
            # Trigger hosted mode for binary transport
            node.send_command("ut 8 1 1 1")
            nodes.append(node)
            devices.append(MeshXDevice(node))
        else:
            logger.error(f"Failed to connect to {port}")
            sys.exit(1)
            
    # Discover Tests
    test_classes = discover_tests(os.path.join(current_dir, "cases"))
    if args.filter:
        test_classes = [t for t in test_classes if args.filter in t.__name__]
        
    logger.info(f"Discovered {len(test_classes)} tests")
    
    # Run Tests
    for test_class in test_classes:
        if len(devices) < test_class.required_nodes:
            logger.warning(f"Skipping {test_class.__name__}: Requires {test_class.required_nodes} nodes, only {len(devices)} available")
            continue
            
        test_instance = test_class(devices[:test_class.required_nodes])
        logger.info(f"Running test: {test_class.__name__}")
        
        try:
            if test_instance.setup():
                success = test_instance.run()
                test_instance.results.success = success
            else:
                test_instance.results.error = "Setup failed"
        except Exception as e:
            test_instance.results.success = False
            test_instance.results.error = str(e)
            logger.exception(f"Test {test_class.__name__} crashed")
        finally:
            test_instance.teardown()
            reporter.add_result(test_instance.results)
            
    reporter.generate_summary()
    
    # Disconnect
    for node in nodes:
        node.disconnect()

if __name__ == "__main__":
    main()
