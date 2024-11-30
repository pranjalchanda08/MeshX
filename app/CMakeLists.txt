# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(TARGET switch_relay)
project(${TARGET})


# set (SRCS "")
# set (INC_DIRS "")

# add_subdirectory(prod_common)
# add_subdirectory(elements)

# message(STATUS "Final SRCS: ${SRCS}")
# message(STATUS "Final INC_DIRS: ${INC_DIRS}")
