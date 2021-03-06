#ifndef PICC_H
#define PICC_H


#define PICC_CMD_REQA                 0x26
#define PICC_CMD_HALTA                0x50
#define PICC_CMD_WUPA                 0x52
#define PICC_CMD_SELECT_CL_1          0x93
#define PICC_CMD_SELECT_CL_2          0x95
#define PICC_CMD_SELECT_CL_3          0x97

// MIFARE set of commands.
#define PICC_CMD_MIFARE_READ          0x30
#define PICC_CMD_MIFARE_AUTH_KEY_A    0x60
#define PICC_CMD_MIFARE_AUTH_KEY_B    0x61
#define PICC_CMD_MIFARE_WRITE         0xA0
#define PICC_CMD_MIFARE_TRANSFER      0xB0
#define PICC_CMD_MIFARE_DECREMENT     0xC0
#define PICC_CMD_MIFARE_INCREMENT     0xC1
#define PICC_CMD_MIFARE_RESTORE       0xC2

#define PICC_CMD_MIFARE_UL_WRITE       0xA2  // Ultralight
#define PICC_CMD_MIFARE_RATS           0xE0

// The MIFARE Ultralight EV1, MIFARE Plus EV1 and MIFARE DESFire EV2 support
// the GET VERSION command.

// NTAG set of commands.
#define PICC_CMD_NTAG_GET_VERSION      0x60
#define PICC_CMD_NTAG_READ             0x30
#define PICC_CMD_NTAG_FAST_READ        0x3A
#define PICC_CMD_NTAG_WRITE            0xA2
#define PICC_CMD_NTAG_COMP_WRITE       0xA0
#define PICC_CMD_NTAG_READ_CNT         0x39
#define PICC_CMD_NTAG_PWD_AUTH         0x1B
#define PICC_CMD_NTAG_READ_SIG         0x3C

#define PICC_CASCADE_TAG               0x88
#define MIFARE_KEY_SIZE                   6

#define PICC_RESPONSE_ACK              0x0A
#define PICC_RESPONSE_NAK_INV_ARG      0x00
#define PICC_RESPONSE_NAK_CRC_ERR      0x01
#define PICC_RESPONSE_NAK_INV_AUTH     0x04
#define PICC_RESPONSE_NAK_WRITE_ERR    0x05

// Response to GET VERSION command.
// https://www.nxp.com/docs/en/data-sheet/NTAG213_215_216.pdf page 36.
typedef struct picc_version_t {
  uint8_t fixed_header;
  uint8_t vendor_id;
  uint8_t product_type;
  uint8_t product_subtype;
  uint8_t maj_product_ver;
  uint8_t min_product_ver;
  uint8_t storage_size;
  uint8_t protocol_type;
} picc_version_t;

// The PICC types this library supports.
typedef enum {
  PICC_NOT_SUPPORTED,
  PICC_SUPPORTED_MIFARE_1K,
  PICC_SUPPORTED_NTAG213,
  PICC_SUPPORTED_COUNT
} picc_supported_e;

#endif // PICC_H
