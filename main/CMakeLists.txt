# It's not enough to comment out an append for a specific dependency.
# The idf_component_register in that dependency already registers that component
# for compilation and pulls its Kconfig symbols.
set(EXT_DEPENDENCIES ${EXT_DEPENDENCIES} spotify)
set(EXT_DEPENDENCIES ${EXT_DEPENDENCIES} rfid_reader)

idf_component_register(SRCS
                       "espotify.c"
                       "periph.c"
                       "tasks.c"
                       "shared.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_http_server esp_http_client nvs_flash ${EXT_DEPENDENCIES})


# This doesn't work. Somehow appending to REQUIRES registers the component
# but the build process doesn't add the include paths...
# idf_component_set_property(main REQUIRES rfid_reader APPEND)
# idf_component_set_property(main REQUIRES spotify APPEND)

# idf_component_get_property(_REQUIRES main REQUIRES)
# message(" ++++++++++++++++++++  ${_REQUIRES}")
