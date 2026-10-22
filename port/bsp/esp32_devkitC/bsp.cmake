# ESP32-DevKitC Board specific configuration
set(BOARD_NAME "ESP32-DevKitC")
set(BOARD_MCU "esp32")

if(MESHX_HOST STREQUAL "zephyr" OR (DEFINED BOARD AND (DEFINED ZEPHYR_BASE OR DEFINED ENV{ZEPHYR_BASE})))
    set(OS "zephyr")
    set(MCU_FAMILY "zephyr")
    include(${CMAKE_SOURCE_DIR}/port/bsp/bsp_common.cmake)
    include(${CMAKE_SOURCE_DIR}/port/platform/zephyr/zephyr.cmake)
else()
    set(OS "FreeRTOS")
    set(MCU_FAMILY "esp")
    include(${CMAKE_SOURCE_DIR}/port/bsp/bsp_common.cmake)
    include(${CMAKE_SOURCE_DIR}/port/platform/${MCU_FAMILY}/${BOARD_MCU}/${BOARD_MCU}.cmake)
endif()

# Add BSP-specific GPIO source files and includes
set(PLATFORM_INC
    ${PLATFORM_INC}
    "${CMAKE_SOURCE_DIR}/port/bsp/${BSP}/gpio"
)

set(SRC_FILES
    ${SRC_FILES}
    "${CMAKE_SOURCE_DIR}/port/bsp/${BSP}/gpio/gpio_constraints.c"
)
