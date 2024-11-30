# Message to indicate the component is being added
message("   >> Adding Prod common")

# Glob all .c files in provisioning
file(GLOB PROVISIONING_SRV "provisioning/*.c")
file(GLOB CONFIG_SERVER_SRCS "config_server/*.c")

# Define source files
set(SRCS
    ${PROVISIONING_SRV} # Use the variable without quotes or a $
    ${CONFIG_SERVER_SRCS} # Use the variable without quotes or a $
)

# Glob all directories recursively
file(GLOB_RECURSE ALL_DIRS
    RELATIVE "${CMAKE_SOURCE_DIR}/prod_common" # Use CMAKE_SOURCE_DIR for consistent paths
    LIST_DIRECTORIES true
    "./*"
)
# Filter only directories
set(INCLUDE_DIRS "")
foreach(DIR ${ALL_DIRS})
    if(IS_DIRECTORY "${CMAKE_SOURCE_DIR}/prod_common/${DIR}")
        list(APPEND INCLUDE_DIRS "${DIR}")
    endif()
endforeach()

# Debugging: Print the included directories
message("       >> Include directories: ${INCLUDE_DIRS}")

# Register the component
idf_component_register(SRCS ${SRCS}
                       INCLUDE_DIRS ${INCLUDE_DIRS}
                       REQUIRES bt
                                example_init
                                nvs_flash)
