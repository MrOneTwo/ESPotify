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

TEST_CASE("rc522 hello", "[rc522]")
{
  spi_device_handle_t spi = periph_get_spi_handle();

  TEST_ASSERT_EQUAL(ESP_OK, rc522_init(spi));
  TEST_ASSERT_EQUAL(true, rc522_say_hello());
}

TEST_CASE("rc522 picc presence", "[rc522][picc_present]")
{
  spi_device_handle_t spi = periph_get_spi_handle();

  TEST_ASSERT_EQUAL(ESP_OK, rc522_init(spi));
  TEST_ASSERT_EQUAL(true, rc522_picc_reqa_or_wupa(PICC_CMD_WUPA));

  rc522_picc_halta(PICC_CMD_HALTA);
}

TEST_CASE("rc522 anti collision", "[rc522][picc_present]")
{
  spi_device_handle_t spi = periph_get_spi_handle();

  TEST_ASSERT_EQUAL(ESP_OK, rc522_init(spi));
  TEST_ASSERT_EQUAL(true, rc522_picc_reqa_or_wupa(PICC_CMD_WUPA));
  TEST_ASSERT_EQUAL(true, rc522_anti_collision(1));

  char uid[10] = {};
  uint8_t uid_size = 0;
  rc522_get_last_picc_uid(uid, &uid_size);
  printf("Detected PICC with UID: ");
  for (int i = 0; i < uid_size; i++)
  {
    printf("%x ", uid[i]);
  }
  printf("\n");

  rc522_picc_halta(PICC_CMD_HALTA);
}

TEST_CASE("rc522 read PICC's data", "[rc522][picc_present]")
{
  spi_device_handle_t spi = periph_get_spi_handle();

  TEST_ASSERT_EQUAL(ESP_OK, rc522_init(spi));

  uint8_t picc_data[16] = {};
  const uint8_t key[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  for (uint8_t sector = 0; sector < 16; sector++)
  {
    {
      uint8_t block = 0 + sector * 4;
      TEST_ASSERT_EQUAL(true, rc522_picc_reqa_or_wupa(PICC_CMD_WUPA));
      TEST_ASSERT_EQUAL(true, rc522_anti_collision(1));

      rc522_authenticate(PICC_CMD_MIFARE_AUTH_KEY_A, block, key);

      rc522_read_picc_data(block, picc_data);
      printf("%02d: ", block);
      for (uint8_t i = 0; i < 16; i++)
      {
        printf("%02x ", picc_data[i]);
      }
      printf("\n");

      rc522_picc_halta(PICC_CMD_HALTA);
      // Clear the MFCrypto1On bit.
      rc522_clear_bitmask(RC522_REG_STATUS_2, 0x08);
    }
    {
      uint8_t block = 1 + sector * 4;
      TEST_ASSERT_EQUAL(true, rc522_picc_reqa_or_wupa(PICC_CMD_WUPA));
      TEST_ASSERT_EQUAL(true, rc522_anti_collision(1));

      rc522_authenticate(PICC_CMD_MIFARE_AUTH_KEY_A, block, key);

      rc522_read_picc_data(block, picc_data);
      printf("%02d: ", block);
      for (uint8_t i = 0; i < 16; i++)
      {
        printf("%02x ", picc_data[i]);
      }
      printf("\n");

      rc522_picc_halta(PICC_CMD_HALTA);
      // Clear the MFCrypto1On bit.
      rc522_clear_bitmask(RC522_REG_STATUS_2, 0x08);
    }
    {
      uint8_t block = 2 + sector * 4;
      TEST_ASSERT_EQUAL(true, rc522_picc_reqa_or_wupa(PICC_CMD_WUPA));
      TEST_ASSERT_EQUAL(true, rc522_anti_collision(1));

      rc522_authenticate(PICC_CMD_MIFARE_AUTH_KEY_A, block, key);

      rc522_read_picc_data(block, picc_data);
      printf("%02d: ", block);
      for (uint8_t i = 0; i < 16; i++)
      {
        printf("%02x ", picc_data[i]);
      }
      printf("\n");

      rc522_picc_halta(PICC_CMD_HALTA);
      // Clear the MFCrypto1On bit.
      rc522_clear_bitmask(RC522_REG_STATUS_2, 0x08);
    }
    {
      uint8_t block = 3 + sector * 4;
      TEST_ASSERT_EQUAL(true, rc522_picc_reqa_or_wupa(PICC_CMD_WUPA));
      TEST_ASSERT_EQUAL(true, rc522_anti_collision(1));

      rc522_authenticate(PICC_CMD_MIFARE_AUTH_KEY_A, block, key);

      rc522_read_picc_data(block, picc_data);
      printf("%02d: ", block);
      for (uint8_t i = 0; i < 16; i++)
      {
        printf("%02x ", picc_data[i]);
      }
      printf("\n");

      rc522_picc_halta(PICC_CMD_HALTA);
      // Clear the MFCrypto1On bit.
      rc522_clear_bitmask(RC522_REG_STATUS_2, 0x08);
    }
  }
}
