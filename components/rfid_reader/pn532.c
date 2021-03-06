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

bool
_pn532_is_ready()
{
  uint8_t cmd = PN532_SPI_STAT_READ;
  uint8_t response = 0;

  {
    spi_transaction_t t = {};

    // Yes, the length is in bits.
    t.length = 8 * (1);
    t.rxlength = 8 * (1);
    t.tx_buffer = (uint8_t*)&cmd;
    t.rx_buffer = (uint8_t*)&response;

    esp_err_t ret = spi_device_transmit(pn532_spi, &t);
  }

  return (response & PN532_SPI_READY);
}

void
_pn532_build_information_frame(uint8_t* buf, uint8_t* cmd, uint8_t cmdlen)
{
  int crc;
  const uint8_t frame_size = 8 + cmdlen;

  buf[0] = PN532_SPI_DATA_WRITE;
  buf[1] = PN532_PREAMBLE;
  buf[2] = PN532_START_CODE1;
  buf[3] = PN532_START_CODE2;
  // cmdlen is the TFI + data fields.
  buf[4] = cmdlen + 1;
  buf[5] = ~cmdlen;
  buf[6] = PN532_HOST_TO_PN532;

  crc = PN532_PREAMBLE + PN532_START_CODE1 + PN532_START_CODE2 + PN532_HOST_TO_PN532;

  uint8_t* p = &buf[7];
  for (uint8_t i = 0; i < cmdlen; i++)
  {
    *p = cmd[i];
    crc += cmd[i];
    p++;
  }

  *p++ = ~crc;
  *p = PN532_POSTAMBLE;
}

static esp_err_t
pn532_write_command(uint8_t* cmd, uint8_t cmdlen)
{
  const uint8_t frame_size = 8 + cmdlen;
  uint8_t packet[frame_size];

  _pn532_build_information_frame(packet, cmd, cmdlen);

  esp_err_t ret = ESP_FAIL;
  {
    spi_transaction_t t = {};

    // Yes, the length is in bits.
    t.length = 8 * (frame_size  + cmdlen);
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
  while(!_pn532_is_ready());

  if(!pn532_read_ack()) return 0;

  // TODO(michalc): add some sleep and timeout.
  while(!_pn532_is_ready());

  uint8_t response[12];
  pn532_read_n(response, 12);

  return (0 == memcmp((void*)response, (void*)_pn532_fw_version, 6));
}

esp_err_t
pn532_init(spi_device_handle_t spi)
{
  if (spi != NULL)
  {
    pn532_spi = spi;
    return ESP_OK;
  }
  return ESP_FAIL;
}

bool
pn532_say_hello()
{
  if(pn532_read_fw_version())
  {
    printf("%s", "PN532 present!\n");
    return true;
  }

  return false;
}

bool
pn532_test_picc_presence()
{
  return false;
}

bool
pn532_anti_collision(uint8_t cascade_level)
{
  return false;
}
