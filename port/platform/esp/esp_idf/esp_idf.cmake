set(ESP_COMMON_DIR "${CMAKE_SOURCE_DIR}/port/platform/esp/esp_idf")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# -------------------------------
# Recursively collect all source files (.c)
# -------------------------------
file(GLOB_RECURSE ALL_SRC_FILES
CONFIGURE_DEPENDS
    "${ESP_COMMON_DIR}/**/*.c"
    )

# Recursively collect all directories within the base path.
file(GLOB_RECURSE ALL_DIRS
    LIST_DIRECTORIES true
    "${ESP_COMMON_DIR}/**"
    )

# Use a regular expression to filter for directories ending with /inc or /include.
list(FILTER ALL_DIRS INCLUDE REGEX "[/\\](inc|include)$")

message(STATUS "ALL DIRS found: ${ALL_DIRS}")

# Remove duplicates from the filtered list.
list(REMOVE_DUPLICATES ALL_DIRS)

# Now set FINAL_INC_DIRS from the filtered list.
set(BASE_PLAT_INC "${ALL_DIRS}")
