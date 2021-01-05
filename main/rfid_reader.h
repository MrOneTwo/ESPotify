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


void
rfid_implement(void);

void
rfid_init(spi_device_handle_t spi);

#endif // RFID_READER_H
