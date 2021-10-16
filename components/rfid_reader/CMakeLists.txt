# Create a component 'rfid_reader' and a target 'librfid_reader.a' target.
idf_component_register(SRCS "rfid_reader.c" "rc522.c" "pn532.c"
                       INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
                       REQUIRES )
