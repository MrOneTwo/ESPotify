/*
 * This is a driver for the PN532 RFID module. It's important to keep this simple.
 *
 * - no complicated peripheral initing/management inside this module.
 */

#ifndef PN532_H
#define PN532_H

#include "driver/spi_master.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "rfid_reader.h"


esp_err_t pn532_init(spi_device_handle_t spi);

status_e pn532_test_picc_presence();

#endif // PN532_H
