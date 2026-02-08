idf_component_register(
    SRCS   "src/encoder/ld_ws2812b_encoder.c" "src/ld_ws2812b_tx.c"

    INCLUDE_DIRS "inc"

    REQUIRES driver esp_timer freertos log common
)

