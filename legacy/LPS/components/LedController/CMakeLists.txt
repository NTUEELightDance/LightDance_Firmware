idf_component_register(
    SRCS   "src/ws2812b_encoder.c"  "src/ws2812b.c"  "src/pca9955b.c" "src/LedController.cpp"

    INCLUDE_DIRS "include"

    PRIV_INCLUDE_DIRS "src"

    REQUIRES esp_driver_i2c esp_driver_rmt esp_timer freertos log ld_core
)

