idf_component_register(
    SRCS  "src/bt_receiver.cpp"

    INCLUDE_DIRS "include"

    REQUIRES Player bt nvs_flash
)