idf_component_register(SRCS  "main.cpp" 
                    INCLUDE_DIRS "."
                    REQUIRES  esp_timer driver LedController Player fatfs console SD Messenger
                    )