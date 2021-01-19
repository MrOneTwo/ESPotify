#include "unity.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "rc522.h"
#include "periph.h"


TEST_CASE("rc522 init", "[rc522]")
{ 
  spi_device_handle_t spi = periph_get_spi_handle();

  TEST_ASSERT_EQUAL(ESP_OK, rc522_init(spi));
}

TEST_CASE("rc522 init NULL", "[rc522]")
{
  TEST_ASSERT_EQUAL(ESP_FAIL, rc522_init(NULL));
}

TEST_CASE("rc522 picc presence", "[rc522][picc_present]")
{
  spi_device_handle_t spi = periph_get_spi_handle();

  TEST_ASSERT_EQUAL(ESP_OK, rc522_init(spi));
  TEST_ASSERT_EQUAL(true, rc522_test_picc_presence());
}

TEST_CASE("rc522 anti collision", "[rc522][picc_present]")
{
  spi_device_handle_t spi = periph_get_spi_handle();

  TEST_ASSERT_EQUAL(ESP_OK, rc522_init(spi));
  TEST_ASSERT_EQUAL(true, rc522_test_picc_presence());
  TEST_ASSERT_EQUAL(true, rc522_anti_collision(1));
}

TEST_CASE("rc522 read PICC's data", "[rc522][picc_present]")
{
  spi_device_handle_t spi = periph_get_spi_handle();

  TEST_ASSERT_EQUAL(ESP_OK, rc522_init(spi));
  TEST_ASSERT_EQUAL(true, rc522_test_picc_presence());
  TEST_ASSERT_EQUAL(true, rc522_anti_collision(1));

  uint8_t block = 0;
  uint8_t transfer_buffer[18] = {};

  const uint8_t key[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  rc522_authenticate(PICC_CMD_MF_AUTH_KEY_A, block, key);

  rc522_read_picc_data(block, transfer_buffer);
  printf("\n---------");
  printf("\nData read from a PICC:\n");
  for (uint8_t i = 0; i < 18; i++)
  {
    printf("%x ", transfer_buffer[i]);
  }
  printf("\n---------\n");

  rc522_picc_halta(PICC_CMD_HALTA);
  // Clear the MFCrypto1On bit.
  rc522_clear_bitmask(RC522_REG_STATUS_2, 0x08);
}
