idf_component_register(
    SRCS
        "src/bt_receiver.cpp"
        "src/bt_protocol.cpp"
        "src/bt_hci.cpp"
        "src/bt_scheduler.cpp"
        "src/bt_ack.cpp"

    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "private"

    REQUIRES Player bt nvs_flash
)
