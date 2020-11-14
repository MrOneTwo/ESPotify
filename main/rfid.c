#include "rfid.h"

#include "shared.h"
#include "rc522.h"


void rfid_init(spi_device_handle_t* spi)
{
  rc522_init(spi);
}

void rfid_start_scanning(void)
{
  rc522_start_scanning();
}

void rfid_save_song_to_picc(void)
{
}
