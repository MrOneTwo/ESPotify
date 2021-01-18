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
pn532_read_ack()
{
  uint8_t _pn532_ack[] = {0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00};
  uint8_t cmd = PN532_SPI_DATA_READ;
  uint8_t response[6] = {};

  {
    spi_transaction_t t = {};

    // Yes, the length is in bits.
    t.length = 8 * (1);
    t.rxlength = 8 * (6);
    t.tx_buffer = (uint8_t*)&cmd;
    t.rx_buffer = (uint8_t*)&response;

    esp_err_t ret = spi_device_transmit(pn532_spi, &t);
  }

  return (0 == memcmp((void*)response, (void*)_pn532_ack, 6));
}

static bool
pn532_is_ready()
{
  uint8_t cmd = PN532_SPI_STAT_READ;
  uint8_t response;

  {
    spi_transaction_t t = {};

    // Yes, the length is in bits.
    t.length = 8 * (1);
    t.rxlength = 8 * (1);
    t.tx_buffer = (uint8_t*)&cmd;
    t.rx_buffer = (uint8_t*)&response;

    esp_err_t ret = spi_device_transmit(pn532_spi, &t);
  }

  return response == PN532_SPI_READY;
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

  esp_err_t ret = ESP_FAIL;
  {
    spi_transaction_t t = {};

    // Yes, the length is in bits.
    t.length = 8 * (8 + cmdlen);
    t.tx_buffer = (uint8_t*)packet;

    ret = spi_device_transmit(pn532_spi, &t);
  }
  
  return ret;
}

esp_err_t
pn532_read_n(uint8_t* buff, uint8_t n)
{
  uint8_t cmd = PN532_SPI_DATA_READ;

  spi_transaction_t t = {};

  // Yes, the length is in bits.
  t.length = 8 * (1);
  t.rxlength = 8 * (n);
  t.tx_buffer = (uint8_t*)&cmd;
  t.rx_buffer = (uint8_t*)buff;

  esp_err_t ret = spi_device_transmit(pn532_spi, &t);
  return ret;
}

bool
pn532_read_fw_version()
{
  uint8_t _pn532_fw_version[] = {0x00, 0x00, 0xFF, 0x06, 0xFA, 0xD5};
  uint8_t cmd = PN532_COMMAND_GETFIRMWAREVERSION;

  pn532_write_command(&cmd, 1);

  // TODO(michalc): add some sleep and timeout.
  while(!pn532_is_ready());

  if(!pn532_read_ack()) return ESP_FAIL;

  // TODO(michalc): add some sleep and timeout.
  while(!pn532_is_ready());

  uint8_t response[12];
  pn532_read_n(response, 12);

  return (0 == memcmp((void*)response, (void*)_pn532_fw_version, 6));
}

esp_err_t
pn532_init(spi_device_handle_t spi)
{
  esp_err_t ret = ESP_FAIL;
  pn532_spi = spi;

  if(pn532_read_fw_version())
  {
    printf("%s", "PN532 present!\n");
    ret = ESP_OK;
  }

  return ret;
}

bool
pn532_test_picc_presence()
{
  return true;
}

bool
pn532_anti_collision(uint8_t cascade_level)
{
  return true;
}
