#include "rc522.h"  // TODO(michalc): remove this when fully ported to rfid_reader
#include "rfid_reader.h"
#include "spotify.h"
#include "periph.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include <string.h>

// TODO(michalc): This is defined in the periph.c... shouldn't be defined in two places.
#define GPIO_IRQ_PIN 15

#define RFID_OP_READ  0x0
#define RFID_OP_WRITE 0x1

static esp_timer_handle_t s_rfid_reader_timer;
TaskHandle_t x_task_rfid_read_or_write = NULL;
TaskHandle_t x_spotify = NULL;
QueueHandle_t q_rfid_to_spotify = NULL;

bool scanning_timer_running = false;
uint8_t reading_or_writing = RFID_OP_READ;


static void task_rfid_scanning(void* arg)
{
  bool picc_present = rfid_test_picc_presence();

  if (picc_present)
  {
    bool status = rfid_anti_collision(1);
    reading_or_writing = periph_get_button_state(1) == 1 ? RFID_OP_WRITE : RFID_OP_READ;
    xTaskNotify(x_task_rfid_read_or_write, reading_or_writing, eSetValueWithOverwrite);
    reading_or_writing = RFID_OP_READ;
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
    const uint8_t key[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    // Authenticate sector access.
    rc522_authenticate(PICC_CMD_MF_AUTH_KEY_A, block, key);

    // 16 bytes and 2 bytes for CRC.
    uint8_t transfer_buffer[18] = {};
    const char* song_id = spotify_playback.song_id;
    uint8_t spotify_should_act = 0;
    uint8_t msg[32] = {};

    if (reading_or_writing == RFID_OP_WRITE && spotify_playback.is_playing != 0xFF)
    {
      // TODO(michalc): wait for refresh of the Spotify's playback state.

      uint8_t write_buffer[32] = {};
      memset(write_buffer, '.', 32);
      memcpy(write_buffer, "sp_song", strlen("sp_song"));
      // Copy the last 16 bytes of the song id.
      memcpy(write_buffer + 16, song_id + strlen(song_id) - 16, 16);
      // Copy the length - last 16 bytes of the song id.
      memcpy(write_buffer + 16 - (strlen(song_id) - 16), song_id, strlen(song_id) - 16);

      // Write the data into PICC.
      memcpy(transfer_buffer, write_buffer, 16);
      rc522_write_picc_data(block, transfer_buffer);

      memcpy(transfer_buffer, write_buffer + 16, 16);
      rc522_write_picc_data(block + 1, transfer_buffer);
    }
    // Value 0f 0x0 means reading.
    else if (reading_or_writing == RFID_OP_READ)
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
        spotify_enqueue_song(msg_cursor);
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
  return scanning_timer_running ? ESP_OK : esp_timer_start_periodic(s_rfid_reader_timer, 125000);
}

esp_err_t scanning_timer_pause()
{
  return ! scanning_timer_running ? ESP_OK : esp_timer_stop(s_rfid_reader_timer);
}

void tasks_start(void)
{
  // Start the scanning task.
  const esp_timer_create_args_t timer_args = {
    .callback = &task_rfid_scanning,
    .arg = (void*)tag_handler,  // rc522.c function
    .name = "task_rfid_scanning",
  };

  // Not checking return value here because I can't imagine when this would fail (hubris?).
  esp_timer_create(&timer_args, &s_rfid_reader_timer);

  scanning_timer_resume();
  return;
}
