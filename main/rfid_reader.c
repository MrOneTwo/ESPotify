#include "rfid_reader.h"

#include "rc522.h"


typedef char* (*rfid_impl_init)(spi_device_handle_t spi);

typedef struct rfid_impl_t {
  rfid_impl_init init;
} rfid_impl_t;

rfid_impl_t rfid;


void
rfid_implement(void)
{
#if defined(RC522)
  rfid.init = rc522_init;
#elif defined(PN532)
#endif
}

void
rfid_init(spi_device_handle_t spi)
{
  rfid.init(spi);
}
