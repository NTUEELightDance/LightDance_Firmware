idf_component_register(SRCS  "main.cpp" 
                    INCLUDE_DIRS "."
                    REQUIRES  esp_timer driver LedController_v2 Player fatfs console 
                    )