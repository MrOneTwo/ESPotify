#include "rc522.h"
#include "spotify.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include <string.h>


static esp_timer_handle_t s_rc522_timer;
TaskHandle_t x_task_rfid_read_or_write = NULL;
TaskHandle_t x_spotify = NULL;
QueueHandle_t q_rfid_to_spotify = NULL;

bool scanning_timer_running = false;


static void task_rfid_scanning(void* arg)
{
  status_e picc_present = rc522_get_picc_id();

  if (picc_present == SUCCESS)
  {
    status_e status = rc522_anti_collision(1);
    xTaskNotify(x_task_rfid_read_or_write, 0, eNoAction);
  }

  (void)arg;
}

void task_rfid_read_or_write(void* pvParameters)
{
  while (1)
  {
    uint32_t notification_value;

    // Wait indefinitely for a notification from the scanning task.
    (void)xTaskNotifyWait(0x0,
                          ULONG_MAX,
                          &notification_value,
                          portMAX_DELAY);
    ESP_LOGI("tasks", "Reading or writing to PICC");

    const uint8_t sector = 2;
    static uint8_t block = 4 * sector - 4;
    uint8_t key[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    // Authenticate sector access.
    rc522_authenticate(PICC_CMD_MF_AUTH_KEY_A, block, key);

    uint8_t transfer_buffer[18] = {
      'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',
      'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p'
    };
    uint8_t data_buffer[32] = {};
    const char* _song_id = "5lRNFkvM9qPxjt3U3drUrl";

    // {
    //   memset(data_buffer, '.', 32);
    //   memcpy(data_buffer, "sp_song", strlen("sp_song"));
    //   // Copy the last 16 bytes of the song id.
    //   memcpy(data_buffer + 16, _song_id + strlen(_song_id) - 16, 16);
    //   // Copy the length - last 16 bytes of the song id.
    //   memcpy(data_buffer + 16 - (strlen(_song_id) - 16), _song_id, strlen(_song_id) - 16);
    //   printf("---- %.32s\n", data_buffer);

    //   memcpy(transfer_buffer, data_buffer, 16);
    //   rc522_write_picc_data(block, transfer_buffer);

    //   memcpy(transfer_buffer, data_buffer + 16, 16);
    //   rc522_write_picc_data(block + 1, transfer_buffer);
    // }

    char msg[32];

    {
      rc522_read_picc_data(block, transfer_buffer);
      memcpy(msg, transfer_buffer, 16);

      rc522_read_picc_data(block + 1, transfer_buffer);
      memcpy(msg + 16, transfer_buffer, 16);
    }

    rc522_picc_halta(PICC_CMD_HALTA);
    // Clear the MFCrypto1On bit.
    rc522_clear_bitmask(RC522_REG_STATUS_2, 0x08);

    (void)xQueueSendToBack(q_rfid_to_spotify, (const void*)msg, portMAX_DELAY);
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
