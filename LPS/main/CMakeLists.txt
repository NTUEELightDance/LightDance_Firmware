idf_component_register(SRCS  "main.cpp" 
                    INCLUDE_DIRS "."
                    REQUIRES LedController Player fatfs console SD Messenger ld_rmt ld_i2c 
                    )