# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(PYTHON_EXECUTABLE /usr/bin/python3)
set(PYTHON_SCRIPT scripts/code_gen.py)

# Define the minimum required ESP-IDF version
set(REQUIRED_IDF_VERSION "5.4.0")

# Compare the current ESP-IDF version with the required version
if("$ENV{IDF_VERSION}" VERSION_LESS "${REQUIRED_IDF_VERSION}")
    message(FATAL_ERROR "ESP-IDF version ${REQUIRED_IDF_VERSION} or higher is required. You are using version ${IDF_VERSION}.")
endif()

# set(TARGET switch_relay)
project(${TARGET})

