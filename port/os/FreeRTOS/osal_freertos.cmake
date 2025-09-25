# Recursively collect all .c files
file(GLOB_RECURSE ALL_SRC_FILES
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/port/os/FreeRTOS/*.c"
)

message(STATUS "FreeRTOS: ${ALL_SRC_FILES}")

add_library(osal_FreeRTOS STATIC ${ALL_SRC_FILES})

set(INC_DIR "${CMAKE_SOURCE_DIR}/apps/component/meshx"
            "${CMAKE_SOURCE_DIR}/apps/component/meshx/inc"
            "${RTOS_ROOT}")

target_include_directories(osal_FreeRTOS PUBLIC ${INC_DIR})

message(STATUS "FreeRTOS_INC: ${INC_DIR}")
