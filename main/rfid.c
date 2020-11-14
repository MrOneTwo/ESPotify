#include "rfid.h"

#include "shared.h"
#include "rc522.h"

QueueHandle_t queue;

void rfid_init(spi_device_handle_t spi, QueueHandle_t q)
{
  queue = q;
  rc522_init(spi, q);
}

void rfid_start_scanning()
{
  rc522_start_scanning();
}

void rfid_save_song_to_picc(void)
{
}
