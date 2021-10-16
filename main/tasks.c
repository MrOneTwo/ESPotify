#include "tasks.h"
#include "spotify.h"
#include "periph.h"
#include "shared.h"

#ifdef CONFIG_RFID_READER
#include "rc522.h"  // TODO(michalc): remove this when fully ported to rfid_reader
#include "rfid_reader.h"
#endif // CONFIG_RFID_READER

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include <string.h>

// TODO(michalc): This is defined in the periph.c... shouldn't be defined in two places.
#define GPIO_IRQ_PIN 15

#define RFID_OP_READ  0x0
#define RFID_OP_WRITE 0x1

TaskHandle_t x_spotify = NULL;
TaskHandle_t x_spotify_read_playlist = NULL;
TaskHandle_t x_spotify_find_playlist = NULL;
QueueHandle_t q_rfid_to_spotify = NULL;

#ifdef CONFIG_RFID_READER
static esp_timer_handle_t s_rfid_reader_timer;
TaskHandle_t x_task_rfid_read_or_write = NULL;

bool scanning_timer_running = false;
uint8_t reading_or_writing = RFID_OP_READ;

esp_err_t scanning_timer_resume();
esp_err_t scanning_timer_pause();


// The read/write switch might be something else than pressing a button.
static inline uint8_t read_or_write()
{
  return periph_get_button_state(1);
}

static void task_rfid_scanning(void* arg)
{
  (void)arg;

  const bool picc_present = rfid_test_picc_presence();

  if (picc_present)
  {
    bool status = rfid_anti_collision(1);
    (void)status;

    reading_or_writing = read_or_write() == 1 ? RFID_OP_WRITE : RFID_OP_READ;

    ESP_LOGI("tasks", "PICC detected.");
    if (reading_or_writing == RFID_OP_READ)
    {
      ESP_LOGI("tasks", "Notifying to read.");
    }
    else
    {
      ESP_LOGI("tasks", "Notifying to write.");
    }

    xTaskNotify(x_task_rfid_read_or_write, reading_or_writing, eSetValueWithOverwrite);
  }
}

void task_rfid_read_or_write(void* pvParameters)
{
  uint8_t spotify_should_act = 0;
  uint32_t reading_or_writing = RFID_OP_READ;
  uint8_t msg[32] = {};

  while (1)
  {
    spotify_should_act = 0;

    // Wait indefinitely for a notification from the scanning task. When notified
    // to act, pause the scanning task.
    (void)xTaskNotifyWait(0x0,
                          ULONG_MAX,
                          &reading_or_writing,
                          portMAX_DELAY);

#ifdef CONFIG_RFID_READER
    defer(scanning_timer_pause(), scanning_timer_resume())
    {
#endif // CONFIG_RFID_READER
      ESP_LOGI("tasks", "Reading or writing to PICC");

      // MIFARE's first sector is not fully available since the first block is taken.
      // NTAG has first 5 (or 4, confused atm) pages (4 byte chunk) taken by manufacturer data.
      const uint8_t sector = 5;
      const uint32_t block_initial = 4 * sector - 4;

      // If we call the authentication on a PICC that doesn't conform to this type of authentication
      // we risk sending the PICC back into the original state. That might be an IDLE state.
      // This is dangerous because we might constantly wake the PICC up. It would behave as if we
      // used the WUPA command.
      if (rc522_get_last_picc().type == PICC_SUPPORTED_MIFARE_1K)
      {
        const uint8_t key[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

        // Authenticate sector access.
        rc522_authenticate(PICC_CMD_MIFARE_AUTH_KEY_A, block_initial, key);
      }

      // 16 bytes and 2 bytes for CRC.
      uint8_t transfer_buffer[32] = {};
      const char* song_id = spotify_context.song_id;

      if (reading_or_writing == RFID_OP_WRITE && spotify_context.is_playing != 0xFF)
      {
        // TODO(michalc): wait for refresh of the Spotify's context state.

        uint8_t write_buffer[32] = {};
        memset(write_buffer, '.', 32);
        memcpy(write_buffer, "sp_song", strlen("sp_song"));
        // Copy the song ID aligned to the end of the buffer.
        memcpy(write_buffer + 32 - strlen(song_id), song_id, strlen(song_id));

        rc522_write_picc_data(block_initial, write_buffer, 32);
      }
      // Value 0f 0x0 means reading.
      else if (reading_or_writing == RFID_OP_READ)
      {
        uint8_t read_buffer[32] = {};
        // We want to send a message to the Spotify task.
        spotify_should_act = 1;

        uint32_t block_to_read = block_initial;

        if (rc522_read_picc_data(block_to_read, transfer_buffer) != SUCCESS)
        {
          spotify_should_act = 0;
        }
        else
        {
          memcpy(read_buffer, transfer_buffer, 16);
        }

        // There is different block addressing for different PICCs.
        if (rc522_get_last_picc().type == PICC_SUPPORTED_MIFARE_1K)
        {
          block_to_read = block_to_read + 1;
        }
        else if (rc522_get_last_picc().type == PICC_SUPPORTED_NTAG213)
        {
          block_to_read = block_to_read + 4;
        }

        if (rc522_read_picc_data(block_to_read, transfer_buffer) != SUCCESS)
        {
          spotify_should_act = 0;
        }
        else
        {
          memcpy(read_buffer + 16, transfer_buffer, 16);
          // NOTE(michalc): what's saved in the PICC is the message we send. Might change in the
          // future.
          memcpy(msg, read_buffer, 32);
        }
      }

      rc522_picc_halta(PICC_CMD_HALTA);
      // Clear the MFCrypto1On bit.
      rc522_clear_bitmask(RC522_REG_STATUS_2, 0x08);
#ifdef CONFIG_RFID_READER
    }
#endif // CONFIG_RFID_READER

    if (spotify_should_act)
    {
      (void)xQueueSendToBack(q_rfid_to_spotify, (const void*)msg, portMAX_DELAY);
    }
  }
}
#endif // CONFIG_RFID_READER

void task_spotify(void* pvParameters)
{
  char msg[32] = {};
  str song_id = {};

  while (1)
  {
    // Block forever waiting for a message.
    const BaseType_t status = xQueueReceive(q_rfid_to_spotify, msg, portMAX_DELAY);
    ESP_LOGI("tasks", "task_spotify got msg %.32s", msg);

#ifdef CONFIG_RFID_READER
    defer(scanning_timer_pause(), scanning_timer_resume())
    {
#endif // CONFIG_RFID_READER
      // TODO(michalc): This can lock
      // TODO(michalc): what if the token expires between here and enqueue song.
      while (!spotify_is_fresh_access_token())
      {
        ESP_LOGW("tasks", "Refreshing the access token");
        spotify_refresh_access_token();
        vTaskDelay(200);
      }

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
          song_id.data = msg_cursor;
          song_id.length = MAX_SONG_ID_LENGTH;
          ESP_LOGI("tasks", "Enqueueing song %.*s", song_id.length, song_id.data);
          spotify_enqueue_song(song_id.data, song_id.length);
        }
      }
      else
      {
        // TODO(michalc): I don't expect we'll get here...
      }
#ifdef CONFIG_RFID_READER
    }
#endif // CONFIG_RFID_READER
  }
}

void task_spotify_read_playlist(void* pvParameters)
{
  while (1)
  {
    vTaskSuspend(x_spotify_read_playlist);
    if (spotify_context.playlist_id[0] == 0)
    {
      ESP_LOGW("tasks", "Requested to read playlist contents but don't know the playlist's ID");
      continue;
    }

#ifdef CONFIG_RFID_READER
    defer(scanning_timer_pause(), scanning_timer_resume())
    {
#endif // CONFIG_RFID_READER
      while (!spotify_is_fresh_access_token())
      {
        ESP_LOGW("tasks", "Refreshing the access token");
        spotify_refresh_access_token();
        vTaskDelay(200);
      }

      // TODO(michalc): this is just a placeholder
      for (uint8_t i = 0; i < 8; i++)
      {
        spotify_get_playlist_song(spotify_context.playlist_id, i);
      }
#ifdef CONFIG_RFID_READER
    }
#endif // CONFIG_RFID_READER
  }
}

void task_spotify_find_playlist(void* pvParameters)
{
  const char* playlist_name = "Score";

  while (1)
  {
    vTaskSuspend(x_spotify_find_playlist);

#ifdef CONFIG_RFID_READER
    defer(scanning_timer_pause(), scanning_timer_resume())
    {
#endif // CONFIG_RFID_READER
      while (!spotify_is_fresh_access_token())
      {
        ESP_LOGW("tasks", "Refreshing the access token");
        spotify_refresh_access_token();
        vTaskDelay(200);
      }

      // TODO(michalc): this is just a placeholder
      for (uint8_t i = 0; i < 8; i++)
      {
        spotify_get_playlist(i);

        if (0 == strncmp(spotify_context.playlist_name, playlist_name, MAX_PLAYLIST_ID_LENGTH))
        {
          ESP_LOGI("tasks", "Found a playlist: %s %s", spotify_context.playlist_name, spotify_context.playlist_id);
          break;
        }
      }
#ifdef CONFIG_RFID_READER
    }
#endif // CONFIG_RFID_READER
  }
}


void tasks_init(void)
{
  const uint8_t queue_length = 4U;
  const uint8_t queue_element_size = 32U;
  q_rfid_to_spotify = xQueueCreate(queue_length, queue_element_size);

  BaseType_t xReturned;

#ifdef CONFIG_RFID_READER
  xReturned = xTaskCreate(&task_rfid_read_or_write,     // Function that implements the task.
                          "task_rfid_read_or_write",    // Text name for the task.
                          8 * 1024 / 4,                 // Stack size in words, not bytes.
                          (void*) 1,                    // Parameter passed into the task.
                          6,                            // Priority at which the task is created.
                          &x_task_rfid_read_or_write);  // Used to pass out the created task's handle.

  if(xReturned == pdPASS)
  {
    // success
  }
#endif // CONFIG_RFID_READER

  xReturned = xTaskCreate(&task_spotify,     // Function that implements the task.
                          "task_spotify",    // Text name for the task.
                          16 * 1024 / 4,                 // Stack size in words, not bytes.
                          (void*) 1,                    // Parameter passed into the task.
                          5,                            // Priority at which the task is created.
                          &x_spotify);  // Used to pass out the created task's handle.

  if(xReturned == pdPASS)
  {
    // success
  }

  xReturned = xTaskCreate(&task_spotify_read_playlist,
                          "task_spotify_read_playlist",
                          16 * 1024 / 4,                 // Stack size in words, not bytes.
                          (void*) 1,                    // Parameter passed into the task.
                          5,                            // Priority at which the task is created.
                          &x_spotify_read_playlist);    // Used to pass out the created task's handle.

  if(xReturned == pdPASS)
  {
    // success
  }

  xReturned = xTaskCreate(&task_spotify_find_playlist,
                          "task_spotify_find_playlist",
                          16 * 1024 / 4,                 // Stack size in words, not bytes.
                          (void*) 1,                    // Parameter passed into the task.
                          5,                            // Priority at which the task is created.
                          &x_spotify_find_playlist);    // Used to pass out the created task's handle.

  if(xReturned == pdPASS)
  {
    // success
  }
}

#ifdef CONFIG_RFID_READER
esp_err_t scanning_timer_resume()
{
  // 125000 microseconds means 8Hz.
  if (!scanning_timer_running)
  {
    esp_timer_start_periodic(s_rfid_reader_timer, 125000 / 4);
    scanning_timer_running = true;
  }
  return ESP_OK;
}

esp_err_t scanning_timer_pause()
{
  if (scanning_timer_running)
  {
    esp_timer_stop(s_rfid_reader_timer);
    scanning_timer_running = false;
  }
  return ESP_OK;
}
#endif // CONFIG_RFID_READER

void tasks_start(void)
{
#ifdef CONFIG_RFID_READER
  // Start the scanning task.
  const esp_timer_create_args_t timer_args = {
    .callback = &task_rfid_scanning,
    .arg = (void*)tag_handler,  // rc522.c function
    .name = "task_rfid_scanning",
  };

  // Not checking return value here because I can't imagine when this would fail (hubris?).
  esp_timer_create(&timer_args, &s_rfid_reader_timer);

  scanning_timer_resume();
#endif // CONFIG_RFID_READER
  // vTaskResume(x_spotify_find_playlist);
  return;
}
