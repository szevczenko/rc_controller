idf_component_register(SRCS "src/radio_espnow.c"
                        INCLUDE_DIRS "include"
                        REQUIRES radio config esp_wifi freertos)
