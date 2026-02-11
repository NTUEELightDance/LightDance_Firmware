idf_component_register(
    SRCS   "src/ws2812b_encoder.c"  "src/ws2812b_hal_v2.c" "src/BoardConfig.c" "src/pca9955b_hal_v2.c" "src/LedController_v2.cpp"

    INCLUDE_DIRS "include"

    PRIV_INCLUDE_DIRS "src"

    REQUIRES driver esp_timer freertos log
)

