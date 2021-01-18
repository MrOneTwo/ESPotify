// periph.h - Michal Ciesielski 2021
//
// ESP32 module for setting up the necessary peripherals.

#ifndef PERIPH_H
#define PERIPH_H


void periph_init(void(*gpio_cb)(void* arg));

void periph_init_spi(void);

spi_device_handle_t periph_get_spi_handle(void);

#endif // PERIPH_H
