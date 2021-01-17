#include "spotify.h"

#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"

#include <string.h>

static const char* TAG = "ESPotify";

// This struct is available through extern in spotify.h.
spotify_access_t spotify;
spotify_playback_t spotify_playback;

/*
 * HTTP event handler for the spotify module. This is where response's are collected in a static
 * buffer and parsed by cJSON.
 */
static esp_err_t spotify_http_event_handler(esp_http_client_event_t *evt);


void spotify_init()
{
  spotify.fresh = false;
  // This is used instead of `inited` field. It's used when the code in tasks.c write using the
  // current spotify_playback.song_id. I don't want it to write crap into PICC.
  spotify_playback.is_playing = 0xFF;
  memset(spotify.client_id, 0, sizeof(spotify.client_id));
  memset(spotify.client_secret, 0, sizeof(spotify.client_secret));
  memset(spotify.refresh_token, 0, sizeof(spotify.refresh_token));
  memset(spotify.access_token, 0, sizeof(spotify.access_token));

  snprintf(spotify.client_id, sizeof(spotify.client_id), "%s", CONFIG_SPOTIFY_CLIENT_ID);
  snprintf(spotify.client_secret, sizeof(spotify.client_secret), "%s", CONFIG_SPOTIFY_CLIENT_SECRET);
  snprintf(spotify.refresh_token, sizeof(spotify.refresh_token), "%s", CONFIG_SPOTIFY_REFRESH_TOKEN);
}

uint8_t spotify_is_fresh_access_token()
{
  return spotify.fresh;
}

void spotify_refresh_access_token()
{
  const char* spotify_url = "https://accounts.spotify.com/api/token";
  esp_http_client_config_t config = {
    .url = spotify_url,
    .transport_type = HTTP_TRANSPORT_OVER_SSL,
    .event_handler = spotify_http_event_handler,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_http_client_set_method(client, HTTP_METHOD_POST);
  char data[1024];
  snprintf(data, 1024, "client_id=%s&client_secret=%s&refresh_token=%s&grant_type=refresh_token",
           spotify.client_id,
           spotify.client_secret,
           spotify.refresh_token);
  esp_http_client_set_post_field(client, data, strlen(data));

  esp_err_t err = esp_http_client_perform(client);

  if (err == ESP_OK) {
   ESP_LOGI(TAG, "Status = %d, content_length = %d",
            esp_http_client_get_status_code(client),
            esp_http_client_get_content_length(client));
  }

  // Closing this connection.
  esp_http_client_cleanup(client);
}

void spotify_query()
{
  esp_http_client_handle_t client;

  char* access_token_header = (char*)malloc(280);
  snprintf(access_token_header, 280, "Bearer %s", spotify.access_token);

  // Modyfing the client here, which we assume is connected to the server.
  const char* spotify_url = "https://api.spotify.com/v1/me/player";
  esp_http_client_config_t config = {
    .url = spotify_url,
    .transport_type = HTTP_TRANSPORT_OVER_SSL,
    .event_handler = spotify_http_event_handler,
  };
  client = esp_http_client_init(&config);

  esp_http_client_set_header(client, "Authorization", access_token_header);
  esp_http_client_set_method(client, HTTP_METHOD_GET);


  esp_err_t err = esp_http_client_perform(client);

  if (err == ESP_OK)
  {
    ESP_LOGI(TAG, "Status = %d, content_length = %d",
            esp_http_client_get_status_code(client),
            esp_http_client_get_content_length(client));
  }
  free(access_token_header);
  esp_http_client_cleanup(client);
}

void spotify_enqueue_song(const char* const song_id)
{
  esp_http_client_handle_t client;

  char* const access_token_header = (char*)malloc(280);
  snprintf(access_token_header, 280, "Bearer %s", spotify.access_token);

  // Modyfing the client here, which we assume is connected to the server.
  const char* const _url = "https://api.spotify.com/v1/me/player/queue?uri=spotify:track:";
  char* const spotify_url = (char*)malloc(128);
  // Spotify's songs ID are 22 chars.
  snprintf(spotify_url, 128, "%s%.22s", _url, song_id);

  esp_http_client_config_t config = {
    .url = spotify_url,
    .transport_type = HTTP_TRANSPORT_OVER_SSL,
    .event_handler = spotify_http_event_handler,
  };
  client = esp_http_client_init(&config);

  esp_http_client_set_header(client, "Authorization", access_token_header);
  esp_http_client_set_method(client, HTTP_METHOD_POST);

  esp_err_t err = esp_http_client_perform(client);

  if (err == ESP_OK) {
    ESP_LOGW(TAG, "Status = %d, content_length = %d",
             esp_http_client_get_status_code(client),
             esp_http_client_get_content_length(client));
  }
  free(access_token_header);
  free(spotify_url);
  esp_http_client_cleanup(client);
}

void spotify_next_song()
{
  esp_http_client_handle_t client;

  char* access_token_header = (char*)malloc(280);
  snprintf(access_token_header, 280, "Bearer %s", spotify.access_token);

  // Modyfing the client here, which we assume is connected to the server.
  const char* spotify_url = "https://api.spotify.com/v1/me/player/next";
  esp_http_client_config_t config = {
    .url = spotify_url,
    .transport_type = HTTP_TRANSPORT_OVER_SSL,
    .event_handler = spotify_http_event_handler,
  };
  client = esp_http_client_init(&config);

  esp_http_client_set_header(client, "Authorization", access_token_header);
  esp_http_client_set_method(client, HTTP_METHOD_POST);

  esp_err_t err = esp_http_client_perform(client);

  if (err == ESP_OK) {
    ESP_LOGW(TAG, "Status = %d, content_length = %d",
             esp_http_client_get_status_code(client),
             esp_http_client_get_content_length(client));
  }
  free(access_token_header);
  esp_http_client_cleanup(client);
}

static esp_err_t spotify_http_event_handler(esp_http_client_event_t *evt)
{
  // TODO(michalc): the response_buf is pretty big... it would be better to put it on the heap.
  #define RESPONSE_BUF_SIZE (1024 * 5)
  // This buffer is local to this scope but the data is persistent.
  static char response_buf[RESPONSE_BUF_SIZE] = {};
  static uint32_t response_buf_tail = 0;

  switch(evt->event_id) {
    case HTTP_EVENT_ERROR:
      ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
      break;
    case HTTP_EVENT_ON_CONNECTED:
      ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
      break;
    case HTTP_EVENT_HEADER_SENT:
      ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
      break;
    case HTTP_EVENT_ON_HEADER:
      ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER");
      printf("%.*s", evt->data_len, (char*)evt->data);
      break;
    case HTTP_EVENT_ON_DATA:
      ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
      // Collect the data into a buffer.
      memcpy(&response_buf[response_buf_tail], evt->data, evt->data_len);
      response_buf_tail += evt->data_len;

      if (response_buf_tail >= RESPONSE_BUF_SIZE)
      {
        ESP_LOGI(TAG, "Not enough space in the response_buf... that's a yikes!");
      }

      // if (!esp_http_client_is_chunked_response(evt->client)) {
          // printf("%.*s", evt->data_len, (char*)evt->data);
      // }
      break;
    case HTTP_EVENT_ON_FINISH:
      // Check if there is data present in the response_buf.
      if (response_buf_tail)
      {
        // cJSON_Parse mallocs memory! Remember to run cJSON_Delete.
        cJSON* response_json = cJSON_Parse(response_buf);

        cJSON* access_token = cJSON_GetObjectItem(response_json, "access_token");
        cJSON* error = cJSON_GetObjectItem(response_json, "error");
        if (error != NULL)
        {
          cJSON* error_message = cJSON_GetObjectItem(response_json, "message");
          if (error_message != NULL)
          {
            char* error_message_value = cJSON_GetStringValue(error_message);
            if (strcmp(error_message_value, "Only valid bearer authentication supported") == 0)
            {
              ESP_LOGE(TAG, "The access token is incorrect!");
              spotify.fresh = false;
            }
            if (strcmp(error_message_value, "The access token expired") == 0)
            {
              ESP_LOGE(TAG, "The access token expired!");
              spotify.fresh = false;
            }
          }
        }

        if (access_token != NULL)
        {
          char* access_token_value = cJSON_GetStringValue(access_token);
          if (access_token_value)
          {
            strncpy(spotify.access_token, access_token_value, strlen(access_token_value));
            spotify.fresh = true;
          }
        }

        cJSON* is_playing = NULL;

        is_playing = cJSON_GetObjectItem(response_json, "is_playing");
        if (is_playing)
        {
          if (cJSON_IsBool(is_playing))
          {
            if (cJSON_IsTrue(is_playing))
            {
              spotify_playback.is_playing = 1;
            }
            else
            {
              spotify_playback.is_playing = 0;
            }
          }
        }

        printf("\n%.*s\n", RESPONSE_BUF_SIZE, response_buf);
        memset(response_buf, 0, RESPONSE_BUF_SIZE);
        response_buf_tail = 0;

        cJSON* item = cJSON_GetObjectItem(response_json, "item");
        cJSON* artists = cJSON_GetObjectItem(item, "artists");
        artists = cJSON_GetArrayItem(artists, 0);
        cJSON* artist_name = cJSON_GetObjectItem(artists, "name");
        cJSON* song_title = cJSON_GetObjectItem(item, "name");
        cJSON* song_id = cJSON_GetObjectItem(item, "id");

        // NOTE(michalc): the size of spotify_playback.artist buffer is 64.
        if (cJSON_GetStringValue(artist_name))
        {
          strncpy(spotify_playback.artist, cJSON_GetStringValue(artist_name), MAX_ARTIST_NAME_LENGTH);
        }

        if (cJSON_GetStringValue(song_title))
        {
          strncpy(spotify_playback.song_title, cJSON_GetStringValue(song_title), MAX_SONG_TITLE_LENGTH);
        }

        if (cJSON_GetStringValue(song_id))
        {
          strncpy(spotify_playback.song_id, cJSON_GetStringValue(song_id), MAX_SONG_ID_LENGTH);
        }

        cJSON_Delete(response_json);
      }
      ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
      break;
    case HTTP_EVENT_DISCONNECTED:
      ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
      break;
  }
  return ESP_OK;
}
