import ctypes
import os
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
TEST_DIR = os.path.dirname(os.path.abspath(__file__))
HOST_LIBC_DIR = os.path.join(TEST_DIR, "host_libc")
BUILD_DIR = os.path.join(TEST_DIR, "_build")

CLANG = r"C:\Program Files\LLVM\bin\clang.exe"
LLD_LINK = r"C:\Program Files\LLVM\bin\lld-link.exe"
RISCV_GCC = r"C:\Users\Pranjal Chanda\.espressif\tools\riscv32-esp-elf\esp-14.2.0_20241119\riscv32-esp-elf\bin\riscv32-esp-elf-gcc.exe"
RISCV_SIZE = r"C:\Users\Pranjal Chanda\.espressif\tools\riscv32-esp-elf\esp-14.2.0_20241119\riscv32-esp-elf\bin\riscv32-esp-elf-size.exe"

INCLUDES = [
    os.path.join(ROOT, "main", "component", "meshx"),
    os.path.join(ROOT, "main", "component", "meshx", "inc"),
    os.path.join(ROOT, "main", "component", "meshx", "inc", "interface"),
    HOST_LIBC_DIR,
]

SUITES = {
    "phase3": {
        "sources": [
            os.path.join(HOST_LIBC_DIR, "host_libc.c"),
            os.path.join(TEST_DIR, "host_platform_shim.c"),
            os.path.join(TEST_DIR, "test_phase3_router.c"),
            os.path.join(ROOT, "main", "component", "meshx", "src", "island_subnet_coex", "meshx_subnet_crc.c"),
            os.path.join(ROOT, "main", "component", "meshx", "src", "island_subnet_coex", "meshx_router_queue.c"),
            os.path.join(ROOT, "main", "component", "meshx", "src", "island_subnet_coex", "meshx_router_core.c"),
        ],
    },
    "phase4_5": {
        "sources": [
            os.path.join(HOST_LIBC_DIR, "host_libc.c"),
            os.path.join(TEST_DIR, "host_platform_shim.c"),
            os.path.join(TEST_DIR, "test_phase4_5_kr_nat.c"),
            os.path.join(ROOT, "main", "component", "meshx", "src", "island_subnet_coex", "meshx_subnet_crc.c"),
            os.path.join(ROOT, "main", "component", "meshx", "src", "island_subnet_coex", "meshx_subnet_kr.c"),
            os.path.join(ROOT, "main", "component", "meshx", "src", "island_subnet_coex", "meshx_nat_mgr.c"),
        ],
    },
    "phase2_5_6": {
        "sources": [
            os.path.join(HOST_LIBC_DIR, "host_libc.c"),
            os.path.join(TEST_DIR, "host_platform_shim.c"),
            os.path.join(TEST_DIR, "test_phase2_5_6_sec_bridge.c"),
            os.path.join(ROOT, "main", "component", "meshx", "src", "island_subnet_coex", "meshx_subnet_crc.c"),
            os.path.join(ROOT, "main", "component", "meshx", "src", "island_subnet_coex", "meshx_router_queue.c"),
            os.path.join(ROOT, "main", "component", "meshx", "src", "island_subnet_coex", "meshx_router_core.c"),
            os.path.join(ROOT, "main", "component", "meshx", "src", "island_subnet_coex", "meshx_subnet_kr.c"),
            os.path.join(ROOT, "main", "component", "meshx", "src", "island_subnet_coex", "meshx_nat_mgr.c"),
            os.path.join(ROOT, "main", "component", "meshx", "src", "island_subnet_coex", "meshx_subnet_nvs.c"),
            os.path.join(ROOT, "main", "component", "meshx", "src", "island_subnet_coex", "meshx_subnet_security.c"),
            os.path.join(ROOT, "main", "component", "meshx", "src", "island_subnet_coex", "meshx_subnet_discovery.c"),
            os.path.join(ROOT, "main", "component", "meshx", "src", "island_subnet_coex", "meshx_subnet_bridge.c"),
            os.path.join(ROOT, "main", "component", "meshx", "src", "island_subnet_coex", "meshx_island_subnet_coex.c"),
        ],
    },
}


def compile_and_run(suite_name: str) -> int:
    os.makedirs(BUILD_DIR, exist_ok=True)
    cfg = SUITES[suite_name]
    objs = []
    inc_flags = []
    for inc in INCLUDES:
        inc_flags.extend(["-I", inc])

    for src in cfg["sources"]:
        obj = os.path.join(BUILD_DIR, f"{suite_name}_{os.path.basename(src)}.obj")
        cmd = [
            CLANG,
            "-target", "x86_64-pc-windows-msvc",
            "-std=c11",
            "-Wall", "-Wextra", "-Werror",
            "-Wno-gnu-zero-variadic-macro-arguments",
            "-D_CRT_SECURE_NO_WARNINGS",
            "-DCONFIG_MESHX_ISLAND_SUBNET_COEXISTENCE=1",
            "-ffreestanding",
            "-fno-builtin",
            "-isystem", HOST_LIBC_DIR,
            *inc_flags,
            "-c", src,
            "-o", obj,
        ]
        res = subprocess.run(cmd, capture_output=True, text=True)
        if res.returncode != 0:
            print(f"[COMPILE FAIL] {src}\n{res.stdout}{res.stderr}")
            return 2
        objs.append(obj)

    dll_path = os.path.join(BUILD_DIR, f"{suite_name}.dll")
    link_cmd = [
        LLD_LINK,
        "/DLL",
        "/NOENTRY",
        "/NODEFAULTLIB",
        "/EXPORT:host_dll_run",
        f"/OUT:{dll_path}",
        *objs,
    ]
    res = subprocess.run(link_cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"[LINK FAIL] {suite_name}\n{res.stdout}{res.stderr}")
        return 3

    dll = ctypes.CDLL(dll_path)
    ABORT_CB = ctypes.CFUNCTYPE(None, ctypes.c_char_p)
    aborted = [False]

    def on_abort(msg: bytes):
        aborted[0] = True
        print(msg.decode("utf-8", errors="replace"), file=sys.stderr)

    cb = ABORT_CB(on_abort)
    log_ptr = ctypes.c_char_p()
    dll.host_dll_run.argtypes = [ABORT_CB, ctypes.POINTER(ctypes.c_char_p)]
    dll.host_dll_run.restype = ctypes.c_int
    rc = dll.host_dll_run(cb, ctypes.byref(log_ptr))
    if log_ptr.value:
        print(log_ptr.value.decode("utf-8", errors="replace"), end="")
    return 1 if (aborted[0] or rc != 0) else 0


if __name__ == "__main__":
    targets = sys.argv[1:] if len(sys.argv) > 1 else list(SUITES.keys())
    overall = 0
    for t in targets:
        rc = compile_and_run(t)
        if rc != 0:
            overall = rc
    sys.exit(overall)
