/*
 * 
 */

#ifndef HARDWARE_H
#define HARDWARE_H


void hardware_init(void(*gpio_cb)(void* arg));

spi_device_handle_t hardware_get_spi_handle(void);

#endif // HARDWARE_H
