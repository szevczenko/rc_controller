idf_component_register(SRCS "main.c"
                        INCLUDE_DIRS "."
                        REQUIRES rc_core rc_protocol radio radio_espnow
                                 input_adc input_switch nvs_flash esp_timer)
