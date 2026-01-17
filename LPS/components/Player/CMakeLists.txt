idf_component_register(
    SRCS  "src/framebuffer.cpp"  "src/player_clock.cpp" "src/player_v2.cpp" "src/player_state.cpp"

    INCLUDE_DIRS "include"

    REQUIRES LedController_v2 driver esp_driver_gptimer 
)