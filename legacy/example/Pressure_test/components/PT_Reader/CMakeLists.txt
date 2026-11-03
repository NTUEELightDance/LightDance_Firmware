idf_component_register(SRCS "control_reader.c" "frame_reader.c" "readframe.c" 
                    INCLUDE_DIRS "."
                    REQUIRES Player fatfs ld_core)
