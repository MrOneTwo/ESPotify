idf_component_register(SRCS
                       "espotify.c"
                       "periph.c"
                       "tasks.c"
                       "shared.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_http_server esp_http_client nvs_flash rfid_reader spotify)

