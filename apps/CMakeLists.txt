cmake_minimum_required(VERSION 3.12)

# Recursively collect sources
file(GLOB_RECURSE APPS_SRC
    CONFIGURE_DEPENDS
    "*.c"
)

set(MESHX_INC_DIRS "${CMAKE_SOURCE_DIR}/apps/component/meshx")

# Recursively collect include dirs
file(GLOB_RECURSE MESHX_INC_DIRS
    LIST_DIRECTORIES true
    CONFIGURE_DEPENDS
    ${MESHX_INC_DIRS}
)

set(APPS_INC "")
foreach(dir ${MESHX_INC_DIRS})
    if(IS_DIRECTORY ${dir} AND dir MATCHES "(/inc$|/include$)")
        list(APPEND APPS_INC "${dir}")
    endif()
endforeach()
list(REMOVE_DUPLICATES APPS_INC)

set(APPS_INC "${APPS_INC}" "${PLATFORM_INC}")

message(STATUS "APPS_INC: ${APPS_INC}")
message(STATUS "APPS_SRC: ${APPS_SRC}")
message(STATUS "PROJECT_NAME: ${PROJECT_NAME}")

add_subdirectory(component)

add_executable(${PROJECT_NAME} ${APPS_SRC})

target_include_directories(${PROJECT_NAME} PUBLIC ${APPS_INC})

target_link_libraries(${PROJECT_NAME} PUBLIC meshx)
target_link_libraries(${PROJECT_NAME} PUBLIC osal_${OS})
target_link_libraries(${PROJECT_NAME} PUBLIC plat_${PLATFORM})
