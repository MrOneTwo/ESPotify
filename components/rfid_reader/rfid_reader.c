#include "rfid_reader.h"

#include "rc522.h"
#include "pn532.h"


typedef esp_err_t (*rfid_impl_init)(spi_device_handle_t spi);
typedef bool (*rfid_impl_say_hello)(void);
typedef bool (*rfid_impl_test_picc_presence)(void);
typedef bool (*rfid_impl_anti_collision)(uint8_t cascade_level);

typedef struct rfid_impl_t {
  rfid_impl_init init;
  rfid_impl_say_hello say_hello;
  rfid_impl_test_picc_presence test_picc_presence;
  rfid_impl_anti_collision anti_collision;
} rfid_impl_t;

static rfid_impl_t rfid;


void
rfid_implement(void)
{
#if defined (CONFIG_RC522)
  rfid.init = rc522_init;
  rfid.say_hello = rc522_say_hello;
  rfid.test_picc_presence = rc522_test_picc_presence;
  rfid.anti_collision = rc522_anti_collision;
#elif defined (CONFIG_PN532)
  rfid.init = pn532_init;
  rfid.say_hello = pn532_say_hello;
  rfid.test_picc_presence = pn532_test_picc_presence;
  rfid.anti_collision = pn532_anti_collision;
#endif
}

esp_err_t
rfid_init(spi_device_handle_t spi)
{
  return rfid.init(spi);
}

bool
rfid_say_hello()
{
  return rfid.say_hello();
}

bool
rfid_test_picc_presence(void)
{
  return rfid.test_picc_presence();
}

bool
rfid_anti_collision(uint8_t cascade_level)
{
  return rfid.anti_collision(cascade_level);
}
