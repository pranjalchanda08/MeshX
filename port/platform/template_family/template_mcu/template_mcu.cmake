# Template MCU/platform CMake
# Copy this file to port/platform/<family>/<mcu>/<mcu>.cmake and update variables

set(ESP_TARGET <your_target> CACHE STRING "Platform target identifier")

# Example: declare that platform supports unit testing
set(PLATFORM_TEST_SUPPORTED ON)

# Set SDK-specific include/source lists here. Replace with appropriate values.
set(BASE_PLAT_INC
    ${BASE_PLAT_INC}
)

set(BASE_PLAT_SRC
    ${BASE_PLAT_SRC}
)

# Provide a register_component function or include your SDK's CMake helpers here.
function(register_component SRC INC LIBS)
    # Replace the line below with your SDK's equivalent of component registration
    message(STATUS "Registering component (template): ${SRC} ${INC} ${LIBS}")
endfunction()

set(PLAT_LIBS
    ${PLAT_LIBS}
)

set(INC_FILES
    ${INC_FILES}
    ${BASE_PLAT_INC}
)

set(SRC_FILES
    ${SRC_FILES}
    ${BASE_PLAT_SRC}
)
