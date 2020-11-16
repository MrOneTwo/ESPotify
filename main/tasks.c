#include "rc522.h"
#include "spotify.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include <string.h>

// TODO(michalc): This is defined in the hardware.c... shouldn't be defined in two places.
#define GPIO_IRQ_PIN 15


static esp_timer_handle_t s_rc522_timer;
TaskHandle_t x_task_rfid_read_or_write = NULL;
TaskHandle_t x_spotify = NULL;
QueueHandle_t q_rfid_to_spotify = NULL;

bool scanning_timer_running = false;
uint8_t reading_or_writing = 0;

// NOTE(michalc): I'm not really happy with this being here. I wanted this scope to be only
// about tasks. It is what it is.
void gpio_isr_callback(void* arg)
{
  uint32_t gpio_num = *(uint32_t*)arg;
  // Next operation will be writing to a PICC.
  if (gpio_num == GPIO_IRQ_PIN)
  {
    reading_or_writing = 0x1;
  }
}

static void task_rfid_scanning(void* arg)
{
  status_e picc_present = rc522_get_picc_id();

  if (picc_present == SUCCESS)
  {
    status_e status = rc522_anti_collision(1);
    xTaskNotify(x_task_rfid_read_or_write, reading_or_writing, eSetValueWithOverwrite);
    reading_or_writing = 0x0;
  }

  (void)arg;
}

void task_rfid_read_or_write(void* pvParameters)
{
  while (1)
  {
    uint32_t reading_or_writing;

    // Wait indefinitely for a notification from the scanning task.
    (void)xTaskNotifyWait(0x0,
                          ULONG_MAX,
                          &reading_or_writing,
                          portMAX_DELAY);
    ESP_LOGI("tasks", "Reading or writing to PICC");

    const uint8_t sector = 2;
    static uint8_t block = 4 * sector - 4;
    uint8_t key[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    // Authenticate sector access.
    rc522_authenticate(PICC_CMD_MF_AUTH_KEY_A, block, key);

    // 16 bytes and 2 bytes for CRC.
    uint8_t transfer_buffer[18] = {};
    const char* _song_id = "2OY8UbvrVHPxTENsdHWnpr";
    uint8_t spotify_should_act = 0;
    uint8_t msg[32] = {};

    // Value of 0x1 means writing.
    if (reading_or_writing == 0x1)
    {
      // TODO(michalc): wait for refresh of the Spotify's playback state.

      uint8_t write_buffer[32] = {};
      memset(write_buffer, '.', 32);
      memcpy(write_buffer, "sp_song", strlen("sp_song"));
      // Copy the last 16 bytes of the song id.
      memcpy(write_buffer + 16, _song_id + strlen(_song_id) - 16, 16);
      // Copy the length - last 16 bytes of the song id.
      memcpy(write_buffer + 16 - (strlen(_song_id) - 16), _song_id, strlen(_song_id) - 16);

      // Write the data into PICC.
      memcpy(transfer_buffer, write_buffer, 16);
      rc522_write_picc_data(block, transfer_buffer);

      memcpy(transfer_buffer, write_buffer + 16, 16);
      rc522_write_picc_data(block + 1, transfer_buffer);
    }
    // Value 0f 0x0 means reading.
    else if (reading_or_writing == 0x0)
    {
      uint8_t read_buffer[32] = {};

      rc522_read_picc_data(block, transfer_buffer);
      memcpy(read_buffer, transfer_buffer, 16);

      rc522_read_picc_data(block + 1, transfer_buffer);
      memcpy(read_buffer + 16, transfer_buffer, 16);

      // We want to send a message to the Spotify task.
      spotify_should_act = 1;
      // NOTE(michalc): what's saved in the PICC is the message we send. Might change in the
      // future.
      memcpy(msg, read_buffer, 32);
    }

    rc522_picc_halta(PICC_CMD_HALTA);
    // Clear the MFCrypto1On bit.
    rc522_clear_bitmask(RC522_REG_STATUS_2, 0x08);

    if (spotify_should_act)
    {
      (void)xQueueSendToBack(q_rfid_to_spotify, (const void*)msg, portMAX_DELAY);
    }
  }
}

void task_spotify(void* pvParameters)
{
  while (1)
  {
    char msg[32] = {};
    // Block forever waiting for a message.
    BaseType_t status;
    status = xQueueReceive(q_rfid_to_spotify, msg, portMAX_DELAY);
    ESP_LOGI("tasks", "task_spotify got msg %.32s", msg);

    if(status == pdPASS)
    {
      // spotify_query(&spotify);
      if (memcmp(msg, "sp_song", strlen("sp_song")) == 0)
      {
        char* msg_cursor = msg;
        do
        {
          msg_cursor++;
        }
        while( !(*(msg_cursor - 1) == '.' && *msg_cursor != '.') );
        ESP_LOGI("tasks", "Enqueueing song %.22s", msg_cursor);
        spotify_enqueue_song(&spotify, msg_cursor);
      }
    }
    else
    {
      // TODO(michalc): I don't expect we'll get here...
    }
  }
}

void tasks_init(void)
{
  BaseType_t xReturned;

  xReturned = xTaskCreate(&task_rfid_read_or_write,     // Function that implements the task.
                          "task_rfid_read_or_write",    // Text name for the task.
                          8 * 1024 / 4,                 // Stack size in words, not bytes.
                          (void*) 1,                    // Parameter passed into the task.
                          5,                            // Priority at which the task is created.
                          &x_task_rfid_read_or_write);  // Used to pass out the created task's handle.

  if(xReturned == pdPASS)
  {
    // success
  }

  xReturned = xTaskCreate(&task_spotify,     // Function that implements the task.
                          "task_spotify",    // Text name for the task.
                          32 * 1024 / 4,                 // Stack size in words, not bytes.
                          (void*) 1,                    // Parameter passed into the task.
                          5,                            // Priority at which the task is created.
                          &x_spotify);  // Used to pass out the created task's handle.

  if(xReturned == pdPASS)
  {
    // success
  }

  const uint8_t queue_length = 4U;
  const uint8_t queue_element_size = 32U;
  q_rfid_to_spotify = xQueueCreate(queue_length, queue_element_size);

}

esp_err_t scanning_timer_resume()
{
  // 125000 microseconds means 8Hz.
  return scanning_timer_running ? ESP_OK : esp_timer_start_periodic(s_rc522_timer, 125000);
}

esp_err_t scanning_timer_pause()
{
  return ! scanning_timer_running ? ESP_OK : esp_timer_stop(s_rc522_timer);
}

void tasks_start(void)
{
  // Start the scanning task.
  const esp_timer_create_args_t timer_args = {
    .callback = &task_rfid_scanning,
    .arg = (void*)tag_handler,
    .name = "task_rfid_scanning",
  };

  esp_err_t ret = esp_timer_create(&timer_args, &s_rc522_timer);

  if(ret != ESP_OK)
  {
    return ret;
  }

  scanning_timer_resume();
  return;
}
