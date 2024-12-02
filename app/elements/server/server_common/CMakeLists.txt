# Message to indicate the component is being added
message("   >> Adding server common")

# Glob all .c files in config_server
file(GLOB GEN_ON_OFF_SRCS "gen_onoff_server/*c")
# Define source files
set(SRCS
    ${GEN_ON_OFF_SRCS} # Use the variable without quotes or a $
)

# Glob all directories recursively
file(GLOB_RECURSE ALL_DIRS
    RELATIVE "${CMAKE_SOURCE_DIR}/elements/server/server_common" # Use CMAKE_SOURCE_DIR for consistent paths
    LIST_DIRECTORIES true
    "./*"
)
# Filter only directories
set(INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/elements/server/server_common/${DIR}")
foreach(DIR ${ALL_DIRS})
    if(IS_DIRECTORY "${CMAKE_SOURCE_DIR}/elements/server/server_common/${DIR}")
        list(APPEND INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/elements/server/server_common/${DIR}")
    endif()
endforeach()

# Debugging: Print the included directories
message("       >> Include directories: ${INCLUDE_DIRS}")

# Register the component
idf_component_register(SRCS ${SRCS}
                       INCLUDE_DIRS ${INCLUDE_DIRS}
                       REQUIRES bt)
