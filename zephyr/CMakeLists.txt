# Copyright (c) 2024 - 2026 MeshX
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_MESHX)
  zephyr_library()

  set(MESHX_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

  # Core sources (C and C++)
  file(GLOB_RECURSE MESHX_CORE_SRC
    "${MESHX_ROOT}/main/component/meshx/**/*.c"
    "${MESHX_ROOT}/main/component/meshx/**/*.cpp"
  )
  list(FILTER MESHX_CORE_SRC EXCLUDE REGEX "main/component/meshx/ble_mesh/elements_c/")

  # OSAL sources for Zephyr
  file(GLOB_RECURSE MESHX_OSAL_SRC
    "${MESHX_ROOT}/port/os/zephyr/*.c"
  )

  # Platform sources for Zephyr
  file(GLOB_RECURSE MESHX_PLAT_SRC
    "${MESHX_ROOT}/port/platform/zephyr/*.c"
  )

  # Nanopb sources
  set(NANOPB_SRC
    "${MESHX_ROOT}/third_party/nanopb/pb_common.c"
    "${MESHX_ROOT}/third_party/nanopb/pb_decode.c"
    "${MESHX_ROOT}/third_party/nanopb/pb_encode.c"
  )

  # Sources are compiled into target 'app' by register_component in main/CMakeLists.txt.
  # zephyr/CMakeLists.txt provides Kconfig, includes, and compile definitions.

  # Include directories
  file(GLOB_RECURSE MESHX_INC_DIRS LIST_DIRECTORIES true "${MESHX_ROOT}/main/component/meshx/*")
  set(FINAL_INC_DIRS "")
  foreach(dir ${MESHX_INC_DIRS})
    if(IS_DIRECTORY ${dir} AND (dir MATCHES "(/inc(/.*)?$|/include(/.*)?$)"))
      if(NOT dir MATCHES "main/component/meshx/ble_mesh/elements_c/")
        list(APPEND FINAL_INC_DIRS "${dir}")
      endif()
    endif()
  endforeach()
  list(REMOVE_DUPLICATES FINAL_INC_DIRS)

  zephyr_include_directories(
    ${FINAL_INC_DIRS}
    ${MESHX_ROOT}/main/component/meshx
    ${MESHX_ROOT}/main/component/meshx/inc
    ${MESHX_ROOT}/main/component/unit_test/inc
    ${MESHX_ROOT}/third_party/nanopb
    ${MESHX_ROOT}/tools/scripts/proto
    ${MESHX_ROOT}/port/platform/zephyr/ble_mesh/inc
    ${CMAKE_BINARY_DIR}/main
    ${CMAKE_BINARY_DIR}
  )

  zephyr_library_compile_definitions(
    MESHX_ZEPHYR_BUILD=1
    CONFIG_CPP=1
    CONFIG_APP_MAIN=main
  )
  if(DEFINED MESHX_SCHEMA_ID)
    zephyr_library_compile_definitions(MESHX_SCHEMA_ID=${MESHX_SCHEMA_ID})
  endif()

endif()
