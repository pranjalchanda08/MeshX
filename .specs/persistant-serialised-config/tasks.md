# Task Breakdown: Persistent Serialized Configuration (Protobuf)

## Wave 1: Tooling and Partition Updates (Independent/Safe)
*These tasks establish the foundation and tooling required to generate and flash the Protobuf configuration binary.*

- [x] **Task 1.1**: Update `port/platform/esp/esp_idf/partitions.csv`.
  - Add the `meshx_cfg` partition (4KB, data, 0x40 subtype) at offset `0x11000`. *(Completed)*
- [x] **Task 1.2**: Enhance `tools/scripts/meshx.py`.
  - Add `--flash-cfg` and `--erase-cfg` commands to selectively manage the `meshx_cfg` partition via `esptool`. *(Completed)*
- [x] **Task 1.3**: Define Protobuf Schema.
  - Create `tools/scripts/proto/meshx_config.proto`.
  - Define messages for `ProductInfo`, `ElementConfig`, `GpioConfig`, `IoBinding`, and the root `MeshXConfig`.
- [x] **Task 1.4**: Extend `tools/scripts/code_gen.py`.
  - Generate python bindings (`meshx_config_pb2.py`) using `protoc`.
  - Update `code_gen.py` to instantiate `MeshXConfig`, populate it from `prod_profile.yml`, and serialize it.
  - Generate the 9-byte header including Magic, Version, Length, and `CRC-16-CCITT` calculations.
  - Write out the `meshx_cfg.bin` file.

## Wave 2: Firmware Loader and Boot Logic (Depends on Wave 1)
*These tasks implement the C-side parser and integrate it into the MeshX boot sequence.*

- [x] **Task 2.1**: Integrate Nanopb.
  - Add `nanopb` to the CMake build system to generate `meshx_config.pb.c` and `.pb.h` from the `.proto` file during the build process.
- [x] **Task 2.2**: Implement `meshx_ro_cfg.c` and `meshx_ro_cfg.h`.
  - Implement `meshx_ro_cfg_init()` to locate the `meshx_cfg` partition using FAL.
  - Implement `CRC-16-CCITT` validation for the header and payload.
- [x] **Task 2.3**: Implement Protobuf decoding and dispatch.
  - Read the payload into a buffer and invoke `pb_decode`.
  - Iterate over the decoded `MeshXConfig` structure.
  - Apply `ProductInfo` to global `cid`/`pid`.
  - Call `meshx_builder_add_element()` for `ElementConfig`.
  - Initialize hardware and IO mapping for `GpioConfig` and `IoBinding`.
- [x] **Task 2.4**: Integrate loader into `meshx_init()`.
  - Ensure configuration is fully loaded and elements are built before `meshx_tasks_init()` and stack initialization. flow, before standard element configuration.
- [x] **Task 2.5**: Integrate CRC-16-CCITT for data integrity validation.
- [x] **Task 2.6**: Implement early-boot loader in `meshx_init()`.
- [x] **Task 2.7**: Finalize on-device verification and error handling.
