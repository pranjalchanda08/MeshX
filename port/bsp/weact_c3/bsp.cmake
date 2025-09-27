# WeAct C3 Board specific configuration
set(BOARD_NAME "WeAct C3")
set(BOARD_MCU "esp32c3")
set(MCU_FAMILY "esp")

# BSP uses FreeRTOS as the OS
set(OS "FreeRTOS")

# Include common BSP settings
include(${CMAKE_SOURCE_DIR}/port/bsp/bsp_common.cmake)
# Include Platform path
include(${CMAKE_SOURCE_DIR}/port/platform/${MCU_FAMILY}/${BOARD_MCU}/${BOARD_MCU}.cmake)
