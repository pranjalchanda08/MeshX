cmake_minimum_required(VERSION 3.12)

# Recursively collect sources
file(GLOB_RECURSE MESHX_SRC
    CONFIGURE_DEPENDS
    "*.c"
)

# Recursively collect include dirs
file(GLOB_RECURSE MESHX_INC_DIRS
    LIST_DIRECTORIES true
    CONFIGURE_DEPENDS
    "*"
)

set(MESHX_FINAL_INC "" CACHE STRING "MeshX include directories")
set(MESHX_SRC "" CACHE STRING "MeshX source files")

foreach(dir ${MESHX_INC_DIRS})
    if(IS_DIRECTORY ${dir} AND dir MATCHES "(/inc$|/include$)")
        list(APPEND MESHX_FINAL_INC "${dir}")
    endif()
endforeach()
list(REMOVE_DUPLICATES MESHX_FINAL_INC)

message(STATUS "PLATFORM_INC: ${PLATFORM_INC}")

set(MESHX_FINAL_INC ${MESHX_FINAL_INC} ${PLATFORM_INC})

message(STATUS "MESHX_FINAL_INC: ${MESHX_FINAL_INC}")
message(STATUS "MESHX_SRC: ${MESHX_SRC}")

# Create static library
add_library(meshx STATIC ${MESHX_SRC})
target_include_directories(meshx PUBLIC ${MESHX_FINAL_INC})


# ------------------------------------------------------------------------------
# Unit Tests
# ------------------------------------------------------------------------------
if(ENABLE_TESTS)
    add_library(unit_test STATIC unit_test/unit_test.c)
    target_include_directories(unit_test PUBLIC "unit_test")
endif()
