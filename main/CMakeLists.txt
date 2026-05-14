cmake_minimum_required(VERSION 3.16)

# Recursively collect sources, excluding elements_c
file(GLOB_RECURSE MESHX_SRC
    "${CMAKE_SOURCE_DIR}/main/component/meshx/**/*.c"
    "${CMAKE_SOURCE_DIR}/main/component/meshx/**/*.cpp"
    "${CMAKE_SOURCE_DIR}/main/src/*.c"
)

# Exclude legacy elements_c from sources
list(FILTER MESHX_SRC EXCLUDE REGEX "main/component/meshx/ble_mesh/elements_c/")

# Add nanopb core sources and generated protobuf C files
set(NANOPB_SRC 
    "${CMAKE_SOURCE_DIR}/third_party/nanopb/pb_common.c"
    "${CMAKE_SOURCE_DIR}/third_party/nanopb/pb_decode.c"
    "${CMAKE_SOURCE_DIR}/tools/scripts/proto/meshx_config.pb.c"
)
set(MESHX_SRC ${MESHX_SRC} ${NANOPB_SRC})


# Recursively collect include dirs
file(GLOB_RECURSE MESHX_INC_DIRS
    LIST_DIRECTORIES true
    "*"
)

set(MESHX_FINAL_INC "" CACHE STRING "MeshX include directories")

foreach(dir ${MESHX_INC_DIRS})
    if(IS_DIRECTORY ${dir} AND dir MATCHES "(/inc$|/include$)")
        # Exclude legacy elements_c from includes
        if(NOT dir MATCHES "main/component/meshx/ble_mesh/elements_c/")
            list(APPEND MESHX_FINAL_INC "${dir}")
        endif()
    endif()
endforeach()
list(REMOVE_DUPLICATES MESHX_FINAL_INC)

# Add nanopb include paths
list(APPEND MESHX_FINAL_INC "${CMAKE_SOURCE_DIR}/third_party/nanopb")
list(APPEND MESHX_FINAL_INC "${CMAKE_SOURCE_DIR}/tools/scripts/proto")

set(MESHX_FINAL_INC ${MESHX_FINAL_INC} ${PLATFORM_INC})

set(INC_FILES
    ${INC_FILES}
    ${MESHX_FINAL_INC}
    "${CMAKE_SOURCE_DIR}/main/component/unit_test"
)


# ------------------------------------------------------------------------------
# Unit Tests
# ------------------------------------------------------------------------------
if(ENABLE_TESTS AND PLATFORM_TEST_SUPPORTED)
    # Set the compile definition for complete project
    add_definitions(-DCONFIG_ENABLE_UNIT_TEST=1)
    include_directories("${CMAKE_SOURCE_DIR}/main/component/unit_test/inc")

    # Collect all unit test source files
    file(GLOB UNIT_TEST_SRC "${CMAKE_SOURCE_DIR}/main/component/unit_test/src/*.c")

    set(MESHX_SRC ${MESHX_SRC} ${UNIT_TEST_SRC})
endif()

set(SRC_FILES ${SRC_FILES} ${MESHX_SRC})

#-----------------------------------------------------------------------------

# Platform library registration
register_component("${SRC_FILES}" "${INC_FILES}" "${PLAT_LIBS}")
#-----------------------------------------------------------------------------

# Create a meshx.py args file like app-flash-args
set(ARGSTRING "-N ${PROD_NAME} -B ${BSP} --build-type ${MESHX_BUILD_TYPE} -H esp")

file(WRITE ${CMAKE_BINARY_DIR}/meshx.args "${ARGSTRING}")

#-----------------------------------------------------------------------------
