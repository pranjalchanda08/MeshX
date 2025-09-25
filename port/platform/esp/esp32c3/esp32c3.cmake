cmake_minimum_required(VERSION 3.12)

include("${CMAKE_SOURCE_DIR}/port/platform/esp/esp_idf/esp_idf.cmake")

set(PLATFORM_INC
    ${BASE_PLAT_INC}
    "${CMAKE_SOURCE_DIR}/port/platform/esp/esp32c3/config"
    PARENT_SCOPE
)

# add_library(plat_esp32c3 STATIC)

# target_sources(plat_esp32c3 PUBLIC
#     ${ALL_SRC_FILES}
# )

# target_include_directories(plat_esp32c3 PUBLIC
#     ${FINAL_INC_DIRS}
# )
# -------------------------------
# Register the ESP-IDF component
# -------------------------------
# idf_component_register(
#     SRCS         ${ALL_SRC_FILES}
#     INCLUDE_DIRS ${FINAL_INC_DIRS}
#     REQUIRES
#         bt
#         console
#         nvs_flash
#         esp_driver_uart
# )

# target_link_libraries(plat_esp32c3 PUBLIC meshx)
# target_link_libraries(plat_esp32c3 PUBLIC osal_FreeRTOS)

# Optional: if you have compile definitions or extra flags
# target_compile_definitions(${TARGET} PUBLIC SOME_DEFINE=1)
