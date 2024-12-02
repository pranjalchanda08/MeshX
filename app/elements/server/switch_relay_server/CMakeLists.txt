# Message to indicate the component is being added
message("   >> Adding switch relay server")

# Glob all .c files in config_server
file(GLOB SW_RELAY_SRCS "*c")
# Define source files
set(SRCS
    ${SW_RELAY_SRCS} # Use the variable without quotes or a $
    )

set(INC_DIRS 
    "${CMAKE_CURRENT_LIST_DIR}"
    )

message("       >> Include Dir: ${INC_DIRS}")

# Register the component
idf_component_register(SRCS ${SRCS}
                       INCLUDE_DIRS ${INC_DIRS}
                       REQUIRES bt
                                server_common
                                prod_common)
