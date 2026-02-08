idf_component_register(
    SRCS   "src/ld_i2c_bus.c" "src/ld_i2c_dev.c"

    INCLUDE_DIRS "inc"

    REQUIRES driver freertos log 
)

