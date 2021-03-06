/*
 * This is a driver for the PN532 RFID module. It's important to keep this simple.
 *
 * - no complicated peripheral initing/management inside this module.
 */

#ifndef PN532_H
#define PN532_H

#include "driver/spi_master.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "rfid_reader.h"
#include "picc.h"


#define PN532_PREAMBLE      (0x00)
#define PN532_START_CODE1   (0x00)
#define PN532_START_CODE2   (0xFF)
#define PN532_POSTAMBLE     (0x00)

#define PN532_HOST_TO_PN532  (0xD4)
#define PN532_PN532_TO_HOST  (0xD5)

#define PN532_COMMAND_DIAGNOSE (0x00)
#define PN532_COMMAND_GETFIRMWAREVERSION (0x02)
#define PN532_COMMAND_GETGENERALSTATUS (0x04)
#define PN532_COMMAND_READREGISTER (0x06)
#define PN532_COMMAND_WRITEREGISTER (0x08)
#define PN532_COMMAND_READGPIO (0x0C)
#define PN532_COMMAND_WRITEGPIO (0x0E)
#define PN532_COMMAND_SETSERIALBAUDRATE (0x10)
#define PN532_COMMAND_SETPARAMETERS (0x12)
#define PN532_COMMAND_SAMCONFIGURATION (0x14)
#define PN532_COMMAND_POWERDOWN (0x16)
#define PN532_COMMAND_RFCONFIGURATION (0x32)
#define PN532_COMMAND_RFREGULATIONTEST (0x58)
#define PN532_COMMAND_INJUMPFORDEP (0x56)
#define PN532_COMMAND_INJUMPFORPSL (0x46)
#define PN532_COMMAND_INLISTPASSIVETARGET (0x4A)
#define PN532_COMMAND_INATR (0x50)
#define PN532_COMMAND_INPSL (0x4E)
#define PN532_COMMAND_INDATAEXCHANGE (0x40)
#define PN532_COMMAND_INCOMMUNICATETHRU (0x42)
#define PN532_COMMAND_INDESELECT (0x44)
#define PN532_COMMAND_INRELEASE (0x52)
#define PN532_COMMAND_INSELECT (0x54)
#define PN532_COMMAND_INAUTOPOLL (0x60)
#define PN532_COMMAND_TGINITASTARGET (0x8C)
#define PN532_COMMAND_TGSETGENERALBYTES (0x92)
#define PN532_COMMAND_TGGETDATA (0x86)
#define PN532_COMMAND_TGSETDATA (0x8E)
#define PN532_COMMAND_TGSETMETADATA (0x94)
#define PN532_COMMAND_TGGETINITIATORCOMMAND (0x88)
#define PN532_COMMAND_TGRESPONSETOINITIATOR (0x90)
#define PN532_COMMAND_TGGETTARGETSTATUS (0x8A)
#define PN532_RESPONSE_INDATAEXCHANGE (0x41)
#define PN532_RESPONSE_INLISTPASSIVETARGET (0x4B)

#define PN532_WAKEUP (0x55)

// PN532 User Guide page 45.
#define PN532_SPI_STAT_READ   (0x02)
#define PN532_SPI_DATA_WRITE  (0x01)
#define PN532_SPI_DATA_READ   (0x03)
#define PN532_SPI_READY       (0x01)


esp_err_t pn532_init(spi_device_handle_t spi);

bool pn532_test_picc_presence(void);

bool pn532_anti_collision(uint8_t cascade_level);

bool pn532_read_fw_version(void);

bool pn532_say_hello(void);


// PRIVATE BUT EXPOSED BECAUSE TESTED
bool _pn532_is_ready();
void _pn532_build_information_frame(uint8_t*buf, uint8_t* cmd, uint8_t cmdlen);

#endif // PN532_H
