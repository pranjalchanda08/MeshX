set(ESP_COMMON_DIR "${CMAKE_SOURCE_DIR}/port/platform/esp/esp_idf")

set(SDKCONFIG_DEFAULTS ${SDKCONFIG_DEFAULTS} "${ESP_COMMON_DIR}/sdkconfig.defaults.ble_mesh")

# Filter out sdkconfig.defaults.old
list(REMOVE_ITEM SDKCONFIG_DEFAULTS "${CMAKE_SOURCE_DIR}/sdkconfig.defaults.old")

message(STATUS "ESP_TARGET: ${ESP_TARGET}")
message(STATUS "IDF_PATH: $ENV{IDF_PATH}")
message(STATUS "SDKCONFIG_DEFAULTS: ${SDKCONFIG_DEFAULTS}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Library registration function for ESP-IDF
function(register_component SRC INC LIBS)
    idf_component_register(
        SRCS ${SRC}
        INCLUDE_DIRS ${INC}
        REQUIRES ${LIBS}
    )
endfunction()

# -------------------------------
# Recursively collect all source files (.c)
# -------------------------------
file(GLOB_RECURSE ESP_SRC
    CONFIGURE_DEPENDS
    "${ESP_COMMON_DIR}/**/*.c"
    )

# Recursively collect all directories within the base path.
file(GLOB_RECURSE ESP_INC_DIRS
    LIST_DIRECTORIES true
    "${ESP_COMMON_DIR}/**"
    )

# Use a regular expression to filter for directories ending with /inc or /include.
list(FILTER ESP_INC_DIRS INCLUDE REGEX "[/\\](inc|include)$")

message(STATUS "ESP_SRC: ${ESP_SRC}")
message(STATUS "ESP_INC: ${ESP_INC_DIRS}")

# Remove duplicates from the filtered list.
list(REMOVE_DUPLICATES ESP_INC_DIRS)

# Now set FINAL_INC_DIRS from the filtered list.
set(BASE_PLAT_INC "${ESP_INC_DIRS}")
set(BASE_PLAT_SRC "${ESP_SRC}")

set(PLAT_LIBS
    ${PLAT_LIBS}
    bt
    nvs_flash
    console
    esp_driver_uart
    freertos
)

add_definitions(-DCONFIG_APP_MAIN=app_main)

set(INC_FILES
    ${INC_FILES}
    ${BASE_PLAT_INC}
)

set(PLATFORM_INC
    ${BASE_PLAT_INC}
    "${CMAKE_SOURCE_DIR}/port/platform/esp/${ESP_TARGET}"
    CACHE STRING "platform include"
)

set(INC_FILES
    ${INC_FILES}
    ${PLATFORM_INC}
    ${CMAKE_SOURCE_DIR}/main/component/meshx/inc
)

set(SRC_FILES
    ${SRC_FILES}
    ${BASE_PLAT_SRC}
)
