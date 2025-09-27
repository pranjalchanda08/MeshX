# if PROD_NAME is not set then set error
if(PROD_NAME)
    message(STATUS "Product: ${PROD_NAME}")
else()
    message(FATAL_ERROR "PROD_NAME is not set. Please set it to the product name.")
endif()

# Include OSAL port
if (NOT DEFINED OS)
    message(FATAL_ERROR "OS is not set. Please set it to the target OS (e.g., freertos, none).")
else()
    message(STATUS "Using OS: ${OS}")
    include(${CMAKE_SOURCE_DIR}/port/os/osal.cmake)
endif()

# run python script
execute_process(COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/scripts/code_gen.py --config port/bsp/${BSP} --profile ${PROD_PROFILE} ${PROD_NAME}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE result
)

set(PLATFORM_INC
    ${PLATFORM_INC}
    "${CMAKE_SOURCE_DIR}/port/bsp/${BSP}"
)

set(INC_FILES
    ${INC_FILES}
    ${PLATFORM_INC}
)
