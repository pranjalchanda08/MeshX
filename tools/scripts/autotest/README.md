# MeshX Automated Test Suite

This suite provides tools for automated testing of MeshX nodes over serial and BLE Mesh. It is designed to be portable and can be deployed on any Linux host, including a Raspberry Pi.

## Features
- **Multi-Node Support**: Control and monitor multiple nodes simultaneously.
- **Log Monitoring**: Real-time parsing of serial logs for success/failure patterns.
- **Unit Test Integration**: Leverages the on-device `ut` framework.
- **Portable**: Works on standard Linux with `pyserial`.

## Directory Structure
- `runner.py`: Main test orchestrator.
- `node.py`: Serial communication and log parsing abstraction.
- `provisioner.py`: (Planned) Linux-side BLE Mesh provisioning via BlueZ D-Bus.
- `cases/`: Individual test cases.

## Installation
Ensure you have Python 3 and the required dependencies installed:
```bash
pip install -r requirements.txt
```

## Usage
Run the test runner by specifying the serial ports of your nodes:
```bash
python3 runner.py --ports /dev/ttyACM0 /dev/ttyACM1
```

## BlueZ Mesh Provisioning (Planned)
To enable automated provisioning from the Linux host:
1. Ensure BlueZ version >= 5.50 is installed.
2. The `bluetooth-mesh` daemon must be running.
3. The runner will use `pydbus` to interact with `org.bluez.mesh1`.

### Setup for RPi
On Raspberry Pi OS:
```bash
sudo apt update
sudo apt install bluez bluetooth-mesh-daemon
pip3 install pydbus pyserial
```

## Adding New Tests
1. Define a new function in `runner.py` or a script in `cases/`.
2. Use `node.send_command()` to trigger a `ut` command.
3. Use `node.expect("pattern")` to verify the output.
