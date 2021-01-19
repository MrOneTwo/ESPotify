#include "unity.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "pn532.h"
#include "periph.h"


TEST_CASE("pn532 build information frame", "[pn532]")
{
  spi_device_handle_t spi = periph_get_spi_handle();

  uint8_t cmd[1] = {PN532_COMMAND_GETFIRMWAREVERSION};
  uint8_t cmdlen = 1;

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

  crc = PN532_HOST_TO_PN532;

  for (uint8_t i = 0; i < cmdlen - 1; i++)
  {
    *p++ = cmd[i];
    crc += cmd[i];
  }

  *p++ = ~crc + 1;
  *p++ = PN532_POSTAMBLE;


  TEST_ASSERT_EQUAL(PN532_SPI_DATA_WRITE, packet[0]);
  TEST_ASSERT_EQUAL(PN532_PREAMBLE, packet[1]);
  TEST_ASSERT_EQUAL(PN532_START_CODE1, packet[2]);
  TEST_ASSERT_EQUAL(PN532_START_CODE2, packet[3]);
  TEST_ASSERT_EQUAL(cmdlen, packet[4]);
  TEST_ASSERT_EQUAL(0x0, 0xFF & (cmdlen + packet[5]));
  TEST_ASSERT_EQUAL(PN532_HOST_TO_PN532, packet[6]);

  for (uint8_t i = 0; i < cmdlen - 1; i++)
  {
    TEST_ASSERT_EQUAL(cmd[i], packet[7 + i]);
  }

  TEST_ASSERT_EQUAL(0x0, 0xFF & (crc + packet[8 + cmdlen - 2]));
  TEST_ASSERT_EQUAL(PN532_POSTAMBLE, packet[8 + cmdlen - 1]);
}

TEST_CASE("pn532 init NULL", "[pn532]")
{
  TEST_ASSERT_EQUAL(ESP_FAIL, pn532_init(NULL));
}

TEST_CASE("pn532 is ready", "[pn532]")
{
  spi_device_handle_t spi = periph_get_spi_handle();

  uint8_t cmd = PN532_SPI_STAT_READ;
  uint8_t response = 0;

  spi_transaction_t t = {};

  // Yes, the length is in bits.
  t.length = 8 * (1);
  t.rxlength = 8 * (1);
  t.tx_buffer = (uint8_t*)&cmd;
  t.rx_buffer = (uint8_t*)&response;

  esp_err_t ret = spi_device_transmit(spi, &t);

  TEST_ASSERT_EQUAL(ESP_OK, ret);
  TEST_ASSERT_EQUAL(PN532_SPI_READY, response);
}
