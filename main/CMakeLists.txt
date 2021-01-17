idf_component_register(SRCS
                       "espotify.c"
                       "hardware.c"
                       "tasks.c"
                       "shared.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_http_server nvs_flash spotify rfid_reader)
