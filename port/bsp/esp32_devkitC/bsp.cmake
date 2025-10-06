# ESP32-DevKitC Board specific configuration
set(BOARD_NAME "ESP32-DevKitC")
set(BOARD_MCU "esp32")
set(MCU_FAMILY "esp")

# BSP uses FreeRTOS as the OS
set(OS "FreeRTOS")

# Include common BSP settings
include(${CMAKE_SOURCE_DIR}/port/bsp/bsp_common.cmake)
# Include Platform path
include(${CMAKE_SOURCE_DIR}/port/platform/${MCU_FAMILY}/${BOARD_MCU}/${BOARD_MCU}.cmake)
