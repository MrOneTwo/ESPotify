#include "rfid.h"

#include "shared.h"
#include "rc522.h"

#include <string.h>


void rfid_init(spi_device_handle_t spi)
{
  rc522_init(spi);
}

void rfid_start_scanning()
{
  rc522_start_scanning();
}

void rfid_save_song_to_picc(char* song_id)
{
  const char* _song_id = "5lRNFkvM9qPxjt3U3drUrl";

  const uint8_t key[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  const uint8_t block = 5;

  rc522_authenticate(PICC_CMD_MF_AUTH_KEY_A, block, key);

  uint8_t data[18] = {};
  // Leave first four bytes for a TAG.
  memcpy(data + 4, _song_id, 12);
  rc522_write_picc_data(5, data);

  memcpy(data, _song_id + 12, strlen(_song_id) - 12);
  rc522_write_picc_data(6, data);

  rc522_picc_halta(PICC_CMD_HALTA);
  // Clear the MFCrypto1On bit.
  rc522_clear_bitmask(RC522_REG_STATUS_2, 0x08);
}

status_e rfid_tag_present(void)
{
  return rc522_get_picc_id();
}
