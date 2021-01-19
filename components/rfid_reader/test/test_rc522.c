#include "unity.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "rc522.h"
#include "periph.h"


TEST_CASE("rc522 init", "rc522")
{ 
  spi_device_handle_t spi = periph_get_spi_handle();

  TEST_ASSERT_EQUAL(ESP_OK, rc522_init(spi));
}
