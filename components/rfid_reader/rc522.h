/*
 * This is a driver for the RC522 RFID module. It's important to keep this simple.
 *
 * - no complicated peripheral initing/management inside this module.
 */

#ifndef RC522_H
#define RC522_H

#include "driver/spi_master.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "rfid_reader.h"
#include "picc.h"

#define RC522_REG_COMMAND         0x01
#define RC522_REG_COM_IRQ_EN_DI   0x02
#define RC522_REG_DIV_IRQ_EN_DI   0x03
#define RC522_REG_COM_IRQ         0x04
#define RC522_REG_DIV_IRQ         0x05
#define RC522_REG_ERROR           0x06
#define RC522_REG_STATUS_1        0x07
#define RC522_REG_STATUS_2        0x08
#define RC522_REG_FIFO_DATA       0x09
#define RC522_REG_FIFO_LEVEL      0x0A
#define RC522_REG_WATER_LEVEL     0x0B
#define RC522_REG_CONTROL         0x0C
#define RC522_REG_BIT_FRAMING     0x0D
#define RC522_REG_MODE            0x11
#define RC522_REG_TX_MODE         0x12
#define RC522_REG_RX_MODE         0x13
#define RC522_REG_TX_CONTROL_REG  0x14
#define RC522_REG_TX_ASK          0x15
#define RC522_REG_CRC_RESULT_1    0x21
#define RC522_REG_CRC_RESULT_2    0x22
#define RC522_REG_MOD_WIDTH       0x24
#define RC522_REG_RF_CFG          0x26
#define RC522_REG_TIMER_MODE      0x2A
#define RC522_REG_TIMER_PRESCALER 0x2B
#define RC522_REG_TIMER_RELOAD_1  0x2C
#define RC522_REG_TIMER_RELOAD_2  0x2D
#define RC522_REG_TIMER_COUNTER_1 0x2E
#define RC522_REG_TIMER_COUNTER_2 0x2F
#define RC522_REG_FW_VERSION      0x37

#define RC522_RF_GAIN_18dB       (010)
#define RC522_RF_GAIN_23dB       (011)
#define RC522_RF_GAIN_33dB       (100)
#define RC522_RF_GAIN_38dB       (101)
#define RC522_RF_GAIN_43dB       (110)
#define RC522_RF_GAIN_48dB       (111)

// Code Transfer buffer validity  Description
// Ah   -                         Acknowledge (ACK)
// 0h   valid                     invalid operation
// 1h   valid                     parity or CRC error
// 4h   invalid                   invalid operation
// 5h   invalid                   parity or CRC error
#define MF_ACK  0xA


typedef enum {
  RC522_CMD_IDLE          = (0b0000),
  RC522_CMD_MEM           = (0b0001),
  RC522_CMD_GEN_RANDOM_ID = (0b0010),
  RC522_CMD_CALC_CRC      = (0b0011),
  RC522_CMD_TRANSMIT      = (0b0100),
  RC522_CMD_NO_CMD_CHANGE = (0b0111),
  RC522_CMD_RECEIVE       = (0b1000),
  RC522_CMD_TRANSCEIVE    = (0b1100),
  RC522_CMD_MF_AUTH       = (0b1110),
  RC522_CMD_SOFT_RESET    = (0b1111),
} rc522_commands_e;

/*
 * The response from the RC522 + PICC is a repeating concept. It doesn't differ that much from
 * a normal uint8_t buffer. It just carries its size information in bits and bytes with it. That's
 * because the response size might not be a multiple of 8 bits.
 */
typedef struct response_t {
  uint8_t* data;
  uint32_t size_bytes;
  uint32_t size_bits;
} response_t;

// A example callback that the user can register.
void tag_handler(uint8_t* serial_no);

void picc_data_to_useful_data();


esp_err_t rc522_init(spi_device_handle_t spi);

esp_err_t rc522_write_n(uint8_t addr, uint8_t data_size, uint8_t *data);
esp_err_t rc522_write(uint8_t addr , uint8_t val);
uint8_t* rc522_read_n(uint8_t addr, uint8_t n) ;
uint8_t rc522_read(uint8_t addr);
esp_err_t rc522_clear_bitmask(uint8_t addr, uint8_t mask);
status_e rc522_picc_halta(uint8_t halta);

#define rc522_fw_version() rc522_read(RC522_REG_FW_VERSION)

/*
 * Send a command to PICC (saved in data buffer). The cmd is usually RC522_CMD_TRANSCEIVE.
 *
 * This function returns both: response's size in bytes and in bits. That's because it's possible
 * to receive a partial byte. It's important during anti collision stage.
 */
void rc522_picc_write(rc522_commands_e cmd, uint8_t* data, uint8_t data_size, response_t* response);


/*
 * This function tries to read the entire UID from the PICC. This is way more complicated than
 * one might expect, mostly because of different UID lengths (4, 7, 10 bytes) and a possiblity of
 * receiving partial byte.
 *
 * This function is a recursive function which tries to handle all the cascading levels of reading
 * UID into a global variable.
 *
 * Returns SUCCESS if a full UID has been read.
 */
status_e rc522_anti_collision(uint8_t cascade_level);

status_e rc522_test_picc_presence();

void rc522_read_picc_data(uint8_t block_adress, uint8_t buffer[16]);
void rc522_write_picc_data(uint8_t block_address, uint8_t buffer[18]);


/*
 * Authenticate a sector access. A sector can be protected with a key A or key B.
 * A proper key needs to be used. Default factory key is {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}.
 */
void rc522_authenticate(uint8_t cmd, uint8_t block_address, const uint8_t key[MF_KEY_SIZE]);

#endif // RC522_H