#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/uart.h"

#include <stdbool.h>
#include <string.h>

#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  18
#define PIN_NUM_CS   5

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

#define GPIO_IRQ_PIN       15 // TODO(michalc): choose a pin
#define GPIO_INPUT_PIN_SEL (1ULL << GPIO_IRQ_PIN)
// const uint32_t _GPIO_IRQ_PIN = 15U;
uint8_t _GPIO_IRQ_PIN[2] = {15U, 0U};

#define BUF_SIZE 512
static QueueHandle_t uart1_queue;
spi_device_handle_t _spi;

// For ESP32 the UART 2 uses pins 16 (rx) and 17 (tx).
#define UART_PERIPH UART_NUM_2

static void
uart_event_task(void *pvParameters)
{
    uart_event_t event;
    size_t buffered_size;
    uint8_t *data = (uint8_t *)malloc(BUF_SIZE);

    while (true) {
        if (xQueueReceive(uart1_queue, (void *)&event, (TickType_t)portMAX_DELAY)) {
            memset(data, 0, BUF_SIZE);
            switch (event.type) {
            case UART_DATA:
                uart_read_bytes(UART_PERIPH, data, event.size, portMAX_DELAY);
                break;
            case UART_FIFO_OVF:
                uart_flush_input(UART_PERIPH);
                xQueueReset(uart1_queue);
                break;
            case UART_BUFFER_FULL:
                uart_flush_input(UART_PERIPH);
                xQueueReset(uart1_queue);
                break;
            case UART_BREAK:
                break;
            case UART_PARITY_ERR:
                break;
            case UART_FRAME_ERR:
                break;
            case UART_PATTERN_DET: {
                uart_get_buffered_data_len(UART_PERIPH, &buffered_size);
                int pos = uart_pattern_pop_pos(UART_PERIPH);
                (void)pos;
            } break;
            default:
                break;
            }
        }
    }
}

static void
spi_pretransfer_callback(spi_transaction_t *t)
{
    int dc = (int)t->user;
    // TODO(michalc): I don't know if this is necessary. I never specified the post callback and yet
    // the CS works ok. In the SPI example this is used to set a pin state but it's for a DC pin.
    // gpio_set_level(PIN_NUM_CS, dc);
}

static void
gpio_isr_callback(void *arg)
{
    uint8_t gpio_num = ((uint8_t *)arg)[0];

    if (gpio_num == GPIO_IRQ_PIN) {
        ((uint8_t *)arg)[1] = 1;
    }
}

// TODO(michalc): this probably should be more robust.
uint8_t
periph_get_button_state(uint8_t clear)
{
    uint8_t ret = _GPIO_IRQ_PIN[1];
    if (clear) {
        _GPIO_IRQ_PIN[1] = 0U;
    }
    return ret;
}

// static void gpio_isr_handler(void* arg)
// {
//   uint32_t gpio_num = *(uint32_t*)arg;
//   if (gpio_num == GPIO_IRQ_PIN)
//   {
//   }
// }

spi_device_handle_t
periph_get_spi_handle(void)
{
    return _spi;
}

#ifdef CONFIG_RFID_READER
void
periph_init_spi(void)
{
    esp_err_t ret;

    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_NUM_MISO,
        .mosi_io_num = PIN_NUM_MOSI,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 0, // 0 results in 4094 bytes.
    };
#if defined(CONFIG_RC522)
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 10 * 1000 * 1000, // PN532 max 5MHz, RC522 max 10MHz
        .mode = 0,                          // SPI mode 0
        .spics_io_num = PIN_NUM_CS,         // CS pin
        .queue_size = 7,                    // transactions queue size
        .pre_cb = spi_pretransfer_callback, // pre transfer to toggle CS
        .flags = SPI_DEVICE_HALFDUPLEX};
#elif defined(CONFIG_PN532)
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 5 * 1000 * 1000,  // PN532 max 5MHz, RC522 max 10MHz
        .mode = 0,                          // SPI mode 0
        .spics_io_num = PIN_NUM_CS,         // CS pin
        .queue_size = 7,                    // transactions queue size
        .pre_cb = spi_pretransfer_callback, // pre transfer to toggle CS
        .flags = SPI_DEVICE_HALFDUPLEX | SPI_DEVICE_TXBIT_LSBFIRST | SPI_DEVICE_RXBIT_LSBFIRST};
#endif

    ret = spi_bus_initialize(VSPI_HOST, &buscfg, 0);
    switch (ret) {
    case ESP_ERR_INVALID_ARG:
        ESP_ERROR_CHECK(ret);
        break;
    case ESP_ERR_INVALID_STATE:
        ESP_ERROR_CHECK(ret);
        break;
        break;
    case ESP_ERR_NO_MEM:
        ESP_ERROR_CHECK(ret);
        break;
    case ESP_OK:
    default:
        break;
    }

    ret = spi_bus_add_device(VSPI_HOST, &devcfg, &_spi);
    ESP_ERROR_CHECK(ret);
}
#endif // CONFIG_RFID_READER

static void
periph_init_uart(void)
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    int intr_alloc_flags = 0;
    ESP_ERROR_CHECK(uart_driver_install(UART_PERIPH, BUF_SIZE * 2, BUF_SIZE * 2, 8, &uart1_queue,
                                        intr_alloc_flags));
    uart_param_config(UART_PERIPH, &uart_config);
    uart_set_mode(UART_PERIPH, UART_MODE_IRDA);
    ESP_ERROR_CHECK(uart_set_pin(UART_PERIPH, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    xTaskCreate(uart_event_task, "uart_event_task", 2048, NULL, 12, NULL);
}

static void
periph_init_gpio(void (*gpio_cb)(void *arg), void *gpio_cb_arg)
{
    esp_err_t ret;

    // GPIO
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .pin_bit_mask = GPIO_INPUT_PIN_SEL,
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = 0,
        .pull_up_en = 1,
    };
    ret = gpio_config(&io_conf);
    ESP_ERROR_CHECK(ret);

    ret = gpio_install_isr_service(ESP_INTR_FLAG_EDGE);
    ESP_ERROR_CHECK(ret);
    ret = gpio_isr_handler_add(GPIO_IRQ_PIN, gpio_cb, (void *)(_GPIO_IRQ_PIN));
    ESP_ERROR_CHECK(ret);
}

void
periph_init()
{
    periph_init_gpio(gpio_isr_callback, (void *)_GPIO_IRQ_PIN);
#ifdef CONFIG_RFID_READER
    periph_init_spi();
#endif // CONFIG_RFID_READER
    periph_init_uart();
}
