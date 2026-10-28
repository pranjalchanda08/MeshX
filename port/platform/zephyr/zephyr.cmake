# Copyright (c) 2024 - 2026 MeshX
# SPDX-License-Identifier: Apache-2.0

set(ZEPHYR_COMMON_DIR "${CMAKE_SOURCE_DIR}/port/platform/zephyr")
set(PLATFORM_TEST_SUPPORTED ON CACHE INTERNAL "Testing supported in platform" FORCE)

# ------------------------------------------------------------------------------
# Auto-detect DeviceTree Overlays: MCU Common + BSP Board Custom
# ------------------------------------------------------------------------------
set(ZEPHYR_OVERLAYS "")

# 1. Common MCU Overlay (e.g. flash partitions: mcuboot, storage, meshx-nvs, meshx-cfg)
if(BOARD_MCU AND EXISTS "${ZEPHYR_COMMON_DIR}/mcu/${BOARD_MCU}/${BOARD_MCU}.overlay")
    list(APPEND ZEPHYR_OVERLAYS "${ZEPHYR_COMMON_DIR}/mcu/${BOARD_MCU}/${BOARD_MCU}.overlay")
    message(STATUS "MeshX Zephyr: Found MCU overlay: ${ZEPHYR_COMMON_DIR}/mcu/${BOARD_MCU}/${BOARD_MCU}.overlay")
endif()

# 2. Board-specific IO Overlay from port/bsp/<bsp>/ (Single authoritative board overlay)
if(BSP)
    if(EXISTS "${CMAKE_SOURCE_DIR}/port/bsp/${BSP}/${BSP}.overlay")
        list(APPEND ZEPHYR_OVERLAYS "${CMAKE_SOURCE_DIR}/port/bsp/${BSP}/${BSP}.overlay")
        message(STATUS "MeshX Zephyr: Found BSP overlay: ${CMAKE_SOURCE_DIR}/port/bsp/${BSP}/${BSP}.overlay")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/port/bsp/${BSP}/zephyr.overlay")
        list(APPEND ZEPHYR_OVERLAYS "${CMAKE_SOURCE_DIR}/port/bsp/${BSP}/zephyr.overlay")
        message(STATUS "MeshX Zephyr: Found BSP overlay: ${CMAKE_SOURCE_DIR}/port/bsp/${BSP}/zephyr.overlay")
    endif()
endif()

# 3. Optional user extra overlay from command line (-DEXTRA_DTC_OVERLAY_FILE=...)
if(EXTRA_DTC_OVERLAY_FILE)
    list(APPEND ZEPHYR_OVERLAYS "${EXTRA_DTC_OVERLAY_FILE}")
endif()

if(ZEPHYR_OVERLAYS)
    set(DTC_OVERLAY_FILE "${ZEPHYR_OVERLAYS}" CACHE STRING "DeviceTree overlays" FORCE)
endif()

# ------------------------------------------------------------------------------
# Collect Zephyr Platform Sources & Includes
# ------------------------------------------------------------------------------
file(GLOB_RECURSE ZEPHYR_PLAT_SRC
    CONFIGURE_DEPENDS
    "${ZEPHYR_COMMON_DIR}/**/*.c"
)

file(GLOB_RECURSE ZEPHYR_INC_DIRS
    LIST_DIRECTORIES true
    "${ZEPHYR_COMMON_DIR}/**"
)
list(FILTER ZEPHYR_INC_DIRS INCLUDE REGEX "[/\\](inc|include)$")
list(REMOVE_DUPLICATES ZEPHYR_INC_DIRS)

set(PLATFORM_INC
    ${PLATFORM_INC}
    ${ZEPHYR_INC_DIRS}
    CACHE STRING "Zephyr platform include"
)

set(INC_FILES
    ${INC_FILES}
    ${PLATFORM_INC}
    ${CMAKE_SOURCE_DIR}/main/component/meshx/inc
)

set(SRC_FILES
    ${SRC_FILES}
    ${ZEPHYR_PLAT_SRC}
)

# ------------------------------------------------------------------------------
# MeshX register_component Implementation for Zephyr
# ------------------------------------------------------------------------------
function(register_component SRC INC LIBS)
    target_sources(app PRIVATE ${SRC})
    target_include_directories(app PRIVATE ${INC})
    target_compile_definitions(app PRIVATE
        CONFIG_APP_MAIN=main
        CONFIG_CPP=1
    )
    if(COMMAND zephyr_linker_sources)
        zephyr_linker_sources(SECTIONS "${CMAKE_SOURCE_DIR}/port/platform/zephyr/meshx_sections.ld")
    endif()
endfunction()
