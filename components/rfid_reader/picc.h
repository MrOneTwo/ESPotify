#ifndef PICC_H
#define PICC_H


// MIFARE set of commands.
#define PICC_CMD_REQA             0x26
#define PICC_CMD_HALTA            0x50
#define PICC_CMD_WUPA             0x52
#define PICC_CMD_MF_READ          0x30
#define PICC_CMD_MF_AUTH_KEY_A    0x60
#define PICC_CMD_MF_AUTH_KEY_B    0x61
#define PICC_CMD_SELECT_CL_1      0x93
#define PICC_CMD_SELECT_CL_2      0x95
#define PICC_CMD_SELECT_CL_3      0x97
#define PICC_CMD_MF_WRITE         0xA0
#define PICC_CMD_UL_WRITE         0xA2
#define PICC_CMD_MF_TRANSFER      0xB0
#define PICC_CMD_MF_DECREMENT     0xC0
#define PICC_CMD_MF_INCREMENT     0xC1
#define PICC_CMD_MF_RESTORE       0xC2
#define PICC_CMD_RATS             0xE0

// NTAG set of commands.
#define PICC_CMD_NTAG_REQA             0x26
#define PICC_CMD_NTAG_HALTA            0x50
#define PICC_CMD_NTAG_WUPA             0x52
#define PICC_CMD_NTAG_MF_READ          0x30
#define PICC_CMD_NTAG_MF_FAST_READ     0x3A
#define PICC_CMD_NTAG_SELECT_CL_1      0x93
#define PICC_CMD_NTAG_SELECT_CL_2      0x95
#define PICC_CMD_NTAG_MF_WRITE         0xA0
#define PICC_CMD_NTAG_UL_WRITE         0xA2
#define PICC_CMD_NTAG_READ_CNT         0x39
#define PICC_CMD_NTAG_PWD_AUTH         0x1B
#define PICC_CMD_NTAG_READ_SIG         0x3C

#define PICC_CASCADE_TAG          0x88
#define MF_KEY_SIZE                  6


#endif // PICC_H
