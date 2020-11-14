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

#include "rc522.h"


void rfid_init(spi_device_handle_t spi);

void rfid_start_scanning();

void rfid_save_song_to_picc(char* song_id);

status_e rfid_tag_present(void);

#endif // RFID_H
