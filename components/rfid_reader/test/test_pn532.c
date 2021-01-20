#include "unity.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "pn532.h"
#include "periph.h"

#include <string.h>


TEST_CASE("pn532 init NULL", "[pn532]")
{
  TEST_ASSERT_EQUAL(ESP_FAIL, pn532_init(NULL));
}

TEST_CASE("pn532 init", "[pn532]")
{
  spi_device_handle_t spi = periph_get_spi_handle();
  TEST_ASSERT_EQUAL(ESP_OK, pn532_init(spi));
}

TEST_CASE("pn532 firmware information frame", "[pn532]")
{
  const uint8_t frame_size = 10;
  uint8_t packet[frame_size];
  uint8_t cmd = PN532_COMMAND_GETFIRMWAREVERSION;
  const uint8_t cmdlen = 1;

  _pn532_build_information_frame(packet, &cmd, cmdlen);

  // This result is an output read with logic analyzer on a working Arduino run.
  const uint8_t result[10] = {0x01, 0x00, 0x00, 0xFF, 0x02, 0xFE, 0xD4, 0x02, 0x2A, 0x00};
  TEST_ASSERT_EQUAL(0, memcmp(packet, result, frame_size));
}

TEST_CASE("pn532 say hello", "[pn532]")
{
  spi_device_handle_t spi = periph_get_spi_handle();
  TEST_ASSERT_EQUAL(ESP_OK, pn532_init(spi));

  TEST_ASSERT_EQUAL(true, pn532_say_hello());
}

// TODO(michalc): this should use the actual implementation.
TEST_CASE("pn532 is ready", "[pn532]")
{
  spi_device_handle_t spi = periph_get_spi_handle();
  TEST_ASSERT_EQUAL(ESP_OK, pn532_init(spi));

  uint8_t cmd = PN532_SPI_STAT_READ;
  uint8_t response = 0;

  // Send request to report the RDY state.
  {
    spi_transaction_t t = {};

    // Yes, the length is in bits.
    t.length = 8 * (1);
    t.tx_buffer = (uint8_t*)&cmd;

    esp_err_t ret = spi_device_transmit(spi, &t);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
  }

  {
    spi_transaction_t t = {};

    // Yes, the length is in bits.
    t.length = 8 * (1);
    t.rxlength = 8 * (1);
    t.rx_buffer = (uint8_t*)&response;

    esp_err_t ret = spi_device_transmit(spi, &t);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
  }

  printf("Response to pn532: %x\n", response);

  TEST_ASSERT_EQUAL(PN532_SPI_READY, (response & PN532_SPI_READY));
}

TEST_CASE("pn532 firmware version", "[pn532]")
{
  spi_device_handle_t spi = periph_get_spi_handle();
  TEST_ASSERT_EQUAL(ESP_OK, pn532_init(spi));

  TEST_ASSERT_EQUAL(true, pn532_read_fw_version());
}

