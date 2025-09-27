# Recursively collect all .c files
file(GLOB_RECURSE OSAL_SRC
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/port/os/${OS}/*.c"
)

message(STATUS "${OS}: ${OSAL_SRC}")

set(OSAL_INC_DIR "${CMAKE_SOURCE_DIR}/main/component/meshx"
            "${CMAKE_SOURCE_DIR}/main/component/meshx/inc"
            "${RTOS_ROOT}")

set(SRC_FILES
    ${SRC_FILES}
    ${OSAL_SRC}
)
set(INC_FILES
    ${INC_FILES}
    ${OSAL_INC_DIR}
)
