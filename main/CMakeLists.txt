idf_component_register(SRCS "blockLightsSetup.cpp" "autoPacing.cpp" "blockLightsSettings.cpp" "main.cpp"
                            "realTimePacing.cpp"        
                    INCLUDE_DIRS ".")
littlefs_create_partition_image(storage ../partition FLASH_IN_PROJECT)