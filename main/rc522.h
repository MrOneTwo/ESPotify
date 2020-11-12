/*
 * This is a driver for the RC522 RFID module. It's important to keep this simple.
 *
 * - no complicated peripheral initing/management inside this module.
 */

#ifndef RC522_H
#define RC522_H

#include "driver/spi_master.h"

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

#define PICC_CMD_REQA             0x26
#define PICC_CMD_HALTA            0x50
#define PICC_CMD_WUPA             0x52
#define PICC_CMD_MF_READ          0x30
#define PICC_CMD_MF_WRITE         0xA0
#define PICC_CMD_SELECT_CL_1      0x93
#define PICC_CMD_SELECT_CL_2      0x95
#define PICC_CMD_SELECT_CL_3      0x97

#define PICC_CASCADE_TAG          0x88

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

typedef void(*rc522_tag_callback_t)(uint8_t*);

// TODO(michalc): throw this structure out.
typedef struct {
    int miso_io;
    int mosi_io;
    int sck_io;
    int sda_io;
    rc522_tag_callback_t callback;
} rc522_start_args_t;

esp_err_t rc522_init(spi_device_handle_t* spi);

esp_err_t rc522_write_n(uint8_t addr, uint8_t data_size, uint8_t *data);
esp_err_t rc522_write(uint8_t addr , uint8_t val);
uint8_t* rc522_read_n(uint8_t addr, uint8_t n) ;
uint8_t rc522_read(uint8_t addr);
esp_err_t rc522_set_bitmask(uint8_t addr, uint8_t mask);
esp_err_t rc522_clear_bitmask(uint8_t addr, uint8_t mask);

#define rc522_fw_version() rc522_read(RC522_REG_FW_VERSION)
esp_err_t rc522_antenna_on();

/*
 * Use the RC522 for CRC calculation.
 */
uint8_t* rc522_calculate_crc(uint8_t *data, uint8_t data_size, uint8_t* crc_buf);

/*
 * Send a command to PICC (saved in data buffer). The cmd is usually RC522_CMD_TRANSCEIVE.
 *
 * This function returns both: response's size in bytes and in bits. That's because it's possible
 * to receive a partial byte. It's important during anti collision stage.
 */
uint8_t* rc522_picc_write(rc522_commands_e cmd,
                          uint8_t* data, uint8_t data_size,
                          uint8_t* response_size_bytes, uint32_t* response_size_bits);

/*
 * This function is for waking up a PICC. It transmits the REQA command.
 */
uint8_t rc522_picc_request(void);

/*
 * This performs the anticollision step of selecting PICC. It basically
 * means reading the UID of the PICC.
 */
uint8_t rc522_anti_collision(uint8_t cascade_level);

/*
 * This wraps the rc522_anti_collision function.
 */
uint8_t* rc522_get_picc_id();

esp_err_t rc522_start();
esp_err_t rc522_resume();
esp_err_t rc522_pause();

#endif // RC522_H
