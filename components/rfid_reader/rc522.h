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

typedef struct picc_t {
  uint8_t uid_hot;
  uint8_t uid_full;
  uint8_t uid[10];
  uint8_t uid_bits;
// Types:
// case 0x04:	return PICC_TYPE_NOT_COMPLETE;
// case 0x09:	return PICC_TYPE_MIFARE_MINI;
// case 0x08:	return PICC_TYPE_MIFARE_1K;
// case 0x18:	return PICC_TYPE_MIFARE_4K;
// case 0x00:	return PICC_TYPE_MIFARE_UL;
// case 0x10:
// case 0x11:	return PICC_TYPE_MIFARE_PLUS;
// case 0x01:	return PICC_TYPE_TNP3XXX;
// case 0x20:	return PICC_TYPE_ISO_14443_4;
// case 0x40:	return PICC_TYPE_ISO_18092;
// default:	return PICC_TYPE_UNKNOWN;
  picc_supported_e type;
  picc_version_t ver;
} picc_t;

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

esp_err_t rc522_write_n(uint8_t addr, uint8_t data_size, const uint8_t* const data);
esp_err_t rc522_write(uint8_t addr , uint8_t val);
uint8_t* rc522_read_n(uint8_t addr, uint8_t n) ;
uint8_t rc522_read(uint8_t addr);
esp_err_t rc522_clear_bitmask(uint8_t addr, uint8_t mask);
status_e rc522_picc_halta(uint8_t halta);
status_e rc522_picc_get_version(void);

/*
 * EXPOSED BECAUSE OF TESTING!!!
 * This function is for waking up a PICC. It transmits the REQA or WUPA command.
 * If the PICC responds then the anticollision procedure can be performed.
 */
status_e rc522_picc_reqa_or_wupa(uint8_t reqa_or_wupa);


#define rc522_fw_version() rc522_read(RC522_REG_FW_VERSION)


/*
 * Init the rc522 module. No device communication is performed at this step.
 *
 * Return ESP_OK if the initialization succeeded, ESP_FAIL if it didn't.
 */
esp_err_t rc522_init(spi_device_handle_t spi);

/*
 * Test communication with the RC522 reader. This also turn on the antenna and
 * reads the firmware version of the RC522 reader.
 *
 * Return true if the communication succeeded.
 */
bool rc522_say_hello(void);

/*
 * Send a command to PICC (saved in data buffer). The cmd is usually RC522_CMD_TRANSCEIVE.
 *
 * This function returns both: response's size in bytes and in bits. That's because it's possible
 * to receive a partial byte. It's important during anti collision stage.
 */
void rc522_picc_write(rc522_commands_e cmd, const uint8_t* const data, const uint8_t data_size, response_t* const response);

/*
 * Sends a PICC_CMD_REQA to move the PICC from IDLE state into Ready 1 state.
 *
 * Return true when PICC responds with ATQA.
 */
bool rc522_test_picc_presence(void);

/*
 * Copy the last detected PICC's UID into buf. The UID's size in bytes gets saved
 * in the size argument.
 */
picc_t rc522_get_last_picc(void);

/*
 * This function tries to read the entire UID from the PICC. This is way more complicated than
 * one might expect, mostly because of different UID lengths (4, 7, 10 bytes) and a possiblity of
 * receiving partial byte. First be sure to set the PICC into Ready 1 state with the rc522_test_picc_presence().
 *
 * This function is a recursive function which tries to handle all the cascading levels of reading
 * UID into a global variable.
 *
 * cascade_level - should be set to 1 when calling explicitly. It will internally call itself with
 *                 increasing cascade_level, to fetch the entire UID.
 *
 * Returns true if a full UID has been read.
 */
bool rc522_anti_collision(uint8_t cascade_level);

status_e rc522_read_picc_data(uint8_t block_adress, uint8_t buffer[16]);
void rc522_write_picc_data(const uint8_t block_address, uint8_t* data, const uint32_t data_len);


/*
 * Authenticate a sector access. A sector can be protected with a key A or key B.
 * A proper key needs to be used. Default factory key is {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}.
 */
void rc522_authenticate(uint8_t cmd, uint8_t block_address, const uint8_t key[MIFARE_KEY_SIZE]);

#endif // RC522_H
