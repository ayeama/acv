idf_component_register(SRCS "acv.c" "main.c" "nvs.c" "server.c" "twai.c" "wifi.c"
                    PRIV_REQUIRES esp_driver_twai esp_http_server esp_wifi nvs_flash
                    INCLUDE_DIRS ".")

littlefs_create_partition_image(www "${CMAKE_CURRENT_SOURCE_DIR}/../front" FLASH_IN_PROJECT)
