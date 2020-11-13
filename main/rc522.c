#include "rc522.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "esp_timer.h"


static spi_device_handle_t* rc522_spi = NULL;
static esp_timer_handle_t rc522_timer;
static bool rc522_timer_running = false;


typedef struct picc_t {
  uint8_t uid_hot;
  uint8_t uid_full;
  uint8_t uid[10];
  uint8_t uid_bits;
// Types:
// case 0x04:	return PICC_TYPE_NOT_COMPLETE;
// case 0x09:	return PICC_TYPE_MIFARE_MINI;
// case 0x08:	return PICC_TYPE_MIFARE_1K;
// case 0x18:	return PICC_TYPE_MIFARE_4K;
// case 0x00:	return PICC_TYPE_MIFARE_UL;
// case 0x10:
// case 0x11:	return PICC_TYPE_MIFARE_PLUS;
// case 0x01:	return PICC_TYPE_TNP3XXX;
// case 0x20:	return PICC_TYPE_ISO_14443_4;
// case 0x40:	return PICC_TYPE_ISO_18092;
// default:	return PICC_TYPE_UNKNOWN;
  uint8_t type;
} picc_t;

picc_t picc;

esp_err_t rc522_init(spi_device_handle_t* spi)
{
  rc522_spi = spi;

  // ---------- RW test ------------
  rc522_write(RC522_REG_MOD_WIDTH, 0x25);
  assert(rc522_read(RC522_REG_MOD_WIDTH) == 0x25);

  rc522_write(RC522_REG_MOD_WIDTH, 0x26);
  assert(rc522_read(RC522_REG_MOD_WIDTH) == 0x26);
  // ------- End of RW test --------

  rc522_write(RC522_REG_COMMAND, 0x0F);
  rc522_write(RC522_REG_TIMER_MODE, 0x8D);
  rc522_write(RC522_REG_TIMER_PRESCALER, 0x3E);
  rc522_write(RC522_REG_TIMER_RELOAD_2, 0x1E);
  rc522_write(RC522_REG_TIMER_RELOAD_1, 0x00);
  rc522_write(RC522_REG_TX_ASK, 0x40);
  rc522_write(RC522_REG_MODE, 0x3D);

  rc522_antenna_on();

  printf("RC522 firmware 0x%x\n", rc522_fw_version());

  return ESP_OK;
}

esp_err_t rc522_write_n(uint8_t addr, uint8_t data_size, uint8_t *data)
{
  // The address gets concatenated with the data here. This buffer gets freed in the same scope
  // so a little malloc won't hurt... famous last words.
  uint8_t* buffer = (uint8_t*) malloc(data_size + 1);
  // TODO(michalc): understand why this is needed.
  buffer[0] = (addr << 1) & 0x7E;

  // Copy data from data to buffer, moved by one byte to make space for the address.
  for (uint8_t i = 1; i <= data_size; i++)
  {
    buffer[i] = data[i - 1];
  }

  spi_transaction_t t = {};

  // Yes, the length is in bits.
  t.length = 8 * (data_size + 1);
  t.tx_buffer = buffer;

  esp_err_t ret = spi_device_transmit(*rc522_spi, &t);

  free(buffer);

  return ret;
}

esp_err_t rc522_write(uint8_t addr, uint8_t val)
{
  return rc522_write_n(addr, 1, &val);
}

/* Returns pointer to dynamically allocated array of N places. */
uint8_t* rc522_read_n(uint8_t addr, uint8_t n)
{
  if (n <= 0) return NULL;

  spi_transaction_t t;
  memset(&t, 0, sizeof(t));

  uint8_t* buffer = (uint8_t*) malloc(n);
  
  t.flags = SPI_TRANS_USE_TXDATA;
  t.length = 8;
  t.tx_data[0] = ((addr << 1) & 0x7E) | 0x80;
  t.rxlength = 8 * n;
  t.rx_buffer = buffer;

  esp_err_t ret = spi_device_transmit(*rc522_spi, &t);
  assert(ret == ESP_OK);

  return buffer;
}

uint8_t rc522_read(uint8_t addr)
{
  uint8_t* buffer = rc522_read_n(addr, 1);
  uint8_t res = buffer[0];
  free(buffer);

  return res;
}


// 
// SPECIFIC FUNCTIONALITY
// 

esp_err_t rc522_set_bitmask(uint8_t addr, uint8_t mask)
{
  return rc522_write(addr, rc522_read(addr) | mask);
}

esp_err_t rc522_clear_bitmask(uint8_t addr, uint8_t mask)
{
  return rc522_write(addr, rc522_read(addr) & ~mask);
}

esp_err_t rc522_antenna_on()
{
  esp_err_t ret;

  if(~(rc522_read(RC522_REG_TX_CONTROL_REG) & 0x03))
  {
    ret = rc522_set_bitmask(RC522_REG_TX_CONTROL_REG, 0x03);

    if(ret != ESP_OK)
    {
      return ret;
    }
  }

  return rc522_write(RC522_REG_RF_CFG, RC522_RF_GAIN_33dB);
}

/* Returns pointer to dynamically allocated array of two element */
uint8_t* rc522_calculate_crc(uint8_t *data, uint8_t data_size, uint8_t* crc_buf)
{
  // 0x04 = CalcCRC command is active and all data is processed.
  rc522_clear_bitmask(RC522_REG_DIV_IRQ, 0x04);
  // 0x08 = flush FIFO buffer.
  rc522_set_bitmask(RC522_REG_FIFO_LEVEL, 0x80);

  rc522_write_n(RC522_REG_FIFO_DATA, data_size, data);

  rc522_write(RC522_REG_COMMAND, RC522_CMD_CALC_CRC);

  uint8_t i = 255;
  uint8_t nn = 0;

  // Wait for the CRC computation to be done.
  while (1)
  {
    nn = rc522_read(RC522_REG_DIV_IRQ);
    i--;

    if (!i)
    {
      break;
    }

    // 0x04 = CalcCRC command is active and all data is processed.
    if(nn & 0x04)
    {
      break;
    }
  }

  crc_buf[0] = rc522_read(RC522_REG_CRC_RESULT_2);
  crc_buf[1] = rc522_read(RC522_REG_CRC_RESULT_1);

  return crc_buf;
}

void rc522_picc_write(rc522_commands_e cmd,
                          uint8_t* data, uint8_t data_size,
                          response_t* response)
{
  uint8_t irq = 0x00;
  uint8_t irq_wait = 0x00;
  uint8_t last_bits = 0;
  uint8_t nn = 0;
  
  if (cmd == RC522_CMD_MF_AUTH)
  {
    irq = 0x12; // 0b10010 - error and idle irq
    irq_wait = 0x10; // IdleIRq
  }
  else if (cmd == RC522_CMD_TRANSCEIVE)
  {
    irq = 0x77; // 0b1110111
    irq_wait = 0x30; // RxIRq | IdleIRq
  }

  // Enable passing the interrupt requests. 0x80 turns on the HiAlert which
  // triggers when the FIFO is almost full (WaterLevel is the limit).
  // The WaterLevel isn't set anywhere here.
  rc522_write(RC522_REG_COM_IRQ_EN_DI, irq | 0x80);
  // Clear the HiAlert.
  rc522_clear_bitmask(RC522_REG_COM_IRQ, 0x80);
  // 0x80 = flush the FIFO buffer.
  rc522_set_bitmask(RC522_REG_FIFO_LEVEL, 0x80);
  // Change to IDLE mode to cancel any command.
  rc522_write(RC522_REG_COMMAND, RC522_CMD_IDLE);
  // Write the data into FIFO buffer.
  rc522_write_n(RC522_REG_FIFO_DATA, data_size, data);

  rc522_write(RC522_REG_COMMAND, cmd);

  if(cmd == RC522_CMD_TRANSCEIVE)
  {
    // All bits of the last byte will be transmitted.
    rc522_set_bitmask(RC522_REG_BIT_FRAMING, 0x80);
  }

  uint16_t i = 1000;

  while (1)
  {
    nn = rc522_read(RC522_REG_COM_IRQ);
    i--;

    if (!i) break;

    // Check for possible interrupts.
    if (nn & irq_wait)
    {
      break;
    }

    // This is a timeout.
    if (nn & 0x01)
    {
      break;
    }
  }

  // Stop the transmission to PICC.
  rc522_clear_bitmask(RC522_REG_BIT_FRAMING, 0x80);

  if (i != 0)
  {
    // Check for 0b11011 error bits.
    if((rc522_read(RC522_REG_ERROR) & 0x1B) == 0x00)
    {
      // The RC522_CMD_MF_AUTH doesn't get a response.
      if(cmd == RC522_CMD_TRANSCEIVE)
      {
        // Check how many bytes are in the FIFO buffer. The last one might be an incomplete byte.
        nn = rc522_read(RC522_REG_FIFO_LEVEL);
        // Returns the number of valid bits in the last received byte. The response might have been
        // smaller than 1 byte.
        last_bits = rc522_read(RC522_REG_CONTROL) & 0x07;
        response->size_bytes = nn;
        if (last_bits == 0)
        {
          response->size_bits = response->size_bytes * 8U;
        }
        else
        {
          response->size_bits = (nn * 8U) - (8 - last_bits);
        }

        if (response->size_bytes)
        {
          response->data = (uint8_t*)malloc(response->size_bytes);

          // Fetch the response.
          for(i = 0; i < response->size_bytes; i++)
          {
            response->data[i] = rc522_read(RC522_REG_FIFO_DATA);
          }
        }
      }
    }
  }

  return;
}

uint8_t rc522_picc_reqa_or_wupa(uint8_t reqa_or_wupa)
{
  uint8_t status = 0;
  // Set a short frame format of 7 bits. That means that only 7 bits of the last byte,
  // in this case the only byte will be transmitted to the PICC.
  rc522_write(RC522_REG_BIT_FRAMING, 0x07);

  uint8_t picc_cmd_buffer[] = {reqa_or_wupa};
  response_t resp = {};
  // Result is so called ATQA. If we get an ATQA this means there is one or more PICC present.
  // ATQA is exactly 2 bytes.
  rc522_picc_write(RC522_CMD_TRANSCEIVE, picc_cmd_buffer, 1, &resp);

  if (resp.data != NULL)
  {
    free(resp.data);
    resp.size_bytes = 0;
    resp.size_bits = 0;
  }

  if(resp.size_bytes == 2 && resp.size_bits == 16)
  {
    // A PICC has responded to REQA.
    status = 1;
  }

  return status;
}

// TODO(michalc): return value should reflect success which depends on the cascade_level.
status_e rc522_anti_collision(uint8_t cascade_level)
{
  assert(cascade_level > 0);
  assert(cascade_level <= 3);

  response_t resp = {};

  // 1. Build an a ANTI COLLISION command.

  // Anti collision commands always have [1] == 0x20.
  // Set NVB. First anti collision round sets the NVB to 0x20.
  // After that NVB should be increased by UID bits count that were already
  // fetched. Those become a search criteria for fetching the rest of the UID.
  // After all the anticollision steps the select step needs to be performed.
  uint8_t anticollision[9];
  uint8_t anticollision_size = 2;
  anticollision[1] = 0x20;

  if (cascade_level == 1)
  {
    anticollision[0] = PICC_CMD_SELECT_CL_1;
  }
  else if (cascade_level == 2)
  {
    // TODO(michalc): for now I just assume that we've received full bytes of UID. No trailing bits.
    anticollision[0] = PICC_CMD_SELECT_CL_2;
  }
  else if (cascade_level == 3)
  {
    anticollision[0] = PICC_CMD_SELECT_CL_3;
  }
  else
  {
    // TODO(michalc): should never happen.
  }

  // 2. Send the ANTI COLLISION command.

  // Sets StartSend bit to 0, all bits are valid.
  rc522_write(RC522_REG_BIT_FRAMING, 0x00);

  rc522_picc_write(RC522_CMD_TRANSCEIVE, anticollision, anticollision_size, &resp);

  if (resp.data == NULL)
  {
    return FAILURE;
  }

  // 3. Handle the ANTI COLLISION command response.

  // Check the BCC byte (XORed UID bytes).
  if (resp.size_bytes != 5)
  {
    // TODO(michalc): error out
  }
  if (resp.data[4] != (resp.data[0] ^ resp.data[1] ^ resp.data[2] ^ resp.data[3]))
  {
    // TODO(michalc): handle incorrect BCC byte value.
  }

  // 4. Extract the UID bytes.

  // Check the condition for having full UID.
  if (resp.data[0] != PICC_CASCADE_TAG)
  {
    // Here we skip copying of the BCC byte.
    picc.uid_full = true;
    memcpy(picc.uid, resp.data, resp.size_bytes - 1);
    picc.uid_bits = 4 * 8;
    picc.uid_hot = 1;
  }
  else
  {
    // Here we skip copying both CT byte and the BCC byte.
    picc.uid_full = false;
    memcpy(picc.uid, resp.data + 1, resp.size_bytes - 2);
    picc.uid_bits = 3 * 8;
    picc.uid_hot = 1;
  }

  // 5. Send a SELECT command.

  if ((cascade_level == 1) || (cascade_level == 2) || (cascade_level == 3))
  {
    anticollision[1] = 0x20 + 0x50;
    memcpy(&anticollision[2], resp.data, 5);
    // BCC and the CRC is transmitted only if we know all the UID bits of the current cascade level.
    anticollision[6] = anticollision[2] ^ anticollision[3] ^ anticollision[4] ^ anticollision[5];
    anticollision_size = 7;
    (void)rc522_calculate_crc(anticollision, 7, &anticollision[7]);
    anticollision_size = 9;

    free(resp.data);


    // Sets StartSend bit to 0, all bits are valid.
    rc522_write(RC522_REG_BIT_FRAMING, 0x00);

    rc522_picc_write(RC522_CMD_TRANSCEIVE, anticollision, anticollision_size, &resp);

    // Need to verify 1 byte (actually 24 bits) SAK response. Check for size and cascade bit.
    if (resp.data != NULL)
    {
      if (!(resp.data[0] & 0x04))
      {
        picc.uid_full = true;
        picc.type = resp.data[0] & 0x7F;
      }

      free(resp.data);
    }
  }
  else
  {
    // TODO(michalc): should never happen.
  }

  status_e status = FAILURE;
  if (picc.uid_full == true)
  {
    status = SUCCESS;
  }
  else
  {
    status = rc522_anti_collision(cascade_level + 1);
  }

  return status;
}

// TODO(michalc): this could be redesigned to be a recursive function to fetch the complete UID.
uint8_t* rc522_get_picc_id()
{
  // An example sequence of establishing the full UID.
  //
  // write len=1, data= 26                   => Welcome (REQA)
  // read: len=2 val= 44 00: OK              => Respond (ATQA)
  //
  // write len=2, data= 93 20                => Select cascade 1 (SEL)
  // read: len=5 val= 88 04 f2 52 2c: OK     => CT, UID, BCC
  // write len=7, data= 93 70 88 04 f2 52 2c => Select available tag (SEL)
  // read: len=1 val= 04: OK                 => Select Acknowledge (SAK)
  //
  // write len=2, data= 95 20                => Select cascade 2 (SEL)
  // read: len=5 val= b1 ec 02 80 df: OK     => UID, BCC
  // write len=7, data= 95 70 b1 ec 02 80 df => Finish select (SEL)
  // read: len=1 val= 00: OK                 => SAK without cascade bit set
  //
  // Layer 2 success (ISO 14443-3 A)         => UID = 04 f2 52 b1 ec 02 80
  // CT  => Cascade tag byte (88), signals that the UID is not complete yet
  // BCC => Checkbyte, calculated as exclusive-or over 4 previous bytes

  response_t resp = {};

  // If you use REQA you wan't be able to wake the PICC up after doing HALTA at the end of this
  // function. You'll have to move the PICC away from the reader to reset it to IDLE and then
  // move it close to reader again.
  // If you use WUPA you'll be able to wake up the PICC every time. That means the entire process
  // below will succeed every time.
  uint8_t picc_present = rc522_picc_reqa_or_wupa(PICC_CMD_WUPA);
  static uint8_t sector = 0;

  if (picc_present)
  {
    // The rc522_anti_collision is a recursive function. It stores the full UID in the global
    // picc variable.
    status_e status = rc522_anti_collision(1);

    if (status == SUCCESS)
    {
      uint8_t key[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
      // Authenticate sector access.
      rc522_authenticate(PICC_CMD_MF_AUTH_KEY_A, 0x2, key);

      uint8_t data[18] = {0x4, 0x3, 0x2, 0x1};
      // if (temp++ % 2)
      // {
      //   printf("WRITING!\n");
      //   rc522_write_picc_data(0x0, data);
      // }
      // else
      {
        const uint8_t block = 0x3 + 0x4 * sector;
        printf("READING %d\n", block);
        rc522_read_picc_data(block, data);
        if (++sector > 15) sector = 0;
        printf("DATA: %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
          data[0],
          data[1],
          data[2],
          data[3],
          data[4],
          data[5],
          data[6],
          data[7],
          data[8],
          data[9],
          data[10],
          data[11],
          data[12],
          data[13],
          data[14],
          data[15]
        );
      }
    }
    else
    {
      memset((void*)&picc, 0, sizeof(picc));
      printf("FAILED TO GET A FULL UID...\n");
    }


    // Halting and clearing the MFCrypto1On bit should be done after readings data.
    // After halting it needs to be WUPA (waken up).
    uint8_t picc_cmd_buffer[] = {PICC_CMD_HALTA, 0x00, 0x00, 0x00 };
    (void)rc522_calculate_crc(picc_cmd_buffer, 2, &picc_cmd_buffer[2]);

    rc522_picc_write(RC522_CMD_TRANSCEIVE, picc_cmd_buffer, 4, &resp);
    // Check if cascade bit is set. If so then the UID isn't fully fetched yet.
    if (resp.data != NULL)
    {
      free(resp.data);
    }

    // Clear the MFCrypto1On bit.
    rc522_clear_bitmask(RC522_REG_STATUS_2, 0x08);
  }

  return NULL;
}

void rc522_read_picc_data(uint8_t block_address, uint8_t buffer[16])
{
  response_t resp = {};

  // At least 18 bytes because it's data + CRC_A.
  uint8_t picc_cmd_buffer[18];
  picc_cmd_buffer[0] = PICC_CMD_MF_READ;
  picc_cmd_buffer[1] = block_address;
  // Calculate the CRC on the RC522 side.
  (void)rc522_calculate_crc(picc_cmd_buffer, 2, &picc_cmd_buffer[2]);

  // Send the command to PICC.
  rc522_picc_write(RC522_CMD_TRANSCEIVE, picc_cmd_buffer, 4, &resp);

  if (resp.data != NULL)
  {
    if (resp.size_bits == 4)
    {
      if (resp.data[0] != MF_ACK)
      {
        printf("PICC responded with NAK (%x) when trying to read data!\n", resp.data[0]);
        return;
      }
    }

    // If it's not NAK then it's most probably the actual data.
    if (resp.size_bytes == 18)
    {
      // Copy the actual data bytes but not the CRC bytes.
      memcpy(buffer, resp.data, resp.size_bytes - 2);
    }
    free(resp.data);
  }

  return;
}

void rc522_write_picc_data(uint8_t block_address, uint8_t buffer[18])
{
  response_t resp = {};

  uint8_t picc_cmd_buffer[4];
  picc_cmd_buffer[0] = PICC_CMD_MF_WRITE;
  picc_cmd_buffer[1] = block_address;  // block address.
  // Calculate the CRC on the RC522 side.
  (void)rc522_calculate_crc(picc_cmd_buffer, 2, &picc_cmd_buffer[2]);

  // Send the command to PICC.
  rc522_picc_write(RC522_CMD_TRANSCEIVE, picc_cmd_buffer, 4, &resp);

  // Here you can get a response of 12 bits and I think that's a 'fuck off' response.
  printf("--------   %d\n", resp.size_bytes);

  if (resp.data != NULL)
  {
    if (resp.size_bits == 4)
    {
      if (resp.data[0] != MF_ACK)
      {
        printf("PICC responded with NAK (%x) when trying to write data!\n", resp.data[0]);
        return;
      }
    }

    free(resp.data);
  }

  // We always write 16 bytes. No other way to do a write.
  (void)rc522_calculate_crc(buffer, 16, &picc_cmd_buffer[16]);
  rc522_picc_write(RC522_CMD_TRANSCEIVE, buffer, 16, &resp);

  printf("--------   %d\n", resp.size_bits);

  if (resp.data != NULL)
  {
    if (resp.size_bits == 4)
    {
      if (resp.data[0] != MF_ACK)
      {
        printf("PICC responded with NAK (%x) when trying to write data!\n", resp.data[0]);
        return;
      }
    }

    free(resp.data);
  }

  return;
}

static void rc522_timer_callback(void* arg)
{
  rc522_get_picc_id();

  rc522_tag_callback_t cb = (rc522_tag_callback_t) arg;
  cb(picc.uid);
}

void tag_handler(uint8_t* serial_no)
{
  if (picc.uid_hot)
  {
    printf("type: 0x%x, %s : ", picc.type, picc.uid_full ? "full" : "not full");
    for (int i = 0; i < 10; i++)
    {
      printf("%x ", serial_no[i]);
    }
    printf("\n");
    picc.uid_hot = false;
  }
}

void rc522_authenticate(uint8_t cmd_auth_key_a_or_b,
                        uint8_t block_address,
                        uint8_t key[MF_KEY_SIZE])
{
  response_t resp = {};

  uint8_t picc_cmd_buffer[12];
  picc_cmd_buffer[0] = cmd_auth_key_a_or_b;
  picc_cmd_buffer[1] = block_address;

  memcpy((void*)(picc_cmd_buffer + 2), key, MF_KEY_SIZE);
  // Use the last uid uint8_ts as specified in http://cache.nxp.com/documents/application_note/AN10927.pdf
  // section 3.2.5 "MIFARE Classic Authentication".
  // The only missed case is the MF1Sxxxx shortcut activation,
  // but it requires cascade tag (CT) uint8_t, that is not part of uid.
  for (uint8_t i = 0; i < 4; i++)
  {
    // Use the last 4 bytes.
    picc_cmd_buffer[8 + i] = *((picc.uid + (picc.uid_bits / 8) - 4) + i);
  }

  rc522_picc_write(RC522_CMD_MF_AUTH, picc_cmd_buffer, 12, &resp);
  // Authentication gets no response.
  assert(resp.data == NULL);
}

esp_err_t rc522_start()
{
  // rc522_init();

  const esp_timer_create_args_t timer_args = {
    .callback = &rc522_timer_callback,
    .arg = (void*)tag_handler,
    .name = "timer_picc_reqa",
  };

  esp_err_t ret = esp_timer_create(&timer_args, &rc522_timer);

  if(ret != ESP_OK) {
    return ret;
  }

  return rc522_resume();
}

esp_err_t rc522_resume()
{
  // 125000 microseconds means 8Hz.
  return rc522_timer_running ? ESP_OK : esp_timer_start_periodic(rc522_timer, 125000);
}

esp_err_t rc522_pause()
{
  return ! rc522_timer_running ? ESP_OK : esp_timer_stop(rc522_timer);
}
