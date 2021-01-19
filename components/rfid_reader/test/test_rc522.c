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
  spi_device_handle_t spi = periph_get_spi_handle();

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
