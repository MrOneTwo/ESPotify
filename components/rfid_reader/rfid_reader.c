#include "rfid_reader.h"

#include "rc522.h"


typedef esp_err_t (*rfid_impl_init)(spi_device_handle_t spi);
typedef bool (*rfid_impl_test_picc_presence)(void);

typedef struct rfid_impl_t {
  rfid_impl_init init;
  rfid_impl_test_picc_presence test_picc_presence;
} rfid_impl_t;

static rfid_impl_t rfid;

#define RC522

void
rfid_implement(void)
{
#if defined(RC522)
  rfid.init = rc522_init;
  rfid.test_picc_presence = rc522_test_picc_presence;
#elif defined(PN532)
  rfid.init = pn532_init;
  rfid.test_picc_presence = pn532_test_picc_presence;
#endif
}

void
rfid_init(spi_device_handle_t spi)
{
  rfid.init(spi);
}

bool
rfid_test_picc_presence(void)
{
  return rfid.test_picc_presence();
}
