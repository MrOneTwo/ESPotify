#include "rc522.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


static esp_timer_handle_t s_rc522_timer;
TaskHandle_t x_task_rfid_read_or_write = NULL;


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
  uint32_t notification_value;

  // Wait indefinitely for a notification from the scanning task.
  (void)xTaskNotifyWait(0x0,
                        ULONG_MAX,
                        &notification_value,
                        portMAX_DELAY);

  const uint8_t sector = 2;
  static uint8_t block = 4 * sector - 4;
  uint8_t key[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  // Authenticate sector access.
  rc522_authenticate(PICC_CMD_MF_AUTH_KEY_A, block, key);

  uint8_t data[18] = {
    0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
    0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF
  };

  printf("READING %d\n", block);
  rc522_read_picc_data(block, data);
  printf("BLOCK %d DATA: %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n", block,
    data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
    data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15]);
}

void tasks_init(void)
{
  BaseType_t xReturned;

  xReturned = xTaskCreate(&task_rfid_read_or_write,     // Function that implements the task.
                          "task_rfid_read_or_write",    // Text name for the task.
                          2 * 1024 / 4,                 // Stack size in words, not bytes.
                          (void*) 1,                    // Parameter passed into the task.
                          5,                            // Priority at which the task is created.
                          &x_task_rfid_read_or_write);  // Used to pass out the created task's handle.

  if(xReturned == pdPASS)
  {
    // success
  }

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

  return rc522_resume();
}


