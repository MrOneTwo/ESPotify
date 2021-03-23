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

  picc_t picc = rc522_get_last_picc();
  printf("Detected PICC with UID: ");
  for (int i = 0; i < (picc.uid_bits / 8); i++)
  {
    printf("%02x ", picc.uid[i]);
  }
  printf("\n");

  rc522_picc_halta(PICC_CMD_HALTA);
}

TEST_CASE("rc522 try GET VERSION command", "[rc522][picc_present]")
{
  spi_device_handle_t spi = periph_get_spi_handle();

  TEST_ASSERT_EQUAL(ESP_OK, rc522_init(spi));
  TEST_ASSERT_EQUAL(true, rc522_picc_reqa_or_wupa(PICC_CMD_WUPA));
  TEST_ASSERT_EQUAL(true, rc522_anti_collision(1));

  TEST_ASSERT_EQUAL(SUCCESS, rc522_picc_get_version());

  picc_t picc = rc522_get_last_picc();

  printf("Detected PICC with UID: ");
  for (int i = 0; i < (picc.uid_bits / 8); i++)
  {
    printf("%02x ", picc.uid[i]);
  }
  printf("\n");

  printf("  %-24s : 0x%02x\n", "fixed_header", picc.ver.fixed_header);
  printf("  %-24s : 0x%02x\n", "vendor_id", picc.ver.vendor_id);
  printf("  %-24s : 0x%02x\n", "product_type", picc.ver.product_type);
  printf("  %-24s : 0x%02x\n", "product_subtype", picc.ver.product_subtype);
  printf("  %-24s : 0x%02x\n", "maj_product_ver", picc.ver.maj_product_ver);
  printf("  %-24s : 0x%02x\n", "min_product_ver", picc.ver.min_product_ver);
  printf("  %-24s : 0x%02x\n", "storage_size", picc.ver.storage_size);
  printf("  %-24s : 0x%02x\n", "protocol_type", picc.ver.protocol_type);

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
