idf_component_register(SRCS "spotify.c"
                       INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
                       REQUIRES esp_http_client json)
