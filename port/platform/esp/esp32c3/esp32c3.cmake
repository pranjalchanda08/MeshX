cmake_minimum_required(VERSION 3.12)

set(ESP_TARGET esp32c3 CACHE STRING "ESP target (e.g., esp32, esp32c3)")

message(STATUS CMAKE_SORCE_DIR: ${CMAKE_SOURCE_DIR})

set(PLATFORM_TEST_SUPPORTED ON)
set(IDF_TARGET ${ESP_TARGET})

file(GLOB SDKCONFIG_DEFAULTS_LIST "${CMAKE_SOURCE_DIR}/port/platform/esp/${ESP_TARGET}/sdkconfig.defaults*")
set(SDKCONFIG_DEFAULTS "${SDKCONFIG_DEFAULTS_LIST}" CACHE STRING "sdkconfig path")

include("${CMAKE_SOURCE_DIR}/port/platform/esp/esp_idf/esp_idf.cmake")
