cmake_minimum_required(VERSION 3.30)

set(TARGET meshx.elf CACHE STRING "MeshX application name")

# set(LIB_LIST "" CACHE STRING "List of libraries to link")

project(${TARGET}
    VERSION 0.3
    DESCRIPTION "MeshX"
    LANGUAGES C
)
# ------------------------------------------------------------------------------
# Options
# ------------------------------------------------------------------------------
set(OS "FreeRTOS" CACHE STRING "OS abstraction layer to use (e.g. freertos, posix)")
set(BSP "weact_c3" CACHE STRING "BSP Selection")

option(ENABLE_TESTS "Enable building unit tests" OFF)

# ------------------------------------------------------------------------------
# Applications
# ------------------------------------------------------------------------------
add_subdirectory(port/bsp/${BSP})
add_subdirectory(apps)

# ------------------------------------------------------------------------------
# BSP
# ------------------------------------------------------------------------------
# ------------------------------------------------------------------------------
# Core
# ------------------------------------------------------------------------------

# ------------------------------------------------------------------------------
# Porting layer
# ------------------------------------------------------------------------------
# # add a condition here if the platform is esp32* then add esp in path as well like port/platform/esp/${PLATFORM}
# if (PLATFORM MATCHES "esp*")
#     add_subdirectory(port/platform/esp/${PLATFORM})
# else()
#     add_subdirectory(port/platform/${PLATFORM})
# endif()

# add_subdirectory(port/os/${OS})

# # add
# target_link_libraries(${TARGET} PUBLIC meshx)
# target_link_libraries(${TARGET} PUBLIC osal_${OS})
# target_link_libraries(${TARGET} PUBLIC plat_${PLATFORM})

# get_target_property(LIBS meshx.elf LINK_LIBRARIES)

# message(STATUS "Linking libraries: ${LIBS}")
