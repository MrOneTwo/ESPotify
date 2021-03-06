#include "rc522.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "soc/gpio_struct.h"

static char* TAG = "rc522";

static spi_device_handle_t rc522_spi;
static esp_timer_handle_t rc522_timer;

// Creating 3 slots.
#define SCRATCH_MEM_SIZE  (48 * 3)
static uint8_t* scratch_mem = NULL;

static picc_t picc;

//
// Functions that communicate with RC522.
//

/*
 * Calculate CRC for data on the RC522 side.
 */
static void rc522_calculate_crc(uint8_t *data, uint8_t data_size, uint8_t* crc_buf);

static esp_err_t rc522_antenna_on();
static esp_err_t rc522_set_bitmask(uint8_t addr, uint8_t mask);

typedef void(*rc522_tag_callback_t)(uint8_t*);


/*
 * Return picc by copy since the caller shouldn't be able to modify the internal structure.
 */
picc_t
rc522_get_last_picc(void)
{
  return picc;
}

esp_err_t
rc522_init(spi_device_handle_t spi)
{
  if (scratch_mem == NULL)
  {
    scratch_mem = (uint8_t*)malloc(SCRATCH_MEM_SIZE);
  }

  if (spi != NULL)
  {
    rc522_spi = spi;
    return ESP_OK;
  }
  return ESP_FAIL;
}

bool
rc522_say_hello()
{
  bool ret = true;

  // RW test
  rc522_write(RC522_REG_MOD_WIDTH, 0x25);
  if (rc522_read(RC522_REG_MOD_WIDTH) != 0x25)
  {
    ret = false;
  }

  rc522_write(RC522_REG_MOD_WIDTH, 0x26);
  if (rc522_read(RC522_REG_MOD_WIDTH) != 0x26)
  {
    ret = false;
  }
  // End of RW test

  rc522_write(RC522_REG_COMMAND, 0x0F);
  // 0x0D part is high part of the timer prescaler.
  rc522_write(RC522_REG_TIMER_MODE, 0x8D);
  // Timer is used for timing out when talking to PICC.
  // 0x3E part is the low part of the timer prescaler.
  // Using the 0x0D3E for prescaler value gives timer frequency of ~2kHz
  // Using 0x0D3D would give exactly 2kHz...
  rc522_write(RC522_REG_TIMER_PRESCALER, 0x3E);
  // 0x1E00 is 7680 in dec - that's the 16 bit reload value.
  rc522_write(RC522_REG_TIMER_RELOAD_2, 0x1E);
  rc522_write(RC522_REG_TIMER_RELOAD_1, 0x00);
  // Transmission modulation. There is only one bit to set in this register. That's the
  // Force100ASK bit. ASK = Amplitude Shift Keying.
  // TODO(michalc): I don't know why we do this.
  rc522_write(RC522_REG_TX_ASK, 0x40);
  // Default 0x3F. Set the preset value for the CRC coprocessor for the CalcCRC command to 0x6363 (ISO 14443-3 part 6.2.4)
  rc522_write(RC522_REG_MODE, 0x3D);

  rc522_antenna_on();

  ESP_LOGW(TAG, "RC522 firmware 0x%x", rc522_fw_version());

  return ret;
}

esp_err_t
rc522_write_n(uint8_t addr, uint8_t data_size, const uint8_t* const data)
{
  // The address gets concatenated with the data here.
  uint8_t* buffer = scratch_mem;
  // MFRC522 documentation says that bit 0 should be 0, bits 1-6 is the address, bit 7
  // is 1 for read and 0 for write.
  buffer[0] = (addr << 1) & 0x7E;

  // Copy data from data to buffer, moved by one byte to make space for the address.
  memcpy(buffer + 1, data, data_size);

  spi_transaction_t t = {};

  // Yes, the length is in bits.
  t.length = 8 * (data_size + 1);
  t.tx_buffer = buffer;

  esp_err_t ret = spi_device_transmit(rc522_spi, &t);

  return ret;
}

esp_err_t rc522_write(uint8_t addr, uint8_t val)
{
  return rc522_write_n(addr, 1, &val);
}

/*
 * Returns pointer to dynamically allocated array of N places.
 */
uint8_t* rc522_read_n(uint8_t addr, uint8_t n)
{
  if (n <= 0) return NULL;

  spi_transaction_t t;
  memset(&t, 0, sizeof(t));

  // Using the second half of the scratch_mem for incoming data.
  uint8_t* buffer = (scratch_mem + SCRATCH_MEM_SIZE / 3);
  
  t.flags = SPI_TRANS_USE_TXDATA;
  t.length = 8;
  // MFRC522 documentation says that bit 0 should be 0, bits 1-6 is the address, bit 7
  // is 1 for read and 0 for write.
  t.tx_data[0] = ((addr << 1) & 0x7E) | 0x80;
  t.rxlength = 8 * n;
  t.rx_buffer = buffer;

  esp_err_t ret = spi_device_transmit(rc522_spi, &t);
  assert(ret == ESP_OK);

  return buffer;
}

uint8_t rc522_read(uint8_t addr)
{
  uint8_t* buffer = rc522_read_n(addr, 1);
  return buffer[0];
}


// 
// SPECIFIC FUNCTIONALITY
// 

static esp_err_t rc522_set_bitmask(uint8_t addr, uint8_t mask)
{
  return rc522_write(addr, rc522_read(addr) | mask);
}

esp_err_t rc522_clear_bitmask(uint8_t addr, uint8_t mask)
{
  return rc522_write(addr, rc522_read(addr) & ~mask);
}

static esp_err_t rc522_antenna_on()
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
static void rc522_calculate_crc(uint8_t *data, uint8_t data_size, uint8_t* crc_buf)
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

    if (i == 0)
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
}

void rc522_picc_write(rc522_commands_e cmd,
                      const uint8_t* const data, const uint8_t data_size,
                      response_t* const response)
{
  uint8_t irq = 0;
  uint8_t irq_wait = 0;
  
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
    // SEND THE DATA!!!
    rc522_set_bitmask(RC522_REG_BIT_FRAMING, 0x80);
  }

  uint16_t dont_lock = 1000;

  while (1)
  {
    uint8_t nn = rc522_read(RC522_REG_COM_IRQ);

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

    if (--dont_lock == 0) break;
  }

  // Stop the transmission to PICC.
  rc522_clear_bitmask(RC522_REG_BIT_FRAMING, 0x80);

  if (dont_lock != 0)
  {
    // Check for 0b11011 error bits.
    if((rc522_read(RC522_REG_ERROR) & 0x1B) == 0x00)
    {
      // The RC522_CMD_MF_AUTH doesn't get a response.
      if(cmd == RC522_CMD_TRANSCEIVE)
      {
        // Check how many bytes are in the FIFO buffer. The last one might be an incomplete byte.
        response->size_bytes = rc522_read(RC522_REG_FIFO_LEVEL);
        // Returns the number of valid bits in the last received byte. The response might have been
        // smaller than 1 byte.
        const uint8_t last_bits = rc522_read(RC522_REG_CONTROL) & 0x07;
        if (last_bits == 0)
        {
          response->size_bits = response->size_bytes * 8U;
        }
        else
        {
          response->size_bits = (response->size_bytes * 8U) - (8 - last_bits);
        }

        if (response->size_bytes)
        {
          // Using the second half of the scratch_mem for incoming data.
          response->data = (scratch_mem + 2 * (SCRATCH_MEM_SIZE / 3));
          // Read the data into scratch memory.
          for(uint32_t j = 0; j < response->size_bytes; j++)
          {
            response->data[j] = rc522_read(RC522_REG_FIFO_DATA);
          }
        }
        else
        {
          response->data = NULL;
        }
      }
    }
  }

  return;
}

status_e rc522_picc_reqa_or_wupa(uint8_t reqa_or_wupa)
{
  status_e status = FAILURE;
  // Set a short frame format of 7 bits. That means that only 7 bits of the last byte,
  // in this case the only byte will be transmitted to the PICC.
  rc522_write(RC522_REG_BIT_FRAMING, 0x07);

  uint8_t picc_cmd_buffer[] = {reqa_or_wupa};
  response_t resp = {};
  // Result is so called ATQA. If we get an ATQA this means there is one or more PICC present.
  // ATQA is exactly 2 bytes.
  rc522_picc_write(RC522_CMD_TRANSCEIVE, picc_cmd_buffer, 1, &resp);

  if(resp.size_bytes == 2 && resp.size_bits == 16)
  {
    // A PICC has responded to REQA with ATQA.
    // ESP_LOGD(TAG, "ATQA: %02x %02x\n", resp.data[0], resp.data[1]);

    // TODO(michalc): According to this document https://www.nxp.com/docs/en/application-note/AN10833.pdf
    // page 10, ATQA should never be used to identify the PICC. SAK should be used for that.
    uint8_t atqa_1 = resp.data[0];
    if (atqa_1 == 0x44)
    {
      picc.type = PICC_SUPPORTED_NTAG213;
    }
    else if (atqa_1 == 0x04)
    {
      picc.type = PICC_SUPPORTED_MIFARE_1K;
    }

    status = SUCCESS;
  }

  return status;
}

// TODO(michalc): this doesn't need an argument. It's always the same halta.
status_e rc522_picc_halta(uint8_t halta)
{
  response_t resp = {};

  // Halting and clearing the MFCrypto1On bit should be done after readings data.
  // After halting it needs to be WUPA (waken up).
  uint8_t picc_cmd_buffer[] = {halta, 0x00, 0x00, 0x00 };
  rc522_calculate_crc(picc_cmd_buffer, 2, &picc_cmd_buffer[2]);

  rc522_picc_write(RC522_CMD_TRANSCEIVE, picc_cmd_buffer, 4, &resp);

  return SUCCESS;
}

status_e rc522_picc_get_version(void)
{
  response_t resp = {};

  uint8_t picc_cmd_buffer[] = {PICC_CMD_NTAG_GET_VERSION, 0x00, 0x00};
  rc522_calculate_crc(picc_cmd_buffer, 1, &picc_cmd_buffer[1]);

  rc522_picc_write(RC522_CMD_TRANSCEIVE, picc_cmd_buffer, 3, &resp);

  // Check for NAK.
  if (resp.data == NULL)
  {
    return FAILURE;
  }

  if (resp.size_bytes == 10)
  {
    picc.ver.fixed_header = resp.data[0];
    picc.ver.vendor_id = resp.data[1];
    picc.ver.product_type = resp.data[2];
    picc.ver.product_subtype = resp.data[3];
    picc.ver.maj_product_ver = resp.data[4];
    picc.ver.min_product_ver = resp.data[5];
    picc.ver.storage_size = resp.data[6];
    picc.ver.protocol_type = resp.data[7];

    if (picc.ver.storage_size == 0x0F)
    {
      picc.type = PICC_SUPPORTED_NTAG213;
    }
  }

  return SUCCESS;
}

// TODO(michalc): return value should reflect success which depends on the cascade_level.
bool
rc522_anti_collision(uint8_t cascade_level)
{
  assert(cascade_level > 0);
  assert(cascade_level <= 3);

  ESP_LOGD(TAG, "Anticollision with cascade level %d\n", cascade_level);

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
    return false;
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
    // TODO(michalc): fix this - it should add more bits, not set.
    picc.uid_bits = 4 * 8;
    picc.uid_hot = 1;
  }
  else
  {
    // Here we skip copying both CT byte and the BCC byte.
    picc.uid_full = false;
    memcpy(picc.uid, resp.data + 1, resp.size_bytes - 2);
    // TODO(michalc): fix this - it should add more bits, not set.
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
    rc522_calculate_crc(anticollision, 7, &anticollision[7]);
    anticollision_size = 9;

    // Sets StartSend bit to 0, all bits are valid.
    rc522_write(RC522_REG_BIT_FRAMING, 0x00);

    rc522_picc_write(RC522_CMD_TRANSCEIVE, anticollision, anticollision_size, &resp);

    // Need to verify 1 byte (actually 24 bits) SAK response. Check for size and cascade bit.
    if (resp.data != NULL)
    {
      if (!(resp.data[0] & 0x04))
      {
        picc.uid_full = true;
        // TODO(michalc): here we should establish the type of PICC based on the contents of SAK.
        // picc.type = resp.data[0] & 0x7F;
      }
    }
  }
  else
  {
    // TODO(michalc): should never happen.
  }

  bool status = 0;
  if (picc.uid_full == true)
  {
    status = true;
  }
  else
  {
    status = rc522_anti_collision(cascade_level + 1);
  }

  return status;
}

bool rc522_test_picc_presence()
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


  // If you use REQA you wan't be able to wake the PICC up after doing HALTA at the end of this
  // function. You'll have to move the PICC away from the reader to reset it to IDLE and then
  // move it close to reader again.
  // If you use WUPA you'll be able to wake up the PICC every time. That means the entire process
  // below will succeed every time.
  status_e picc_present = rc522_picc_reqa_or_wupa(PICC_CMD_REQA);

  return picc_present == SUCCESS ? true : false;
}

status_e rc522_read_picc_data(uint8_t block_address, uint8_t buffer[16])
{
  response_t resp = {};

  // At least 18 bytes because it's data + CRC_A.
  uint8_t picc_cmd_buffer[18];
  picc_cmd_buffer[0] = PICC_CMD_MIFARE_READ;
  picc_cmd_buffer[1] = block_address;
  // Calculate the CRC on the RC522 side.
  rc522_calculate_crc(picc_cmd_buffer, 2, &picc_cmd_buffer[2]);

  // Send the command to PICC.
  rc522_picc_write(RC522_CMD_TRANSCEIVE, picc_cmd_buffer, 4, &resp);

  if (resp.data != NULL)
  {
    if (resp.size_bits == 4)
    {
      if (resp.data[0] != PICC_RESPONSE_ACK)
      {
        ESP_LOGD(TAG, "PICC responded with NAK (%x) when trying to read data!\n", resp.data[0]);
        return FAILURE;
      }
    }

    // If it's not NAK then it's most probably the actual data.
    if (resp.size_bytes == 18)
    {
      // Copy the actual data bytes but not the CRC bytes.
      memcpy(buffer, resp.data, resp.size_bytes - 2);
    }
  }
  else
  {
    ESP_LOGW(TAG, "No data returned when reading PICC.\n");
    return FAILURE;
  }

  return SUCCESS;
}

void rc522_write_picc_data(const uint8_t block_address, uint8_t* data, const uint32_t data_len)
{
  response_t resp = {};

  // The compatibility WRITE is supported by NTAG but we're using the native version here.
  // I had some problems with compatibility WRITE with data being corrupted when written.
  if (picc.type == PICC_SUPPORTED_NTAG213)
  {
    // TODO(michalc): More robust? Not writing entire blocks?
    assert(data_len % 4 == 0);
    const uint8_t write_operations = data_len / 4;
    uint8_t picc_write_buffer[8] = {};

    for (uint8_t i = 0; i < write_operations; i++)
    {
      memset(picc_write_buffer, 0, 8);
      picc_write_buffer[0] = PICC_CMD_NTAG_WRITE;
      picc_write_buffer[1] = block_address + i;
      memcpy(picc_write_buffer + 2, data + i * 4, 4);
      rc522_calculate_crc(picc_write_buffer, 6, &picc_write_buffer[6]);

      rc522_picc_write(RC522_CMD_TRANSCEIVE, picc_write_buffer, 8, &resp);

      if (resp.data != NULL)
      {
        if (resp.size_bits == 4)
        {
          if (resp.data[0] != PICC_RESPONSE_ACK)
          {
            ESP_LOGW(TAG, "PICC responded with NAK (%x) when trying to write data!\n", resp.data[0]);
            return;
          }
          else
          {
            ESP_LOGD(TAG, "PICC responded with ACK (%x) when trying to write data!\n", resp.data[0]);
          }
        }
      }
    }  // for write_operations
  }
  else if(picc.type == PICC_SUPPORTED_MIFARE_1K)
  {
    // TODO(michalc): More robust? Not writing entire blocks?
    assert(data_len % 16 == 0);
    const uint8_t write_operations = data_len / 16;
    uint8_t picc_write_buffer[18] = {};
    // This works for both MIFARE and NTAG since NTAG has the PICC_CMD_NTAG_COMP_WRITE.

    for (uint8_t i = 0; i < write_operations; i++)
    {
      memset(picc_write_buffer, 0, 18);
      picc_write_buffer[0] = PICC_CMD_MIFARE_WRITE;
      picc_write_buffer[1] = block_address + i;
      // Calculate the CRC on the RC522 side.
      rc522_calculate_crc(picc_write_buffer, 2, &picc_write_buffer[2]);

      // Send the command to PICC.
      rc522_picc_write(RC522_CMD_TRANSCEIVE, picc_write_buffer, 4, &resp);

      if (resp.data != NULL)
      {
        if (resp.size_bits == 4)
        {
          if (resp.data[0] != PICC_RESPONSE_ACK)
          {
            ESP_LOGW(TAG, "PICC responded with NAK (%x) when trying to write data!\n", resp.data[0]);
            return;
          }
          else
          {
            ESP_LOGD(TAG, "PICC responded with ACK (%x) when trying to write data!\n", resp.data[0]);
          }
        }
      }

      // We always write 16 data + 2 CRC bytes. No other way to do a write.
      memcpy(picc_write_buffer, data + 16 * i, 16);
      rc522_calculate_crc(picc_write_buffer, 16, &picc_write_buffer[16]);
      rc522_picc_write(RC522_CMD_TRANSCEIVE, picc_write_buffer, 18, &resp);

      if (resp.data != NULL)
      {
        if (resp.size_bits == 4)
        {
          if (resp.data[0] != PICC_RESPONSE_ACK)
          {
            ESP_LOGW(TAG, "PICC responded with NAK (%x) when trying to write data!\n", resp.data[0]);
            return;
          }
          else
          {
            ESP_LOGD(TAG, "PICC responded with ACK (%x) when trying to write data!\n", resp.data[0]);
          }
        }
      }
    }  // for write operations
  }
  else
  {
    ESP_LOGW(TAG, "Unsupported PICC for write operation!\n");
  }

  return;
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

void
rc522_authenticate(uint8_t cmd_auth_key_a_or_b,
                   uint8_t block_address,
                   const uint8_t key[MIFARE_KEY_SIZE])
{
  response_t resp = {};

  uint8_t picc_cmd_buffer[12];
  picc_cmd_buffer[0] = cmd_auth_key_a_or_b;
  picc_cmd_buffer[1] = block_address;

  memcpy((void*)(picc_cmd_buffer + 2), key, MIFARE_KEY_SIZE);
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

/*
 * TODO(michalc): this should be part of examples of this component.
 * This is an unused function which shows how to start scanning for the tags.
 * Currently the actual processing is in the tasks.c.
static void rc522_timer_callback(void* arg)
{
  status_e picc_present = rc522_test_picc_presence();

  const uint8_t sector = 2;
  static uint8_t block = 4 * sector - 4;

  if (picc_present == SUCCESS)
  {
    // The rc522_anti_collision is a recursive function. It stores the full UID in the global
    // picc variable.
    bool status = rc522_anti_collision(1);

    if (status)
    {
      uint8_t key[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
      // Authenticate sector access.
      rc522_authenticate(PICC_CMD_MIFARE_AUTH_KEY_A, block, key);

      uint8_t data[18] = {
        0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
        0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF
      };

      {
        printf("READING %d\n", block);
        rc522_read_picc_data(block, data);
        printf("BLOCK %d DATA: %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n", block,
          data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
          data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15]
        );
        block++;
        if (block > 7) block = 0;
      }
    }
    else
    {
      memset((void*)&picc, 0, sizeof(picc));
      printf("FAILED TO GET A FULL UID...\n");
    }

    rc522_picc_halta(PICC_CMD_HALTA);
    // Clear the MFCrypto1On bit.
    rc522_clear_bitmask(RC522_REG_STATUS_2, 0x08);
  }

  rc522_tag_callback_t cb = (rc522_tag_callback_t) arg;
  cb(picc.uid);
}

esp_err_t rc522_start_scanning()
{
  const esp_timer_create_args_t timer_args = {
    .callback = &rc522_timer_callback,
    .arg = (void*)tag_handler,
    .name = "timer_picc_reqa",
  };

  esp_err_t ret = esp_timer_create(&timer_args, &rc522_timer);

  if(ret != ESP_OK) {
    return ret;
  }

  // return rc522_resume();
  return ESP_OK;
}
*/
