message("   >> Adding main")
file(GLOB MAIN_SRCS "src/*c")

# Filter only directories
set(INC_DIRS
    "${CMAKE_SOURCE_DIR}/main"
    "${CMAKE_SOURCE_DIR}/main/src"
    ${DEP_INC}
    )

message("       >> Include Dir: ${INC_DIRS} ${TARGET}")

set(SRCSX
    ${MAIN_SRCS}
    )

idf_component_register( SRCS ${SRCSX}
                        INCLUDE_DIRS "${INC_DIRS}"
                        REQUIRES meshx
                    )
