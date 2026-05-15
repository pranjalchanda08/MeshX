# if PROD_NAME is not set then set error
if(PROD_NAME)
else()
    message(FATAL_ERROR "PROD_NAME is not set. Please set it to the product name.")
endif()

# Include OSAL port
if (NOT DEFINED OS)
    message(FATAL_ERROR "OS is not set. Please set it to the target OS (e.g., freertos, none).")
else()
    include(${CMAKE_SOURCE_DIR}/port/os/osal.cmake)
endif()

# The product configuration is now generated at build time in main/CMakeLists.txt
# to ensure all dependencies (like protobuf) are satisfied.


set(PLATFORM_INC
    ${PLATFORM_INC}
    "${CMAKE_SOURCE_DIR}/port/bsp/${BSP}"
)

set(INC_FILES
    ${INC_FILES}
    ${PLATFORM_INC}
)
