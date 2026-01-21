idf_component_register(SRCS  "main.cpp" "bt_receiver.cpp""console_new.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES  esp_timer driver LedController Player fatfs console bt nvs_flash SD
                    )