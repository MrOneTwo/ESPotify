#include "rfid.h"

#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "shared.h"
#include "rc522.h"

#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  18
#define PIN_NUM_CS    5

/*
 * ESP32 WROVER
 * 1.GPIO12 is internally pulled high in the module and is not recommended for use as a touch pin.
 * 2.Pins SCK/CLK, SDO/SD0, SDI/SD1, SHD/SD2, SWP/SD3 and SCS/CMD, namely, GPIO6 to GPIO11 are
 *   connected to the SPI flash integrated on the module and are not recommended for other uses.
 *
 * 1 .GPIO12 is internally pulled high in the module and is not recommended for use as a touch pin.
 * 2. External connections can be made to any GPIO except for GPIOs in the range 6-11, 16, or 17.
 *    GPIOs 6-11 areconnected to the module’s integrated SPI flash and PSRAM. GPIOs 16 and 17 are
 *    connected to the module’sintegrated PSRAM. For details, please see Chapter 6 Schematics.
 *
 * 0~39 except for 20, 24, 28~31 are valid pins (soc_caps.h).
 */

#define GPIO_IRQ_PIN 15  // TODO(michalc): choose a pin
#define GPIO_INPUT_PIN_SEL  (1ULL << GPIO_IRQ_PIN)
const uint32_t _GPIO_IRQ_PIN = 15U;

uint8_t newTagPresent = 0;
spi_device_handle_t spi;


static void spi_pretransfer_callback(spi_transaction_t *t)
{
  int dc=(int)t->user;
  gpio_set_level(PIN_NUM_CS, dc);
}

static void gpio_isr_handler(void* arg)
{
  uint32_t gpio_num = *(uint32_t*)arg;
  if (gpio_num == GPIO_IRQ_PIN)
  {
    newTagPresent = true;
  }
}

void rfid_init()
{
  esp_err_t ret;

  // GPIO
  gpio_config_t io_conf = {
    .intr_type = GPIO_INTR_POSEDGE,
    .pin_bit_mask = GPIO_INPUT_PIN_SEL,
    .mode = GPIO_MODE_INPUT,
    .pull_down_en = 1,
    .pull_up_en = 0,
  };
  ret = gpio_config(&io_conf);
  ESP_ERROR_CHECK(ret);

  ret = gpio_install_isr_service(ESP_INTR_FLAG_EDGE);
  ESP_ERROR_CHECK(ret);
  ret = gpio_isr_handler_add(GPIO_IRQ_PIN, gpio_isr_handler, (void*)(&_GPIO_IRQ_PIN));
  ESP_ERROR_CHECK(ret);

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
      // NOTE(michalc): is it ok to just skip when it's already intilized?
      break;
    case ESP_ERR_NO_MEM:
      ESP_ERROR_CHECK(ret); break;
    case ESP_OK:
    default:
      break;
  }

  ret = spi_bus_add_device(VSPI_HOST, &devcfg, &spi);
  ESP_ERROR_CHECK(ret);

  rc522_init(&spi);
  rc522_start();
}

// void rfid_read()
// {
//   esp_err_t ret;
//   spi_transaction_t t = {};
//   t.length = 8;
//   t.tx_buffer = &cmd;
//   t.user = (void*)0;
//   ret = spi_device_polling_transmit(spi, &t);
//   ESP_ERROR_CHECK(ret);
// }
