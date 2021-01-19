idf_component_register(SRCS "test_rfid_reader.c" "test_pn532.c" "test_rc522.c"
                       SRC_DIRS "."
                       INCLUDE_DIRS "."
                                    ".."
                                    "../../../main/" # I don't like that this references the main dir.
                                                     # That's for periph.c. periph module probably should become a component.
                       REQUIRES "unity")
