#!/usr/bin/env python3
"""
Zero-Gateway Bootstrapping (ZGB) Protocol Engine & State Machine Simulator.

Implements authoritative opaque-box protocol definitions, frame codecs,
state machines, and validation oracles for MeshX ZGB (Cases 1, 2, and 3).
"""

import math
import os
import re
import struct
from typing import Dict, List, Optional, Set, Tuple

# ==============================================================================
# MXCP Constants & Protocol Framing
# ==============================================================================
MXCP_PREAMBLE = 0xFE
MXCP_TRAILER = 0xEF

# MXCP Commands
MXCP_CMD_PROV_ENABLE = 0x0B
MXCP_CMD_PROV_DISABLE = 0x0C
MXCP_CMD_PROV_GET_STATE = 0x0D
MXCP_CMD_PROV_SET_NAME = 0x0E
MXCP_CMD_PROV_AUTH_INPUT = 0x0F
MXCP_CMD_BOOTSTRAP_COMMISSION = 0x39
MXCP_CMD_BOOTSTRAP_ABORT = 0x3A

# MXCP Events
MXCP_EVT_PROV_COMP = 0x01
MXCP_EVT_PROV_FAILED = 0x02
MXCP_EVT_PROV_START = 0x03
MXCP_EVT_PROV_STATE_IND = 0x8C
MXCP_EVT_PROV_LINK_OPEN = 0x8D
MXCP_EVT_PROV_LINK_CLOSE = 0x8E
MXCP_EVT_PROV_AUTH_REQ = 0x8F

# Provisioning States (MXCP state codes)
PROV_STATE_UNPROVISIONED = 0x00
PROV_STATE_BEACONING = 0x01
PROV_STATE_PROVISIONING = 0x02
PROV_STATE_PROVISIONED = 0x03
PROV_STATE_COMMISSIONING = 0x04
PROV_STATE_SUPPRESSED = 0x05

# Provisioner Roles (meshx_group_allocator)
MESHX_PROV_ROLE_STANDALONE = 0
MESHX_PROV_ROLE_MASTER = 1
MESHX_PROV_ROLE_STANDBY = 2
MESHX_PROV_ROLE_CANDIDATE = 3
MESHX_PROV_ROLE_STALE = 4

# Address Constants
MESHX_ADDR_UNASSIGNED = 0x0000
MESHX_ADDR_ANCHOR_MASTER = 0x0001
MESHX_ADDR_POOL_START = 0x0002
MESHX_ADDR_POOL_END = 0x7FFF

# Company ID
MESHX_COMPANY_ID = 0x02E5

# UVP Opcodes
UVP_CMD_RPR_SCAN_START = 0xC0
UVP_CMD_RPR_SCAN_STOP = 0xC1
UVP_CMD_RPR_TUNNEL_DOWN = 0xC2
UVP_CMD_RPR_TUNNEL_UP = 0xC3
UVP_CMD_REDUNDANCY_HEARTBEAT = 0xC7
UVP_CMD_WAVEFRONT_LEASE_REQ = 0xC8
UVP_CMD_WAVEFRONT_LEASE_RESP = 0xCC
UVP_CMD_REHEAL_BEACON = 0xCE
UVP_CMD_REHEAL_RESPONSE = 0xCF

# Error Codes
MESHX_SUCCESS = 0
MESHX_ERR_INVALID_ARG = -1
MESHX_ERR_INVALID_STATE = -2
MESHX_ERR_PROVISIONING_FAILED = -3
MESHX_ERR_WRITE_FAIL = -4
MESHX_ERR_OUT_OF_ADDR = -5
MESHX_ERR_BUSY = -6
MESHX_ERR_ZOMBIE_REJECTED = -7


class MXCPProtocol:
    """Encoder and decoder for MXCP host serial protocol frames."""

    @staticmethod
    def calculate_checksum(cmd: int, payload: bytes) -> int:
        return (len(payload) + cmd + sum(payload)) & 0xFF

    @classmethod
    def encode(cls, cmd: int, payload: bytes = b"") -> bytes:
        length = len(payload)
        chk = cls.calculate_checksum(cmd, payload)
        return bytes([MXCP_PREAMBLE, length, cmd]) + payload + bytes([chk, MXCP_TRAILER])

    @classmethod
    def decode(cls, frame: bytes) -> Tuple[bool, int, bytes, Optional[str]]:
        if len(frame) < 5:
            return False, 0, b"", "Frame too short"
        if frame[0] != MXCP_PREAMBLE:
            return False, 0, b"", f"Invalid preamble: 0x{frame[0]:02X}"
        if frame[-1] != MXCP_TRAILER:
            return False, 0, b"", f"Invalid trailer: 0x{frame[-1]:02X}"

        length = frame[1]
        cmd = frame[2]
        expected_len = 1 + 1 + 1 + length + 1 + 1
        if len(frame) != expected_len:
            return False, 0, b"", f"Length mismatch: header={length}, actual={len(frame)}"

        payload = frame[3 : 3 + length]
        received_chk = frame[3 + length]
        expected_chk = cls.calculate_checksum(cmd, payload)
        if received_chk != expected_chk:
            return False, 0, b"", f"Checksum mismatch: received=0x{received_chk:02X}, expected=0x{expected_chk:02X}"

        return True, cmd, payload, None


# ==============================================================================
# BLE Mesh Beacon Codec
# ==============================================================================
class BeaconCodec:
    """Encoder/Decoder for BT Mesh PB-ADV Unprovisioned Beacons with ZGB Extensions."""

    @staticmethod
    def pack_unprovisioned_beacon(
        uuid: bytes,
        oob_info: int = 0x0000,
        uri_hash: Optional[bytes] = None,
        is_commissioning: bool = False,
    ) -> bytes:
        """
        Packs PB-ADV unprovisioned beacon.
        Structure:
        - AD Length: 1 byte
        - AD Type: 0x29 (Mesh Beacon)
        - Beacon Type: 0x00 (Unprovisioned Device Beacon)
        - Device UUID: 16 bytes
        - OOB Info: 2 bytes (big endian)
        - Optional Flags / URI Hash: URI hash (4 bytes) or commissioning flag (1 byte)
        """
        assert len(uuid) == 16, "UUID must be exactly 16 bytes"
        payload = bytes([0x00]) + uuid + struct.pack(">H", oob_info)
        if is_commissioning:
            # ZGB Case-3 extension: commissioning flag + 4-byte UUID prefix
            payload += bytes([0x01]) + uuid[:4]
        elif uri_hash:
            payload += uri_hash[:4]

        ad_length = len(payload) + 1  # includes AD Type
        return bytes([ad_length, 0x29]) + payload

    @staticmethod
    def unpack_beacon(raw_bytes: bytes) -> Dict:
        if len(raw_bytes) < 21:
            raise ValueError(f"Beacon length {len(raw_bytes)} too short for PB-ADV unprovisioned beacon")
        ad_len = raw_bytes[0]
        ad_type = raw_bytes[1]
        if ad_type != 0x29:
            raise ValueError(f"Invalid AD Type 0x{ad_type:02X}, expected 0x29")
        beacon_type = raw_bytes[2]
        if beacon_type != 0x00:
            raise ValueError(f"Invalid Beacon Type 0x{beacon_type:02X}, expected 0x00")

        uuid = raw_bytes[3:19]
        oob_info = struct.unpack(">H", raw_bytes[19:21])[0]

        # Extract trait mask from UUID bytes 8..11 (big endian uint32 or uint16)
        trait_mask = struct.unpack(">H", uuid[8:10])[0]

        is_commissioning = False
        prefix = uuid[:4]
        if len(raw_bytes) >= 26 and raw_bytes[21] == 0x01:
            is_commissioning = True
            prefix = raw_bytes[22:26]

        return {
            "ad_len": ad_len,
            "ad_type": ad_type,
            "beacon_type": beacon_type,
            "uuid": uuid,
            "oob_info": oob_info,
            "trait_mask": trait_mask,
            "is_commissioning": is_commissioning,
            "commission_prefix": prefix,
        }


# ==============================================================================
# Transactional NVS Simulator
# ==============================================================================
class SimulatedNVS:
    """Simulates MeshX NVS with transactional commit/rollback and sanitization audit."""

    def __init__(self):
        self.committed_store: Dict[str, bytes] = {}
        self.staged_writes: Dict[str, bytes] = {}
        self.in_transaction: bool = False
        self.log_stream: List[str] = []
        self.fail_on_key: Optional[str] = None

    def begin(self) -> int:
        self.staged_writes = {}
        self.in_transaction = True
        return MESHX_SUCCESS

    def set(self, key: str, value: bytes) -> int:
        if not self.in_transaction:
            # Auto-commit mode
            self.committed_store[key] = bytes(value)
            return MESHX_SUCCESS

        self.staged_writes[key] = bytes(value)
        return MESHX_SUCCESS

    def get(self, key: str) -> Optional[bytes]:
        return self.committed_store.get(key)

    def commit(self) -> int:
        if not self.in_transaction:
            return MESHX_ERR_INVALID_STATE

        # Check for simulated write failure
        for k, v in self.staged_writes.items():
            if self.fail_on_key and self.fail_on_key in k:
                # Write failed during iteration -> must rollback
                self.rollback()
                self.log(f"commit failed for key: {k}")
                return MESHX_ERR_WRITE_FAIL

        # Atomically apply all staged writes
        self.committed_store.update(self.staged_writes)
        self.staged_writes = {}
        self.in_transaction = False
        self.log("NVS transaction committed successfully.")
        return MESHX_SUCCESS

    def rollback(self) -> int:
        self.staged_writes.clear()
        self.in_transaction = False
        self.log("NVS transaction rolled back cleanly.")
        return MESHX_SUCCESS

    def log(self, msg: str):
        self.log_stream.append(msg)

    def format_devkey_key(self, uuid: bytes) -> str:
        prefix = uuid[:4].hex().upper()
        return f"dk_{prefix}"

    def audit_zero_key_leakage(self, keys: List[bytes]) -> bool:
        """Verifies that no raw hex or ASCII plaintext key bytes exist in the log stream."""
        full_logs = "\n".join(self.log_stream).lower()
        for k in keys:
            if not k:
                continue
            hex_str = k.hex().lower()
            if hex_str in full_logs:
                return False
        return True


# ==============================================================================
# HWRNG Key Generator
# ==============================================================================
class HWRNGKeyGenerator:
    """Simulates hardware RNG seeding from eFuse and radio entropy."""

    @staticmethod
    def calculate_shannon_entropy(data: bytes) -> float:
        if not data:
            return 0.0
        entropy = 0.0
        length = len(data)
        counts = {}
        for b in data:
            counts[b] = counts.get(b, 0) + 1
        for count in counts.values():
            p_x = count / length
            entropy -= p_x * math.log2(p_x)
        return entropy

    @classmethod
    def generate_key(cls, entropy_source: Optional[bytes] = None) -> bytes:
        if entropy_source is not None:
            raw = entropy_source[:16]
        else:
            raw = os.urandom(16)
        # Check for weak keys (all zeros or all ones)
        if raw == b"\x00" * 16 or raw == b"\xFF" * 16:
            raise ValueError("Weak key detected")
        return raw


# ==============================================================================
# ZGB Node Simulator (High-Fidelity State Machine)
# ==============================================================================
class ZGBNodeSimulator:
    """
    High-fidelity opaque-box simulator for a single MeshX node running ZGB logic.
    Follows state machine rules and interface contracts from PROJECT.md.
    """

    def __init__(self, uuid: bytes, element_count: int = 1):
        assert len(uuid) == 16, "UUID must be 16 bytes"
        self.uuid = bytes(uuid)
        self.element_count = element_count
        self.trait_mask = element_count  # advertised in beacon UUID bytes 8..9

        self.nvs = SimulatedNVS()
        self.state = PROV_STATE_UNPROVISIONED
        self.role = MESHX_PROV_ROLE_STANDALONE
        self.unicast_addr = MESHX_ADDR_UNASSIGNED
        self.net_key: Optional[bytes] = None
        self.app_key: Optional[bytes] = None
        self.dev_key: Optional[bytes] = None
        self.net_key_id: int = 0

        self.commission_window_open: bool = False
        self.commission_timer_sec: float = 0.0
        self.suppressed_backoff_sec: float = 0.0
        self.beacon_gated: bool = False
        self.beacon_ttl_sec: Optional[float] = None
        self.oob_watchdog_sec: float = 0.0
        self.gpio_initialized: bool = False

        self.group_allocator_hint = MESHX_ADDR_POOL_START
        self.blacklist_vault: Set[bytes] = set()

    def boot(self):
        """Simulate power-on boot sequence."""
        # 1. Complete GPIO initialization first
        self.gpio_initialized = True

        # 2. Check persistence in NVS
        stored_prov = self.nvs.get("is_prov")
        if stored_prov == b"\x01":
            self.state = PROV_STATE_PROVISIONED
            self.beacon_gated = True
            stored_addr = self.nvs.get("node_addr")
            if stored_addr:
                self.unicast_addr = struct.unpack(">H", stored_addr)[0]
            stored_role = self.nvs.get("prov_role")
            if stored_role:
                self.role = struct.unpack(">B", stored_role)[0]
            return

        # 3. If unprovisioned, enter BEACONING
        self.state = PROV_STATE_BEACONING
        self.beacon_gated = False

    def is_provisioned(self) -> bool:
        return self.state == PROV_STATE_PROVISIONED

    def is_commissioning(self) -> bool:
        return self.commission_window_open

    def handle_gpio_button(self, hold_duration_sec: float) -> int:
        if not self.gpio_initialized:
            return MESHX_ERR_INVALID_STATE
        if self.is_provisioned():
            return MESHX_ERR_INVALID_STATE

        if hold_duration_sec >= 3.0:
            return self.open_commission_window()
        return MESHX_SUCCESS

    def handle_mxcp_command(self, cmd: int, payload: bytes = b"") -> Tuple[int, bytes]:
        if cmd == MXCP_CMD_PROV_ENABLE:
            if self.is_provisioned():
                return MESHX_ERR_INVALID_STATE, b""
            self.state = PROV_STATE_BEACONING
            self.beacon_gated = False
            return MESHX_SUCCESS, bytes([PROV_STATE_BEACONING])

        elif cmd == MXCP_CMD_PROV_DISABLE:
            self.state = PROV_STATE_UNPROVISIONED
            self.beacon_gated = True
            return MESHX_SUCCESS, bytes([PROV_STATE_UNPROVISIONED])

        elif cmd == MXCP_CMD_PROV_GET_STATE:
            return MESHX_SUCCESS, bytes([self.state])

        elif cmd == MXCP_CMD_BOOTSTRAP_COMMISSION:
            if self.is_provisioned():
                return MESHX_ERR_INVALID_STATE, b""
            res = self.open_commission_window()
            return res, bytes([self.state])

        elif cmd == MXCP_CMD_BOOTSTRAP_ABORT:
            self.commission_window_open = False
            if not self.is_provisioned():
                self.state = PROV_STATE_UNPROVISIONED
            return MESHX_SUCCESS, bytes([self.state])

        return MESHX_ERR_INVALID_ARG, b""

    def open_commission_window(self) -> int:
        if self.is_provisioned():
            return MESHX_ERR_INVALID_STATE
        self.commission_window_open = True
        self.commission_timer_sec = 60.0
        self.state = PROV_STATE_COMMISSIONING
        return MESHX_SUCCESS

    def on_external_prov_start(self) -> int:
        """External PB-ADV / PB-GATT arrives, pre-empting commission mode."""
        if self.commission_window_open:
            self.commission_window_open = False
            self.state = PROV_STATE_PROVISIONING
        return MESHX_SUCCESS

    def validate_capability_integrity(self, advertised_trait_mask: int, composition_element_count: int) -> bool:
        return advertised_trait_mask == composition_element_count

    def perform_atomic_self_provision(self, fault_key: Optional[str] = None) -> int:
        """Executes Case-2 atomic self-provisioning per ZGB-T08."""
        if self.is_provisioned():
            self.commission_window_open = False
            return MESHX_ERR_INVALID_STATE

        # 1. Key birth
        try:
            net_k = HWRNGKeyGenerator.generate_key()
            app_k = HWRNGKeyGenerator.generate_key()
            dev_k = HWRNGKeyGenerator.generate_key()
        except ValueError:
            return MESHX_ERR_INVALID_ARG

        # 2. Begin atomic transaction
        self.nvs.begin()
        self.nvs.fail_on_key = fault_key

        dk_key = self.nvs.format_devkey_key(self.uuid)
        self.nvs.set("net_key", net_k)
        self.nvs.set("app_key", app_k)
        self.nvs.set(dk_key, dev_k)
        self.nvs.set("node_addr", struct.pack(">H", MESHX_ADDR_ANCHOR_MASTER))
        self.nvs.set("prov_role", struct.pack(">B", MESHX_PROV_ROLE_MASTER))
        self.nvs.set("is_prov", b"\x01")

        err = self.nvs.commit()
        if err != MESHX_SUCCESS:
            # Rollback occurred in simulated NVS
            self.net_key = None
            self.app_key = None
            self.dev_key = None
            self.unicast_addr = MESHX_ADDR_UNASSIGNED
            self.state = PROV_STATE_UNPROVISIONED
            return err

        # Local stack init succeeds
        self.net_key = net_k
        self.app_key = app_k
        self.dev_key = dev_k
        self.unicast_addr = MESHX_ADDR_ANCHOR_MASTER
        self.role = MESHX_PROV_ROLE_MASTER
        self.state = PROV_STATE_PROVISIONED
        self.beacon_gated = True
        self.commission_window_open = False
        return MESHX_SUCCESS

    def min_uuid_arbitration(self, peer_beacon_info: Dict) -> bool:
        """
        Case-3 min-UUID listen-before-founding rule.
        Returns True if self wins, False if suppressed.
        """
        if not peer_beacon_info.get("is_commissioning"):
            return True  # Ignore non-commissioning peers

        peer_uuid = peer_beacon_info["uuid"]
        # Comparison: lowest UUID wins
        if peer_uuid < self.uuid:
            # Self is higher UUID -> suppress
            self.commission_window_open = False
            self.state = PROV_STATE_SUPPRESSED
            self.suppressed_backoff_sec = 60.0
            return False
        return True

    def absorb_case1_target(self, target: "ZGBNodeSimulator", tunneled_rpr: bool = False) -> int:
        """Anchor absorbs Case-1 target via native Prov Server."""
        if not self.is_provisioned() or self.role != MESHX_PROV_ROLE_MASTER:
            return MESHX_ERR_INVALID_STATE

        # Validate target capability trait mask
        if not self.validate_capability_integrity(target.trait_mask, target.element_count):
            return MESHX_ERR_PROVISIONING_FAILED

        # Allocate contiguous unicast addresses
        needed = target.element_count
        assigned_base = self.group_allocator_hint
        if assigned_base + needed - 1 > MESHX_ADDR_POOL_END:
            return MESHX_ERR_OUT_OF_ADDR

        self.group_allocator_hint += needed

        # Target receives credentials
        target_dev_key = HWRNGKeyGenerator.generate_key()
        target.net_key = self.net_key
        target.app_key = self.app_key
        target.dev_key = target_dev_key
        target.unicast_addr = assigned_base
        target.state = PROV_STATE_PROVISIONED
        target.beacon_gated = True

        # Persist target state into target's NVS
        target.nvs.begin()
        target.nvs.set("net_key", self.net_key)
        target.nvs.set("node_addr", struct.pack(">H", assigned_base))
        target.nvs.set("is_prov", b"\x01")
        target.nvs.commit()

        # Anchor records target's DevKey in anchor vault
        self.nvs.set(self.nvs.format_devkey_key(target.uuid), target_dev_key)
        return MESHX_SUCCESS

    def handle_split_brain_contact(self, peer_uuid: bytes, peer_net_key: bytes) -> bool:
        """
        Evaluates split brain contact.
        If NetKey mismatches, node with higher UUID executes factory reset.
        Returns True if self survived, False if self reset.
        """
        if self.net_key == peer_net_key:
            return True  # Same network, no conflict

        # NetKey mismatch detected!
        if self.uuid > peer_uuid:
            # Loser: factory reset
            self.factory_reset()
            return False
        else:
            # Winner: retains 0x0001
            return True

    def factory_reset(self):
        """Wipes NVS and returns node to fresh factory unprovisioned state."""
        self.nvs = SimulatedNVS()
        self.state = PROV_STATE_UNPROVISIONED
        self.role = MESHX_PROV_ROLE_STANDALONE
        self.unicast_addr = MESHX_ADDR_UNASSIGNED
        self.net_key = None
        self.app_key = None
        self.dev_key = None
        self.beacon_gated = False
        self.commission_window_open = False
        self.suppressed_backoff_sec = 0.0


# ==============================================================================
# Firmware & Architecture Auditor
# ==============================================================================
class FirmwareAuditor:
    """Performs static and architectural compliance checks across project artifacts."""

    @staticmethod
    def audit_unified_binary_invariant(sdkconfig_path: str) -> Dict[str, bool]:
        """Verifies CONFIG_BLE_MESH_PROVISIONER=n and CONFIG_BLE_MESH_PROV=y."""
        if not os.path.exists(sdkconfig_path):
            return {"exists": False, "provisioner_disabled": False, "prov_srv_enabled": False}

        with open(sdkconfig_path, "r", encoding="utf-8", errors="ignore") as fh:
            content = fh.read()

        provisioner_disabled = (
            "# CONFIG_BLE_MESH_PROVISIONER is not set" in content or "CONFIG_BLE_MESH_PROVISIONER=n" in content
        )
        prov_srv_enabled = "CONFIG_BLE_MESH_PROV=y" in content

        return {
            "exists": True,
            "provisioner_disabled": provisioner_disabled,
            "prov_srv_enabled": prov_srv_enabled,
        }

    @staticmethod
    def audit_cpp_header_encapsulation(root_dir: str) -> List[Dict]:
        """
        Scans all .cpp and .hpp files to ensure no C headers are included directly,
        verifying they adhere to the coding guideline rule.
        """
        violations = []
        c_header_regex = re.compile(r'^\s*#include\s+[<"]([a-zA-Z0-9_\-\./]+\.h)[>"]')

        # Allowed C header through shim
        whitelist = {"meshx_c_header.h"}

        for dirpath, _, filenames in os.walk(root_dir):
            # Skip build, .git, .agents, tools, and external vendor code (modules, submodules, third_party)
            if any(skip in dirpath for skip in ["build", ".git", ".agents", "tools", "modules", "submodules", "third_party"]):
                continue
            for f in filenames:
                if f.endswith(".cpp") or f.endswith(".hpp"):
                    file_path = os.path.join(dirpath, f)
                    with open(file_path, "r", encoding="utf-8", errors="ignore") as fh:
                        for lineno, line in enumerate(fh, 1):
                            match = c_header_regex.match(line)
                            if match:
                                inc = match.group(1)
                                if inc not in whitelist and not inc.endswith("_base_model_class.hpp"):
                                    violations.append({
                                        "file": file_path,
                                        "line": lineno,
                                        "include": inc,
                                    })
        return violations

    @staticmethod
    def validate_flash_delta(zgb_text_bytes: int, zgb_data_bytes: int, budget_bytes: int = 15360) -> bool:
        """Verifies total ZGB flash footprint fits within 15 KB (REQ-NF01)."""
        return (zgb_text_bytes + zgb_data_bytes) <= budget_bytes
