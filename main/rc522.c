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
  uint8_t cascade_level;
} picc_t;

picc_t picc;

esp_err_t rc522_init(spi_device_handle_t* spi)
{
  rc522_spi = spi;
  picc.cascade_level = 1;

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

esp_err_t rc522_write_n(uint8_t addr, uint8_t n, uint8_t *data)
{
  // The address gets concatenated with the data here.
  uint8_t* buffer = (uint8_t*) malloc(n + 1);
  buffer[0] = (addr << 1) & 0x7E;

  // Copy data from data to buffer, moved by one byte to make space for the address.
  for (uint8_t i = 1; i <= n; i++)
  {
    buffer[i] = data[i-1];
  }

  spi_transaction_t t;
  memset(&t, 0, sizeof(t));

  t.length = 8 * (n + 1);
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

uint8_t* rc522_picc_write(uint8_t cmd,
                          uint8_t* data, uint8_t data_size,
                          uint8_t* response_size_bytes, uint32_t* response_size_bits)
{
  uint8_t *result = NULL;
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

    // I think this is a timeout.
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
      if(cmd == RC522_CMD_TRANSCEIVE)
      {
        // Check how many bytes are in the FIFO buffer. The last one might be an incomplete byte.
        nn = rc522_read(RC522_REG_FIFO_LEVEL);
        // Check if the entire, last byte read is valid. Returns the number of valid bits in the
        // last received byte.
        last_bits = rc522_read(RC522_REG_CONTROL) & 0x07;
        *response_size_bytes = nn;
        *response_size_bits = nn * 8U + last_bits;

        result = (uint8_t*)malloc(*response_size_bytes);

        for(i = 0; i < *response_size_bytes; i++)
        {
          result[i] = rc522_read(RC522_REG_FIFO_DATA);
        }
      }
    }
  }

  return result;
}

uint8_t rc522_picc_request(void)
{
  uint8_t status = 0;
  // Set a short frame format of 7 bits. That means that only 7 bits of the last byte,
  // in this case the only byte will be transmitted to the PICC.
  rc522_write(RC522_REG_BIT_FRAMING, 0x07);

  uint8_t picc_cmd_buffer[] = {PICC_CMD_REQA};
  uint32_t res_n_bits = 0;
  uint8_t res_n = 0;
  // Result is so called ATQA. If we get an ATQA this means there is one or more PICC present.
  // ATQA is exactly 2 bytes.
  uint8_t* response = rc522_picc_write(RC522_CMD_TRANSCEIVE, picc_cmd_buffer, 1, &res_n, &res_n_bits);

  if (response != NULL)
  {
    free(response);
  }

  if(res_n == 2 && res_n_bits == 16)
  {
    // A PICC has responded to REQA.
    status = 1;
  }

  return status;
}

// TODO(michalc): return value should reflect success which depends on the cascade_level.
uint8_t rc522_anti_collision(uint8_t cascade_level)
{
  uint8_t status = 0;
  uint8_t* result = NULL;
  uint8_t res_n;
  uint32_t res_n_bits = 0;
  // Set NVB. First anti collision round sets the NVB to 0x20.
  // After that NVB should be increased by UID bits count that were already
  // fetched. Those become a search criteria for fetching the rest of the UID.
  // After all the anticollision steps the select step needs to be performed.
  uint8_t anticollision[9];
  uint8_t anticollision_size = 2;
  if (cascade_level == 1)
  {
    anticollision[0] = PICC_CMD_SELECT_CL_1;
    // 2 bytes, 0 bits.
    anticollision[1] = 0x20;
    anticollision_size = 2;
  }
  else if (cascade_level == 2)
  {
    anticollision[0] = PICC_CMD_SELECT_CL_2;
    anticollision[1] = 0x20 + picc.uid_bits;;
    memcpy(&anticollision[2], picc.uid, 4);
    anticollision[6] = anticollision[2] ^ anticollision[3] ^ anticollision[4] ^ anticollision[5];
    anticollision_size = 7;
  }
  else if (cascade_level == 3)
  {
    anticollision[0] = PICC_CMD_SELECT_CL_3;
    anticollision_size = 10;
  }

  // Sets StartSend bit to 0.
  rc522_write(RC522_REG_BIT_FRAMING, 0x00);

  result = rc522_picc_write(RC522_CMD_TRANSCEIVE, anticollision, anticollision_size, &res_n, &res_n_bits);

  if (cascade_level == 1)
  {
    // Check the condition for having full UID.
    if (result[0] == PICC_CASCADE_TAG)
    {
      picc.uid_full = false;
    }
    else
    {
      picc.uid_full = true;
    }
    // Copy bytes UID bytes without the BCC byte.
    memcpy(picc.uid, result, res_n - 1);
    picc.uid_bits = res_n_bits;
    picc.uid_hot = 1;
    status = 2;

    // Check the BCC byte (XORed UID bytes).
    if (res_n == 5)
    {
      if(result[4] != (result[0] ^ result[1] ^ result[2] ^ result[3]))
      {
        // TODO(michalc): handle incorrect BCC byte value.
      }
    }
  }
  else if (cascade_level == 2)
  {
    printf("\n");
    for (uint8_t i = 0; i < res_n; i++)
    {
      printf("%x ", result[i]);
    }
    printf("\n");
    picc.uid_hot = 1;
    status = 3;
  }
  else if (cascade_level == 3)
  {
  }

  return status;
}

// TODO(michalc): this could be redesigned to be a recursive function to fetch the complete UID.
uint8_t* rc522_get_picc_id()
{
  uint8_t* result = NULL;
  uint8_t* response_data = NULL;
  uint8_t response_data_n;
  uint32_t response_data_n_bits;

  // TODO(michalc): I think this is incompatible with multiple anticollision steps. It probably
  // resets the state of PICC.
  uint8_t picc_present = rc522_picc_request();

  if (picc_present)
  {
    // This updates a global variable.
    picc.cascade_level = rc522_anti_collision(picc.cascade_level);

    // Halt the PICC - make it repond only to WUPA command.
    if (result != NULL)
    {
      uint8_t picc_cmd_buffer[] = {PICC_CMD_HALTA, 0x00, 0x00, 0x00 };
      (void)rc522_calculate_crc(picc_cmd_buffer, 2, &picc_cmd_buffer[2]);

      // Send the command to PICC.
      response_data = rc522_picc_write(RC522_CMD_TRANSCEIVE, picc_cmd_buffer, 4, &response_data_n, &response_data_n_bits);
      // Check if cascade bit is set. If so then the UID isn't fully fetched yet.
      if (response_data != NULL)
      {
        free(response_data);
      }

      // Clear the MFCrypto1On bit.
      rc522_clear_bitmask(RC522_REG_STATUS_2, 0x08);

      return result;
    }
  }
  else
  {
    // Go back to the first level of anticollision step.
    picc.cascade_level = 1;
  }

  return NULL;
}

uint8_t* rc522_get_picc_data()
{
  uint8_t* result = NULL;
  uint8_t* response_data = NULL;
  uint8_t response_data_n;
  uint32_t response_data_n_bits;

  uint8_t picc_present = rc522_picc_request();

  if (picc_present)
  {
    rc522_anti_collision(1);

    if (result != NULL)
    {
      // At least 18 bytes because it's data + CRC_A.
      uint8_t picc_cmd_buffer[18];
      picc_cmd_buffer[0] = PICC_CMD_MF_READ;
      picc_cmd_buffer[1] = 0x0;  // block address.
      // Calculate the CRC on the RC522 side.
      (void)rc522_calculate_crc(picc_cmd_buffer, 2, &picc_cmd_buffer[2]);

      // Send the command to PICC.
      response_data = rc522_picc_write(RC522_CMD_TRANSCEIVE, picc_cmd_buffer, 4, &response_data_n, &response_data_n_bits);
      if (response_data)
      {
        free(response_data);
      }

      // Clear the MFCrypto1On bit.
      rc522_clear_bitmask(RC522_REG_STATUS_2, 0x08);

      return result;
    }
  }

  return NULL;
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
    printf("%s : ", picc.uid_full ? "full" : "not full");
    for (int i = 0; i < 10; i++)
    {
      printf("%x ", serial_no[i]);
    }
    printf("\n");
    picc.uid_hot = false;
  }
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
