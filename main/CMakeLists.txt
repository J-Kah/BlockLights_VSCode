idf_component_register(SRCS "blockLightsSettings.cpp" "main.cpp"
                            "realTimePacing.cpp"
                           
                    INCLUDE_DIRS ".")
spiffs_create_partition_image(storage ../partition FLASH_IN_PROJECT)