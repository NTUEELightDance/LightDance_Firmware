idf_component_register(SRCS "sd_utils.c" "sd_logger.c"
                    INCLUDE_DIRS "."
                    REQUIRES Player spiffs ld_core)
