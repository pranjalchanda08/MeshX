cmake_minimum_required(VERSION 3.20)

# ------------------------------------------------------------------------------
# MeshX Project Settings & Options (Authoritative)
# ------------------------------------------------------------------------------
set(MESHX_VERSION "0.6.0" CACHE STRING "MESHX Build Version")
set(BSP "weact_c3" CACHE STRING "BSP Selection")
set(ELF meshx_build CACHE STRING "Name of the final executable")
set(PROD_PROFILE "port/bsp/${BSP}/prod_profile.yml" CACHE STRING "Path to the product profile YAML file")
set(MESHX_BUILD_TYPE "Release" CACHE STRING "MeshX build type")

set(SRC_FILES "" CACHE INTERNAL "Source files for the project")
set(INC_FILES "" CACHE INTERNAL "Include directories for the project")
set(PLAT_LIBS "" CACHE INTERNAL "Platform libraries to link against")
set(PLATFORM_TEST_SUPPORTED OFF CACHE INTERNAL "Testing supported in platform")

add_definitions(-DMESHX_BUILD_TYPE=${MESHX_BUILD_TYPE})
add_definitions(-DMESHX_VERSION="${MESHX_VERSION}")

if(MESHX_EXTRA_DEFS)
    foreach(def ${MESHX_EXTRA_DEFS})
        add_compile_definitions(${def})
    endforeach()
endif()

if(MESHX_BUILD_TYPE STREQUAL "Debug")
    option(ENABLE_TESTS "Enable building unit tests" ON)
endif()

# ------------------------------------------------------------------------------
# Include BSP (Configures BOARD_MCU, OSAL, Platform Port, and Overlays)
# ------------------------------------------------------------------------------
include(port/bsp/${BSP}/bsp.cmake)

# Remove duplicate source files and include directories
list(REMOVE_DUPLICATES SRC_FILES)
list(REMOVE_DUPLICATES INC_FILES)
include_directories(${INC_FILES})

# ------------------------------------------------------------------------------
# Target Project Initialization
# ------------------------------------------------------------------------------
if(MESHX_HOST STREQUAL "zephyr" OR (DEFINED BOARD AND (DEFINED ZEPHYR_BASE OR DEFINED ENV{ZEPHYR_BASE})))
    find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
    project(${ELF} LANGUAGES C CXX)
    add_subdirectory(main)
else()
    project(${ELF}
            VERSION ${MESHX_VERSION}
            DESCRIPTION "MeshX"
            LANGUAGES C
    )
endif()
