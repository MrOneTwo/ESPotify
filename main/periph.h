// periph.h - Michal Ciesielski 2021
//
// ESP32 module for setting up the necessary peripherals.

#ifndef PERIPH_H
#define PERIPH_H

#include "driver/spi_master.h"

void periph_init();

void periph_init_spi(void);

spi_device_handle_t periph_get_spi_handle(void);

uint8_t periph_get_button_state(uint8_t clear);

#endif // PERIPH_H
