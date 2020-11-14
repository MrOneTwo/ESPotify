#ifndef RFID_H
#define RFID_H

/*
 * This module is wrapping the RFID functionality. It narrows the functionality to the high
 * level functions like saving the song in a PICC.
 * This should be the only module which uses lower level rc522 module.
 */

#include "driver/spi_master.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"


void rfid_init(spi_device_handle_t spi, QueueHandle_t q);

void rfid_start_scanning();

#endif // RFID_H
