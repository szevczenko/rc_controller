idf_component_register(SRCS "main.c"
                        INCLUDE_DIRS "."
                        REQUIRES rc_core rc_protocol radio radio_espnow
                                 output_pwm nvs_flash esp_timer)
