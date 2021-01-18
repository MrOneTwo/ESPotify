/*
 * 
 */

#ifndef PERIPH_H
#define PERIPH_H


void periph_init(void(*gpio_cb)(void* arg));

spi_device_handle_t periph_get_spi_handle(void);

#endif // PERIPH_H
