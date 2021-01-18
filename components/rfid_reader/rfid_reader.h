/*
 * The idea behind this module is to abstract the RFID reader. That's because this project supports
 * readers using following chips:
 *
 * - MFRC522
 * - PN532
 * 
 */

#ifndef RFID_READER_H
#define RFID_READER_H

#include "driver/spi_master.h"

typedef enum {
  FAILURE,
  SUCCESS,
} status_e;


void
rfid_implement(void);

esp_err_t
rfid_init(spi_device_handle_t spi);

// TODO(michalc): not implemented yet
bool
rfid_test_picc_presence(void);

// TODO(michalc): not implemented yet
void
rfid_get_picc_id(void);

// TODO(michalc): not implemented yet
bool
rfid_anti_collision(uint8_t cascade_level);

#endif // RFID_READER_H
