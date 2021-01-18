#include "unity.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "pn532.h"


#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  18
#define PIN_NUM_CS    5


static void spi_pretransfer_callback(spi_transaction_t *t)
{
  int dc=(int)t->user;
  gpio_set_level(PIN_NUM_CS, dc);
}


TEST_CASE("pn532 ready", "pn532")
{ 
  esp_err_t ret;
  spi_device_handle_t _spi;

  // SPI
  spi_bus_config_t buscfg = {
      .miso_io_num = PIN_NUM_MISO,
      .mosi_io_num = PIN_NUM_MOSI,
      .sclk_io_num = PIN_NUM_CLK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 0, // 0 results in 4094 bytes.
  };
  spi_device_interface_config_t devcfg = {
      .clock_speed_hz = 10*1000*1000,           // 10 MHz
      .mode = 0,                                // SPI mode 0
      .spics_io_num = PIN_NUM_CS,               // CS pin
      .queue_size = 7,                          // transactions queue size
      .pre_cb = spi_pretransfer_callback,       // pre transfer to toggle CS
      .flags = SPI_DEVICE_HALFDUPLEX
  };

  ret = spi_bus_initialize(VSPI_HOST, &buscfg, 0);
  switch (ret)
  {
    case ESP_ERR_INVALID_ARG:
      ESP_ERROR_CHECK(ret); break;
    case ESP_ERR_INVALID_STATE:
      ESP_ERROR_CHECK(ret); break;
      break;
    case ESP_ERR_NO_MEM:
      ESP_ERROR_CHECK(ret); break;
    case ESP_OK:
    default:
      break;
  }

  ret = spi_bus_add_device(VSPI_HOST, &devcfg, &_spi);
  ESP_ERROR_CHECK(ret);

  pn532_init(_spi);

  TEST_ASSERT_EQUAL(true, pn532_is_ready());
}
