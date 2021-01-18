#include "pn532.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "soc/gpio_struct.h"


static spi_device_handle_t pn532_spi;
static esp_timer_handle_t pn532_timer;


static bool
pn532_is_ready()
{
  uint8_t cmd = PN532_SPI_STATREAD;
  uint8_t reply;
  
  spi_transaction_t t = {};

  // Yes, the length is in bits.
  t.length = 8 * (1);
  t.tx_buffer = (uint8_t*)&cmd;
  t.rx_buffer = (uint8_t*)&reply;

  esp_err_t ret = spi_device_transmit(pn532_spi, &t);

  return reply == PN532_SPI_READY;
}

static esp_err_t
pn532_write_command(uint8_t* cmd, uint8_t cmdlen)
{
  int crc;
  uint8_t packet[8 + cmdlen];
  uint8_t* p = packet;

  *p++ = PN532_SPI_DATA_WRITE;
  *p++ = PN532_PREAMBLE;
  *p++ = PN532_START_CODE1;
  *p++ = PN532_START_CODE2;
  *p++ = cmdlen;
  *p++ = ~cmdlen + 1;
  *p++ = PN532_HOST_TO_PN532;

  crc = PN532_PREAMBLE + PN532_START_CODE1 + PN532_START_CODE2 + PN532_HOST_TO_PN532;

  for (uint8_t i = 0; i < cmdlen - 1; i++)
  {
    *p++ = cmd[i];
    crc += cmd[i];
  }

  *p++ = ~crc;
  *p++ = PN532_POSTAMBLE;
  
  spi_transaction_t t = {};

  // Yes, the length is in bits.
  t.length = 8 * (8 + cmdlen);
  t.tx_buffer = (uint8_t*)packet;

  esp_err_t ret = spi_device_transmit(pn532_spi, &t);
  
  return ret;
}

esp_err_t
pn532_init(spi_device_handle_t spi)
{
  pn532_spi = spi;
  return ESP_OK;
}


