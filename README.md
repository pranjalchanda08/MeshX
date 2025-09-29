# MeshX

```txt
*********************************************************************************************************************
* MMMMMMMM               MMMMMMMM                                     hhhhhhh                 XXXXXXX       XXXXXXX *
* M:::::::M             M:::::::M                                     h:::::h                 X:::::X       X:::::X *
* M::::::::M           M::::::::M                                     h:::::h                 X:::::X       X:::::X *
* M:::::::::M         M:::::::::M                                     h:::::h                 X::::::X      X:::::X *
* M::::::::::M       M::::::::::M    eeeeeeeeeeee        ssssssssss   h:::: hhhhhh            XX:::::X     X:::::XX *
* M:::::::::::M     M:::::::::::M  ee::::::::::::ee    ss::::::::::s  h::::::::::hhh            X:::::X   X:::::X   *
* M:::::::M::::M   M::::M:::::::M e::::::eeeee:::::eess:::::::::::::s h::::::::::::::hh           X:::::X:::::X     *
* M::::::M M::::M M::::M M::::::Me::::::e     e:::::es::::::ssss:::::sh:::::::hhh::::::h           X:::::::::X      *
* M::::::M  M::::M::::M  M::::::Me:::::::eeeee::::::e s:::::s  ssssss h::::::h   h::::::h          X:::::::::X      *
* M::::::M   M:::::::M   M::::::Me:::::::::::::::::e    s::::::s      h:::::h     h:::::h         X:::::X:::::X     *
* M::::::M    M:::::M    M::::::Me::::::eeeeeeeeeee        s::::::s   h:::::h     h:::::h        X:::::X X:::::X    *
* M::::::M     MMMMM     M::::::Me:::::::e           ssssss   s:::::s h:::::h     h:::::h     XXX:::::X   X:::::XXX *
* M::::::M               M::::::Me::::::::e          s:::::ssss::::::sh:::::h     h:::::h     X::::::X     X::::::X *
* M::::::M               M::::::M e::::::::eeeeeeee  s::::::::::::::s h:::::h     h:::::h     X:::::X       X:::::X *
* M::::::M               M::::::M  ee:::::::::::::e   s:::::::::::ss  h:::::h     h:::::h     X:::::X       X:::::X *
* MMMMMMMM               MMMMMMMM    eeeeeeeeeeeeee    sssssssssss    hhhhhhh     hhhhhhh     XXXXXXX       XXXXXXX *
*********************************************************************************************************************
```

[![CI Pipeline:main](https://github.com/pranjalchanda08/MeshX/actions/workflows/build_ci.yml/badge.svg)](https://github.com/pranjalchanda08/MeshX/actions/workflows/ci.yml) ![Release](https://img.shields.io/badge/Release-v0.4.3-blue)

MeshX is a portable C implementation of a Bluetooth Low Energy (BLE) Mesh node stack and example components. It is designed to be portable across board support packages (BSPs), microcontroller units (MCUs) and SDKs via a CMake-driven build system and small platform abstraction layers.

This README documents the new portability model, the BSP/MCU/SDK support available in this repository, and how to build MeshX using the CMake-level build system.

## Highlights
- Portable CMake-based build integration for multiple BSPs and MCUs
- Current BSP: `weact_c3` (WeAct ESP32-C3 board)
- Current MCU family: `esp` with supported MCU: `esp32c3`
- SDK integration: ESP-IDF (driven by the `IDF_PATH` environment variable)
- CMake options to select BSP, product profile, and build an executable

## Repository layout (relevant parts)

- `CMakeLists.txt` - Top-level CMake entry. Defines project variables and includes the selected BSP.
- `main/CMakeLists.txt` - Collects sources from `main/component/meshx` and registers the component with the platform.
- `port/bsp/` - BSPs live here. Each BSP contains board-specific CMake configuration and product profiles.
- `port/platform/esp/` - ESP platform integration (ESP-IDF glue and esp32c3 support files).
- `tools/scripts/` - Helper scripts and code-generation utilities (`code_gen.py`, `build.sh`, etc.).

## Supported BSPs / MCUs / SDKs (current)

### BSPs

| BSP name  | Board / Notes                         | Location (CMake)                        |
|-----------|----------------------------------------|-----------------------------------------|
| weact_c3  | WeAct ESP32-C3 development board      | `port/bsp/weact_c3/bsp.cmake`          |

### MCU families / targets

| MCU family | Target     | Notes / Location                                      |
|------------|------------|-------------------------------------------------------|
| esp        | esp32c3    | `port/platform/esp/esp32c3/esp32c3.cmake`             |

### SDKs / Integrations

| SDK       | Integration notes                                        | Required env / file                         |
|-----------|-----------------------------------------------------------|--------------------------------------------|
| ESP-IDF   | ESP-IDF integration via `port/platform/esp/esp_idf/esp_idf.cmake`. Uses `idf_component_register` under the hood. | `IDF_PATH` environment variable; run `$IDF_PATH/export.sh` |

If you want to add support for another BSP or MCU, add a directory under `port/bsp/` for the BSP and a corresponding `port/platform/<family>/<mcu>/<mcu>.cmake` for the MCU/platform glue.

## CMake-level build system

MeshX is driven from the top-level `CMakeLists.txt`. The most important CMake options you can set are:

- `-DBSP=<bsp-name>` — Select which BSP to include. Default: `weact_c3`.
- `-DPROD_NAME=<product_name>` — Required product name used by the code generation pipeline.
- `-DPROD_PROFILE=<path>` — Path to the product profile YAML (defaults to `port/bsp/${BSP}/prod_profile.yml`).
- `-DIDF_PATH=<path>` — (Exported as environment variable) The ESP-IDF path is read from the `IDF_PATH` environment variable.
- `-DELF=<executable_name>` — Name of the final CMake project/ELF. Default set in top-level CMake.
- `-DENABLE_TESTS=ON` — Enable unit tests if the chosen platform supports it.

### Examples — out-of-tree CMake build

1. Set up ESP-IDF environment (example — bash):

```bash
export IDF_PATH=/path/to/esp-idf
source $IDF_PATH/export.sh
```

2. Create build directory and configure CMake (replacing PROD_NAME):

```bash
mkdir -p build
cmake -S . -B build -DBSP=weact_c3 -DPROD_NAME=4_relay_panel -DPROD_PROFILE=../port/bsp/weact_c3/prod_profile.yml
cmake --build build
```

This will include the BSP CMake (`port/bsp/weact_c3/bsp.cmake`), which in turn includes the platform glue `port/platform/esp/esp32c3/esp32c3.cmake` and the ESP-IDF integration.

### Notes on environment and code generation

- `PROD_NAME` is required; `port/bsp/bsp_common.cmake` will call `tools/scripts/code_gen.py` to generate product-specific source at configure time. If `PROD_NAME` is not provided, CMake will stop with a helpful error.
- The build system registers MeshX sources with the platform via a helper `register_component()` function implemented in the platform glue (for ESP-IDF this maps to `idf_component_register`).

### Using the included helper scripts

- `tools/scripts/build.sh` and `tools/scripts/build_ci.sh` wrap the typical configure + build flow and drive `code_gen.py`. They are useful for CI or convenient local builds. You can inspect and adapt them to new BSPs or product profiles.

### Product and model profiles

- Product profiles live in `port/bsp/<bsp>/prod_profile.yml`. Add a product by adding an entry under the `products` mapping. The build scripts and `code_gen.py` will read this profile to generate product-specific composition files.
- Model profiles are under `tools/scripts/model_profile.yml` (used by the code generator).

### Writing a custom product profile (prod_profile.yml)

If you want to add a new product for a BSP, create or edit `port/bsp/<bsp>/prod_profile.yml` and add a product entry under the `products` mapping. The product profile describes the product id, CID, elements and optional components.

#### Minimum product entry example

```yaml
prod:
	cid: 0x7908
	products:
		- name: my_new_product
			pid: 0x0100
			elements:
				- switch_relay_server: 2
```

#### Fields explained
- prod.cid: Company identifier for the product set (hex integer).
- products[].name: Human- and tool-friendly product name (string). This name is used as `PROD_NAME` when invoking the build.
- products[].pid: Product ID (hex integer).
- products[].elements: Ordered list of elements used by the product. Each element is expressed as a mapping of element-name:instance-count. Element names must match keys in `tools/scripts/model_profile.yml` under `elements`.
- products[].components (optional): list of small component toggles, e.g. `- unit_test: true`.

#### Practical example (based on `tools/scripts/prod_profile.ci.yml`)

```yaml
prod:
	cid: 0x7908
	products:
		- name: 4_relay_panel
			pid: 0x0001
			elements:
				- switch_relay_server: 4
		- name: all_in_one
			pid: 0x0004
			elements:
				- switch_relay_client: 1
				- switch_relay_server: 1
				- light_cwww_server: 1
				- light_cwww_client: 1
			components:
				- unit_test: true
```

#### How MeshX uses the profile

- `tools/scripts/code_gen.py` reads the product profile and `tools/scripts/model_profile.yml` and constructs:
	- compile-time macros (e.g. `CONFIG_PID_ID`, `CONFIG_MAX_ELEMENT_COUNT`) written into `meshx_config.h`.
	- a list of source files and include directories from the selected elements and models so the build system can register them.

#### Testing a product profile locally

1. Choose your BSP and ensure you have the BSP's `prod_profile.yml` (for quick tests you can copy `tools/scripts/prod_profile.ci.yml` to `port/bsp/weact_c3/prod_profile.yml`).

2. Run the code generator directly to verify it finds the product and creates `meshx_config.h`:

```bash
# from repo root (example product name: 4_relay_panel)
python3 tools/scripts/code_gen.py 4_relay_panel --root main/component/meshx --config main/component/meshx/default/inc --profile tools/scripts/prod_profile.ci.yml

# Inspect the generated header
ls -l main/component/meshx/default/inc/meshx_config.h
cat main/component/meshx/default/inc/meshx_config.h
```

3. If the generator complains `Product Not Found`, check that `products[].name` exactly matches the name you passed to `code_gen.py` / `PROD_NAME`.

### Notes and tips

- Always re-run CMake (or the helper build script) after changing product profiles because the profile is processed at configure time.
- Use `tools/scripts/model_profile.yml` to see available element names and how they map to source paths and macros.
- Keep `pid` values unique within a `cid` namespace to avoid accidental conflicts when you test product identification.


### Flashing and monitoring (ESP-IDF)

When building for ESP-IDF targets, the typical flash+monitor flow uses `idf.py` provided by ESP-IDF. For example:

```bash
# from the build directory configured with ESP-IDF environment
idf.py -p /dev/ttyUSB0 flash monitor
```

If you are using the `build.sh` helper it will generate an IDF project and artifacts compatible with `idf.py`.

### Adding support for new boards, MCUs or SDKs

1. Add a BSP: create `port/bsp/<your_bsp>/bsp.cmake` and a `prod_profile.yml` for your board. Follow the pattern established by `weact_c3`.
2. Add a platform glue directory: `port/platform/<family>/<mcu>/<mcu>.cmake` that sets `BASE_PLAT_SRC`, `BASE_PLAT_INC`, `PLAT_LIBS`, and exposes `register_component()` that maps to your target SDK build helpers.
3. If integrating with a new SDK, provide a small `register_component()` implementation that wires MeshX sources into that SDK's build system (for ESP-IDF we call `idf_component_register`).

#### Templates and a step-by-step example

To make it easier to add a new BSP or MCU, this repository includes simple templates you can copy and adapt:

- `port/bsp/template_bsp/bsp.cmake` — minimal BSP CMake that validates `PROD_NAME` and includes platform glue.
- `port/bsp/template_bsp/prod_profile.yml` — example product profile you can start from.
- `port/platform/template_family/template_mcu/template_mcu.cmake` — minimal platform glue template you can adapt to your SDK.

Quick step-by-step: add a new BSP + MCU

1. Copy the BSP template and update variables:

```bash
cp -r port/bsp/template_bsp port/bsp/my_board
# Edit port/bsp/my_board/bsp.cmake and set BOARD_NAME, BOARD_MCU, MCU_FAMILY
```

2. Copy and edit the product profile for your board:

```bash
cp port/bsp/template_bsp/prod_profile.yml port/bsp/my_board/prod_profile.yml
# Edit prod_profile.yml: set cid, add products[].name, pid, and elements
```

3. Create platform glue for your MCU family/target by copying the template and filling in SDK specifics:

```bash
mkdir -p port/platform/my_family/my_mcu
cp port/platform/template_family/template_mcu/template_mcu.cmake port/platform/my_family/my_mcu/my_mcu.cmake
# Edit my_mcu.cmake to set BASE_PLAT_INC, BASE_PLAT_SRC, PLAT_LIBS and implement register_component() using your SDK's API
```

4. Configure a local CMake build to test:

```bash
mkdir -p build && cd build
cmake .. -DBSP=my_board -DPROD_NAME=example_product -DPROD_PROFILE=../port/bsp/my_board/prod_profile.yml
cmake --build .
```

5. Flash / monitor using your SDK's toolchain (for ESP-IDF use `idf.py`) after the build completes.

### Troubleshooting & tips

- If CMake fails with "PROD_NAME is not set", re-run CMake with `-DPROD_NAME=<your_product>`.
- Ensure `IDF_PATH` is exported and `source $IDF_PATH/export.sh` has been run when building ESP targets.
- `tools/scripts/code_gen.py` is invoked at configure time. If you change product profiles, re-run CMake to regenerate sources.

## Contributing

Contributions that add BSPs or improve portability are welcome. Please follow repository conventions and add CMake glue under `port/bsp` and `port/platform`.

## License

See `LICENSE.md` for licensing information.
