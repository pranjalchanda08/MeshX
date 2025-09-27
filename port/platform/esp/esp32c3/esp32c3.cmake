cmake_minimum_required(VERSION 3.12)

set(ESP_TARGET esp32c3 CACHE STRING "ESP target (e.g., esp32, esp32c3)")

message(STATUS CMAKE_SORCE_DIR: ${CMAKE_SOURCE_DIR})

set(PLATFORM_TEST_SUPPORTED ON)
set(IDF_TARGET ${ESP_TARGET})
set(SDKCONFIG "${CMAKE_SOURCE_DIR}/port/platform/esp/esp32c3/config/sdkconfig.defaults" CACHE STRING "sdkconfig path")

include("${CMAKE_SOURCE_DIR}/port/platform/esp/esp_idf/esp_idf.cmake")

set(PLATFORM_INC
    ${BASE_PLAT_INC}
    "${CMAKE_SOURCE_DIR}/port/platform/esp/esp32c3/config"
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
