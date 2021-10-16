set(EXT_DEPENDENCIES ${EXT_DEPENDENCIES} rfid_reader)

idf_component_register(SRCS "test_app" "../../main/periph.c"
                       INCLUDE_DIRS "." "../../main"
                       REQUIRES unity ${EXT_DEPENDENCIES})

idf_build_set_property(COMPILE_DEFINITIONS -DCONFIG_RC522 APPEND)
