idf_component_register(SRCS "test_rfid_reader.c" "test_pn532.c" "test_rc522.c"
                       SRC_DIRS "."
                       INCLUDE_DIRS "." ".."
                       REQUIRES "unity")
