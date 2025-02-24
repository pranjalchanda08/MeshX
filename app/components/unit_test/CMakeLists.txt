set(DEP_SRC
    "unit_test.c"
)
set(DEP_INC
    "${COMPONENT_DIR}"
)
# This is the MeshX CMakeLists.txt template. Any changes made to this file shall directy reflect to the generated CMakeLists.txt

# Filter only directories
set(INC_DIRS
    ${DEP_INC}
    )

message("       >> Include Dir: ${INC_DIRS} ${TARGET}")

set(SRCSX
    ${DEP_SRC}
    )

idf_component_register( SRCS ${SRCSX}
                        INCLUDE_DIRS "${INC_DIRS}"
                        REQUIRES
                                console
                                esp_driver_uart
                                meshx
                        )
