# Template BSP CMake for a new board
# Copy this folder to `port/bsp/<your_bsp>` and update the variables below.

set(BOARD_NAME "<Your Board>")
set(BOARD_MCU "<your_mcu>")
set(MCU_FAMILY "<family>")

# Select the OS used by this BSP (example: FreeRTOS)
set(OS "FreeRTOS")

# Include common BSP settings (this validates PROD_NAME and runs code generation)
include(${CMAKE_SOURCE_DIR}/port/bsp/bsp_common.cmake)

# Include platform-specific glue (create the file under port/platform/<family>/<mcu>/<mcu>.cmake)
include(${CMAKE_SOURCE_DIR}/port/platform/${MCU_FAMILY}/${BOARD_MCU}/${BOARD_MCU}.cmake)

set(PLATFORM_INC
    ${PLATFORM_INC}
    "${CMAKE_SOURCE_DIR}/port/bsp/${BSP}"
)
