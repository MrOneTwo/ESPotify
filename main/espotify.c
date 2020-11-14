#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "cJSON.h"

#include "spotify.h"
#include "rfid.h"
#include "shared.h"

#define MIN(a, b) (a <= b ? a : b)

// Parameters set via menuconfig target.
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY
#define SPOTIFY_CLIENT_ID          CONFIG_SPOTIFY_CLIENT_ID
#define SPOTIFY_CLIENT_SECRET      CONFIG_SPOTIFY_CLIENT_SECRET
#define SPOTIFY_REFRESH_TOKEN      CONFIG_SPOTIFY_REFRESH_TOKEN

// FreeRTOS event group to signal when we are connecte
static EventGroupHandle_t s_wifi_event_group;

// The event group allows multiple bits for each event, but we only care about two even
//  - we are connected to the AP with an IP
//  - we failed to connect after the maximum amount of retries
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1


static int s_retry_num = 0;

//
// FUNCTION PROTOTYPES
//
static esp_err_t http_event_handler(esp_http_client_event_t *evt);
static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data);
static esp_err_t http_event_handler(esp_http_client_event_t *evt);
static esp_err_t get_handler(httpd_req_t *req);
static esp_err_t post_handler(httpd_req_t *req);



static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
  {
    esp_wifi_connect();
  }
  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
  {
    if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
    {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(TAG, "retry to connect to the AP");
    }
    else
    {
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
    ESP_LOGI(TAG, "Connect to the AP fail");
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(TAG, "Got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

#define RESPONSE_BUF_SIZE (1024 * 5)
char response_buf[RESPONSE_BUF_SIZE];
uint32_t response_buf_tail = 0;


// From https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_http_client.html
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
  switch(evt->event_id)
  {
    case HTTP_EVENT_ERROR:
      ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
      break;
    case HTTP_EVENT_ON_CONNECTED:
      ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
      break;
    case HTTP_EVENT_HEADER_SENT:
      ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
      break;
    case HTTP_EVENT_ON_HEADER:
      ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER");
      printf("%.*s", evt->data_len, (char*)evt->data);
      break;
    case HTTP_EVENT_ON_DATA:
      ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
      // Collect the data into a buffer.
      memcpy(&response_buf[response_buf_tail], evt->data, evt->data_len);
      response_buf_tail += evt->data_len;

      if (response_buf_tail >= RESPONSE_BUF_SIZE)
      {
        ESP_LOGD(TAG, "Not enough space in the response_buf... that's a yikes!");
      }

      // if (!esp_http_client_is_chunked_response(evt->client)) {
          // printf("%.*s", evt->data_len, (char*)evt->data);
      // }
      break;
    case HTTP_EVENT_ON_FINISH:
      ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
      break;
    case HTTP_EVENT_DISCONNECTED:
      ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
      break;
  }
  return ESP_OK;
}

static esp_err_t get_handler(httpd_req_t *req)
{
  ESP_LOGI(TAG, "get_handler");

  char content[128];
  size_t recv_size = MIN(req->content_len, sizeof(content));
  int ret = httpd_req_recv(req, content, recv_size);
  printf("%.*s", req->content_len, content);

  // Send a simple response
  const char resp[] = "ESP32 /espotify endpoint";
  httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

// Our URI handler function to be called during POST /uri request
static esp_err_t post_handler(httpd_req_t *req)
{
  ESP_LOGI(TAG, "post_handler");
  // Destination buffer for content of HTTP POST request.
  // httpd_req_recv() accepts char* only, but content could
  // as well be any binary data (needs type casting).
  // In case of string data, null termination will be absent, and
  // content length would give length of string.
  char content[128];

  // Truncate if content length larger than the buffer.
  size_t recv_size = MIN(req->content_len, sizeof(content));

  int ret = httpd_req_recv(req, content, recv_size);
  // 0 idicates connection closed.
  if (ret <= 0)
  {
    if (ret == HTTPD_SOCK_ERR_TIMEOUT)
    {
      // In case of timeout one can choose to retry calling
      // httpd_req_recv(), but to keep it simple, here we
      // respond with an HTTP 408 (Request Timeout) error.
      httpd_resp_send_408(req);
    }
    // In case of error, returning ESP_FAIL will
    // ensure that the underlying socket is closed.
    return ESP_FAIL;
  }

  /* Send a simple response */
  const char resp[] = "ESP32 /espotify endpoint";
  httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

// URI setups.
httpd_uri_t uri_get = {
  .uri      = "/espotify",
  .method   = HTTP_GET,
  .handler  = get_handler,
  .user_ctx = NULL
};

httpd_uri_t uri_post = {
  .uri      = "/espotify",
  .method   = HTTP_POST,
  .handler  = post_handler,
  .user_ctx = NULL
};

httpd_handle_t start_webserver(void)
{
  // Generate default configuration
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  // Empty handle to esp_http_server
  httpd_handle_t server = NULL;

  // Start the httpd server
  if (httpd_start(&server, &config) == ESP_OK)
  {
    // Register URI handlers
    httpd_register_uri_handler(server, &uri_get);
    httpd_register_uri_handler(server, &uri_post);
  }
  // If server failed to start, handle will be NULL
  return server;
}

void stop_webserver(httpd_handle_t server)
{
  if (server)
  {
    httpd_stop(server);
  }
}

static void wifi_init_sta(void)
{
  s_wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      &event_handler,
                                                      NULL,
                                                      &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                      IP_EVENT_STA_GOT_IP,
                                                      &event_handler,
                                                      NULL,
                                                      &instance_got_ip));

  wifi_config_t wifi_config = {
    .sta = {
        .ssid = EXAMPLE_ESP_WIFI_SSID,
        .password = EXAMPLE_ESP_WIFI_PASS,
        // Setting a password implies station will connect to all security modes including WEP/W
        // However these modes are deprecated and not advisable to be used. In case your Access point
        // doesn't support WPA2, these mode can be enabled by commenting below line.
        .threshold.authmode = WIFI_AUTH_WPA2_PSK,

    .pmf_cfg = {
        .capable = true,
        .required = false
      },
    },
  };
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
  ESP_ERROR_CHECK(esp_wifi_start() );

  ESP_LOGI(TAG, "wifi_init_sta finished.");

  // Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maxi
  // number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above).
  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                         WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                         pdFALSE,
                                         pdFALSE,
                                         portMAX_DELAY);

  // xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actua
  // happened.
  if (bits & WIFI_CONNECTED_BIT)
  {
    ESP_LOGI(TAG, "Connected to ap SSID:%s password:%s",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
  }
  else if (bits & WIFI_FAIL_BIT)
  {
    ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
  }
  else
  {
    ESP_LOGE(TAG, "UNEXPECTED EVENT");
  }

  // The event will not be processed after unregister
  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
  vEventGroupDelete(s_wifi_event_group);
}

void app_main(void)
{
  esp_log_level_set("*", ESP_LOG_WARN);
  esp_log_level_set("wifi", ESP_LOG_WARN);
  esp_log_level_set("dhcpc", ESP_LOG_INFO);

  //Initialize NonVolatile Storage.
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  wifi_init_sta();

  spotify_init(&spotify);
  rfid_init();

  while (1)
  {
    if (!spotify.fresh)
    {
      ESP_LOGW(TAG, "Refreshing the access token");
      spotify_refresh_access_token(&spotify);
    }
    spotify_query(&spotify);

    // NOTE(michalc): the ESP_LOGI below flushes the output I think. That's why those prinfs fails
    // without subsequent ESP_LOGI.
    printf("Music playing: %s\n", spotify_playback.is_playing ? "YES" : "NO");
    printf("Artist: %s\n", spotify_playback.artist);
    printf("Song: %s\n", spotify_playback.song_title);
    printf("Song ID: %s\n", spotify_playback.song_id);

    vTaskDelay(500);

    spotify_enqueue_song(&spotify, NULL);
    spotify_next_song(&spotify);

    vTaskDelay(500);
  }

  if (start_webserver() == NULL)
  {
    ESP_LOGE(TAG, "Failed to start the webserver!");
  }
  else
  {
    ESP_LOGI(TAG, "Started the webserver!");
  }
}
