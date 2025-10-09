cmake_minimum_required(VERSION 3.20)
# ------------------------------------------------------------------------------
# Options
# ------------------------------------------------------------------------------
set(MESHX_VERSION "0.3.4" CACHE STRING "MESHX Build Version")
set(BSP "weact_c3" CACHE STRING "BSP Selection")
set(ELF meshx_build CACHE STRING "Name of the final executable")
set(PROD_PROFILE "port/bsp/${BSP}/prod_profile.yml" CACHE STRING "Path to the product profile YAML file")

# ------------------------------------------------------------------------------
# INTERNAL CACHE VARIABLES
# ------------------------------------------------------------------------------

set(OS "FreeRTOS" CACHE INTERNAL "OS abstraction layer to be set by BSP")
set(SRC_FILES "" CACHE INTERNAL "Source files for the project")
set(INC_FILES "" CACHE INTERNAL "Include directories for the project")
set(PLAT_LIBS "" CACHE INTERNAL "Platform libraries to link against")
set(MESHX_BUILD_TYPE "Release" CACHE INTERNAL "MeshX build type")
set(PLATFORM_TEST_SUPPORTED ON CACHE INTERNAL "Testing supported in platform")

add_definitions(-DMESHX_BUILD_TYPE=${MESHX_BUILD_TYPE})

if(MESHX_BUILD_TYPE STREQUAL "Debug")
    option(ENABLE_TESTS "Enable building unit tests" ON)
    # add_definitions(-DDEBUG)
endif()
# ------------------------------------------------------------------------------
# BSP
# ------------------------------------------------------------------------------
include(port/bsp/${BSP}/bsp.cmake)

# ------------------------------------------------------------------------------
# Remove duplicate source files
list(REMOVE_DUPLICATES SRC_FILES)
list(REMOVE_DUPLICATES INC_FILES)
# ------------------------------------------------------------------------------
include_directories(${INC_FILES})

message(STATUS "     >>>> INC_FILES: ${INC_FILES}")
message(STATUS "     >>>> PLAT_LIBS: ${PLAT_LIBS}")

project(${ELF}
        VERSION ${MESHX_VERSION}
        DESCRIPTION "MeshX"
        LANGUAGES C
    )

