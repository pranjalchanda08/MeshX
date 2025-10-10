cmake_minimum_required(VERSION 3.16)

# Recursively collect sources
file(GLOB_RECURSE MESHX_SRC
    "${CMAKE_SOURCE_DIR}/main/component/meshx/**/*.c"
    "${CMAKE_SOURCE_DIR}/main/src/*.c"
)
message(STATUS "MESHX_SRC: ${MESHX_SRC}")

# Recursively collect include dirs
file(GLOB_RECURSE MESHX_INC_DIRS
    LIST_DIRECTORIES true
    "*"
)

set(MESHX_FINAL_INC "" CACHE STRING "MeshX include directories")

foreach(dir ${MESHX_INC_DIRS})
    if(IS_DIRECTORY ${dir} AND dir MATCHES "(/inc$|/include$)")
        list(APPEND MESHX_FINAL_INC "${dir}")
    endif()
endforeach()
list(REMOVE_DUPLICATES MESHX_FINAL_INC)

set(MESHX_FINAL_INC ${MESHX_FINAL_INC} ${PLATFORM_INC})

set(INC_FILES
    ${INC_FILES}
    ${MESHX_FINAL_INC}
    "${CMAKE_SOURCE_DIR}/main/component/unit_test"
)

message(STATUS "INC_FILES: ${INC_FILES}")

# ------------------------------------------------------------------------------
# Unit Tests
# ------------------------------------------------------------------------------
if(ENABLE_TESTS AND PLATFORM_TEST_SUPPORTED)
    message(STATUS ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>")
    message(STATUS ">>>>>>>>> Unit tests enabled <<<<<<<<<")
    message(STATUS ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>")
    # Set the compile definition for complete project
    add_definitions(-DCONFIG_ENABLE_UNIT_TEST=1)
    include_directories("${CMAKE_SOURCE_DIR}/main/component/unit_test")
    set(MESHX_SRC ${MESHX_SRC} "${CMAKE_SOURCE_DIR}/main/component/unit_test/unit_test.c")
endif()

set(SRC_FILES ${SRC_FILES} ${MESHX_SRC})
message(STATUS "SRC_FILES: ${SRC_FILES}")

#-----------------------------------------------------------------------------

# Platform library registration
register_component("${SRC_FILES}" "${INC_FILES}" "${PLAT_LIBS}")
#-----------------------------------------------------------------------------

# Create a meshx.py args file like app-flash-args
set(ARGSTRING "-N ${PROD_NAME} -B ${BSP} --build-type ${MESHX_BUILD_TYPE} -H esp")

message(STATUS "ARGSTRING: ${ARGSTRING}")
message(STATUS "CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}/meshx.args")
file(WRITE ${CMAKE_BINARY_DIR}/meshx.args "${ARGSTRING}")

#-----------------------------------------------------------------------------
