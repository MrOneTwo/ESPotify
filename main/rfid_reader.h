/*
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
