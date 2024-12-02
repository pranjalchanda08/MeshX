message("   >> Adding main")
file(GLOB MAIN_SRCS "src/*c")

# Filter only directories
set(INC_DIRS 
    "${CMAKE_SOURCE_DIR}/main"
    "${CMAKE_SOURCE_DIR}/main/src"
    )

message("       >> Include Dir: ${INC_DIRS}")

idf_component_register( SRCS ${MAIN_SRCS}
                        INCLUDE_DIRS "${INC_DIRS}"
                        REQUIRES bt
                                example_init
                                nvs_flash)
