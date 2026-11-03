idf_component_register(SRCS "ld_partition.c" "ld_nvs.c"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash esp_partition)
