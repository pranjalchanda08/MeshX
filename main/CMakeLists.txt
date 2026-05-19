cmake_minimum_required(VERSION 3.16)

# Recursively collect sources, excluding elements_c
file(GLOB_RECURSE MESHX_SRC
    "${CMAKE_SOURCE_DIR}/main/component/meshx/**/*.c"
    "${CMAKE_SOURCE_DIR}/main/component/meshx/**/*.cpp"
    "${CMAKE_SOURCE_DIR}/main/src/*.c"
)

# Exclude legacy elements_c from sources
list(FILTER MESHX_SRC EXCLUDE REGEX "main/component/meshx/ble_mesh/elements_c/")

# ------------------------------------------------------------------------------
# Protobuf / Nanopb Generation
# ------------------------------------------------------------------------------
# We wrap this in a check to avoid "not scriptable" errors during ESP-IDF's
# early expansion/requirement gathering phase.
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    set(PROTO_PATH "${CMAKE_SOURCE_DIR}/tools/scripts/proto")
    set(PROTO_FILE "${PROTO_PATH}/meshx_config.proto")
    set(NANOPB_DIR "${CMAKE_SOURCE_DIR}/third_party/nanopb")

    # Locate tools provided in third_party
    set(PROTOC_EXE "${NANOPB_DIR}/generator/protoc")
    set(NANOPB_GENERATOR "${NANOPB_DIR}/generator/nanopb_generator.py")

    # Ensure nanopb submodule is initialized
    if(NOT EXISTS "${NANOPB_DIR}/pb_common.c")
        message(WARNING "Nanopb submodule not found at ${NANOPB_DIR}. Attempting to initialize...")
        find_package(Git QUIET)
        if(GIT_FOUND)
            execute_process(
                COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive -- third_party/nanopb
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                RESULT_VARIABLE git_result
            )
            if(NOT git_result EQUAL 0)
                message(FATAL_ERROR "Failed to initialize Nanopb submodule. Please run 'git submodule update --init --recursive' manually.")
            endif()
        else()
            message(FATAL_ERROR "Git not found. Cannot initialize Nanopb submodule. Please run 'git submodule update --init --recursive' manually.")
        endif()
    endif()


    # Ensure python dependencies are installed in a local directory to avoid permission issues
    set(MESHX_PYTHON_DEPS "${CMAKE_BINARY_DIR}/python_deps")
    message(STATUS "MeshX: Using Python interpreter: ${PYTHON}")
    message(STATUS "MeshX: Installing Python dependencies to ${MESHX_PYTHON_DEPS}...")

    file(MAKE_DIRECTORY "${MESHX_PYTHON_DEPS}")

    execute_process(
        COMMAND ${PYTHON} -m pip install protobuf grpcio-tools pyyaml --target "${MESHX_PYTHON_DEPS}"
        RESULT_VARIABLE pip_status
    )
    if(NOT pip_status EQUAL 0)
        message(FATAL_ERROR "MeshX: Failed to install Python dependencies to local directory.")
    endif()

    # Helper to run python with the local deps in PYTHONPATH
    set(PYTHON_WITH_DEPS ${CMAKE_COMMAND} -E env "PYTHONPATH=${MESHX_PYTHON_DEPS}" ${PYTHON})

    # Output files
    set(GEN_C "${CMAKE_CURRENT_BINARY_DIR}/meshx_config.pb.c")
    set(GEN_H "${CMAKE_CURRENT_BINARY_DIR}/meshx_config.pb.h")
    set(GEN_PY "${PROTO_PATH}/meshx_config_pb2.py")
    set(OPTIONS_FILE "${PROTO_PATH}/meshx_config.options")

    # Generate Schema ID based on CRC32 of .proto and .options
    # We use a python one-liner that strips whitespace to match code_gen.py
    execute_process(
        COMMAND ${PYTHON_WITH_DEPS} -c "import binascii, os; content=b''; [ (content := content + b''.join(open(p,'rb').read().split())) for p in ['${PROTO_FILE}','${OPTIONS_FILE}'] if os.path.exists(p)]; print(binascii.crc32(content) & 0xFFFFFFFF)"
        OUTPUT_VARIABLE MESHX_SCHEMA_ID
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    add_definitions(-DMESHX_SCHEMA_ID=${MESHX_SCHEMA_ID})
    message(STATUS "MeshX Schema ID: ${MESHX_SCHEMA_ID}")

    # Generate C and H files using Nanopb generator
    add_custom_command(
        OUTPUT "${GEN_C}" "${GEN_H}"
        COMMAND ${PYTHON_WITH_DEPS} ${NANOPB_GENERATOR}
                -I ${PROTO_PATH}
                --output-dir=${CMAKE_CURRENT_BINARY_DIR}
                ${PROTO_FILE}
        DEPENDS ${PROTO_FILE} ${PROTO_PATH}/meshx_config.options ${NANOPB_GENERATOR}
        COMMENT "Generating Nanopb C files using nanopb_generator.py"
        VERBATIM
    )

    # Generate Python pb2 file (needed for code_gen.py)
    # We generate this in the source tree because code_gen.py expects it there
    add_custom_command(
        OUTPUT "${GEN_PY}"
        COMMAND ${PYTHON_WITH_DEPS} -m grpc_tools.protoc
                -I ${PROTO_PATH}
                --python_out=${PROTO_PATH}
                ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
        COMMENT "Generating Python Protobuf for code_gen.py"
        VERBATIM
    )

    # ------------------------------------------------------------------------------
    # Product Config Generation (meshx_config.h and meshx_cfg.bin)
    # ------------------------------------------------------------------------------
    set(PROD_NAME_STR "${PROD_NAME}")
    set(PROD_PROFILE_PATH "${CMAKE_SOURCE_DIR}/port/bsp/${BSP}/prod_profile.yml")
    set(MODEL_PROFILE_PATH "${CMAKE_SOURCE_DIR}/tools/scripts/model_profile.yml")

    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/meshx_config.h" "${CMAKE_CURRENT_BINARY_DIR}/meshx_cfg.mxc"
        COMMAND ${PYTHON_WITH_DEPS} ${CMAKE_SOURCE_DIR}/tools/scripts/code_gen.py
                ${PROD_NAME_STR}
                --root ${CMAKE_SOURCE_DIR}
                --config ${CMAKE_CURRENT_BINARY_DIR}
                --profile ${PROD_PROFILE_PATH}
        DEPENDS "${PROD_PROFILE_PATH}" "${MODEL_PROFILE_PATH}" "${GEN_PY}"
        COMMENT "Generating product configuration for ${PROD_NAME_STR}"
        VERBATIM
    )


    # Add nanopb core sources and generated protobuf C files
    set(NANOPB_SRC
        "${NANOPB_DIR}/pb_common.c"
        "${NANOPB_DIR}/pb_decode.c"
        "${GEN_C}"
    )
    set(MESHX_SRC ${MESHX_SRC} ${NANOPB_SRC} "${CMAKE_CURRENT_BINARY_DIR}/meshx_config.h")
endif()


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
list(APPEND MESHX_FINAL_INC "${CMAKE_CURRENT_BINARY_DIR}") # For generated headers

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
