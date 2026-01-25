idf_component_register(
    SRCS  "src/framebuffer.cpp"  "src/player_clock.cpp" "src/player.cpp" "src/player_fsm.cpp" "src/player_console.cpp" 

    INCLUDE_DIRS "include"

    REQUIRES LedController SD driver esp_driver_gptimer console common
)