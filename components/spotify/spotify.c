#include "spotify.h"

#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"

#include <string.h>

static const char* TAG = "spotify";

// This struct is available through extern in spotify.h.
spotify_access_t spotify;
spotify_context_t spotify_context;

/*
 * HTTP event handler for the spotify module. This is where response's are collected in a static
 * buffer and parsed by cJSON.
 */
static esp_err_t spotify_http_event_handler(esp_http_client_event_t *evt);

// Memory used by this component. Trying to avoid sprinkling the code with mallocs and frees.
#define RESPONSE_BUF_SIZE     (1024 * 8)
#define SCRATCH_MEM_SIZE      (1024)
#define MAX_SONGS_IN_QUEUE        (5)
#define SONGS_QUEUE_MEM_SIZE  (MAX_SONG_ID_LENGTH * MAX_SONGS_IN_QUEUE)
static char* response_buf = NULL;
static char* scratch_mem = NULL;
static char* songs_queue = NULL;
static uint32_t songs_queue_write_counter = 0;


void spotify_init(void)
{
  spotify.fresh = false;
  memset(spotify.client_id, 0, sizeof(spotify.client_id));
  memset(spotify.client_secret, 0, sizeof(spotify.client_secret));
  memset(spotify.refresh_token, 0, sizeof(spotify.refresh_token));
  memset(spotify.access_token, 0, sizeof(spotify.access_token));

  // Context init.
  memset(&spotify_context, 0, sizeof(spotify_context_t));
  // This is used instead of `inited` field. It's used when the code in tasks.c writes using the
  // current spotify_context.song_id. I don't want it to write crap into PICC.
  spotify_context.is_playing = 0xFF;

  snprintf(spotify.client_id, sizeof(spotify.client_id), "%s", CONFIG_SPOTIFY_CLIENT_ID);
  snprintf(spotify.client_secret, sizeof(spotify.client_secret), "%s", CONFIG_SPOTIFY_CLIENT_SECRET);
  snprintf(spotify.refresh_token, sizeof(spotify.refresh_token), "%s", CONFIG_SPOTIFY_REFRESH_TOKEN);

  // We are assuming the init function is called only once. Otherwise we have a memory leak.
  response_buf = (char*)malloc(RESPONSE_BUF_SIZE);
  scratch_mem = (char*)malloc(SCRATCH_MEM_SIZE);
  songs_queue = (char*)malloc(SONGS_QUEUE_MEM_SIZE);
}

uint8_t spotify_is_fresh_access_token(void)
{
  return spotify.fresh;
}

void spotify_refresh_access_token(void)
{
  const char* const _url= "https://accounts.spotify.com/api/token";

  esp_http_client_config_t config = {
    .url = _url,
    .transport_type = HTTP_TRANSPORT_OVER_SSL,
    .event_handler = spotify_http_event_handler,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_http_client_set_method(client, HTTP_METHOD_POST);
  // Build a URL encoded key-value data pairs.
  snprintf(scratch_mem, SCRATCH_MEM_SIZE, "client_id=%s"
                                         "&client_secret=%s"
                                         "&refresh_token=%s"
                                         "&grant_type=refresh_token",
           spotify.client_id,
           spotify.client_secret,
           spotify.refresh_token);
  esp_http_client_set_post_field(client, scratch_mem, strlen(scratch_mem));

  esp_err_t err = esp_http_client_perform(client);

  if (err == ESP_OK)
  {
    ESP_LOGD(TAG, "Status = %d, content_length = %d",
             esp_http_client_get_status_code(client),
             esp_http_client_get_content_length(client));
  }

  // Closing the connection.
  esp_http_client_cleanup(client);
}

void spotify_query(void)
{
  const char* const _url = "https://api.spotify.com/v1/me/player";
  snprintf(scratch_mem, SCRATCH_MEM_SIZE, "Bearer %s", spotify.access_token);

  esp_http_client_config_t config = {
    .url = _url,
    .transport_type = HTTP_TRANSPORT_OVER_SSL,
    .event_handler = spotify_http_event_handler,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_http_client_set_method(client, HTTP_METHOD_GET);
  esp_http_client_set_header(client, "Authorization", scratch_mem);

  esp_err_t err = esp_http_client_perform(client);

  if (err == ESP_OK)
  {
    ESP_LOGD(TAG, "Status = %d, content_length = %d",
             esp_http_client_get_status_code(client),
             esp_http_client_get_content_length(client));
  }

  // Closing the connection.
  esp_http_client_cleanup(client);
}

void spotify_enqueue_song(const char* const song_id, const uint8_t song_id_len)
{
  const char* const _url = "https://api.spotify.com/v1/me/player/queue?uri=spotify:track:";
  const uint32_t _url_len = strlen(_url);
  // The idea below is to use the scratch buffer for building the URL and the header.
  char* const spotify_url = scratch_mem;
  memcpy(spotify_url, _url, _url_len);
  memcpy(spotify_url + _url_len, song_id, song_id_len);
  *(spotify_url + _url_len + song_id_len) = 0;

  char* const spotify_header = (scratch_mem + _url_len + song_id_len + 1);
  memcpy(spotify_header, "Bearer ", 7);
  memcpy(spotify_header + 7, spotify.access_token, strlen(spotify.access_token));
  *(spotify_header + 7 + strlen(spotify.access_token)) = 0;

  esp_http_client_config_t config = {
    .url = spotify_url,
    .transport_type = HTTP_TRANSPORT_OVER_SSL,
    .event_handler = spotify_http_event_handler,
    .is_async = false,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_http_client_set_header(client, "Authorization", spotify_header);
  esp_http_client_set_method(client, HTTP_METHOD_POST);

  // NOTE(michalc): _connect part of the _open can be made asynchronous.
  // Open connection and write header strings.
  if (esp_http_client_open(client, 0) == ESP_FAIL)
  {
    ESP_LOGW(TAG, "Failed to open connection!");
  }

  if (esp_http_client_fetch_headers(client) == ESP_FAIL)
  {
    ESP_LOGW(TAG, "Failed to fetch headers!");
  }
  else
  {
    ESP_LOGD(TAG, "Status = %d, content_length = %d",
             esp_http_client_get_status_code(client),
             esp_http_client_get_content_length(client));
  }

  // Closing the connection.
  esp_http_client_cleanup(client);
}

void spotify_next_song(void)
{
  const char* _url = "https://api.spotify.com/v1/me/player/next";
  snprintf(scratch_mem, SCRATCH_MEM_SIZE, "Bearer %s", spotify.access_token);

  // Modyfing the client here, which we assume is connected to the server.
  esp_http_client_config_t config = {
    .url = _url,
    .transport_type = HTTP_TRANSPORT_OVER_SSL,
    .event_handler = spotify_http_event_handler,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_http_client_set_header(client, "Authorization", scratch_mem);
  esp_http_client_set_method(client, HTTP_METHOD_POST);

  esp_err_t err = esp_http_client_perform(client);

  if (err == ESP_OK)
  {
    ESP_LOGD(TAG, "Status = %d, content_length = %d",
             esp_http_client_get_status_code(client),
             esp_http_client_get_content_length(client));
  }

  // Closing the connection.
  esp_http_client_cleanup(client);
}

void spotify_get_playlist(const uint32_t playlist_idx)
{
  const char* _url = "https://api.spotify.com/v1/me/playlists?limit=1&offset=";
  // The idea below is to use the scratch buffer for building the URL and the header.
  char* const spotify_url = scratch_mem;
  snprintf(spotify_url, SCRATCH_MEM_SIZE, "%s%d", _url, playlist_idx);

  char* const spotify_header = (scratch_mem + strlen(spotify_url) + 1);
  snprintf(spotify_header, SCRATCH_MEM_SIZE, "Bearer %s", spotify.access_token);

  // TODO(michalc): the response should read the 'total' field
  // TODO(michalc): the 'next' field is the url to the next playlist

  esp_http_client_config_t config = {
    .url = spotify_url,
    .transport_type = HTTP_TRANSPORT_OVER_SSL,
    .event_handler = spotify_http_event_handler,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_http_client_set_header(client, "Authorization", spotify_header);
  esp_http_client_set_method(client, HTTP_METHOD_GET);

  esp_err_t err = esp_http_client_perform(client);

  if (err == ESP_OK)
  {
    ESP_LOGD(TAG, "Status = %d, content_length = %d",
             esp_http_client_get_status_code(client),
             esp_http_client_get_content_length(client));
  }

  // Closing the connection.
  esp_http_client_cleanup(client);
}

void spotify_get_playlist_song(const char* playlist_id, const uint32_t song_idx)
{
  const char* _url = "https://api.spotify.com/v1/playlists/";
  // The idea below is to use the scratch buffer for building the URL and the header.
  char* const spotify_url = scratch_mem;
  snprintf(spotify_url, SCRATCH_MEM_SIZE,
           "%s%.*s/tracks?%s&%s%d", _url, MAX_PLAYLIST_ID_LENGTH, playlist_id, "fields=href(),items(track(name,id)),total()", "limit=1&offset=", song_idx);

  char* const spotify_header = (scratch_mem + strlen(spotify_url) + 1);
  snprintf(spotify_header, SCRATCH_MEM_SIZE, "Bearer %s", spotify.access_token);

  esp_http_client_config_t config = {
    .url = spotify_url,
    .transport_type = HTTP_TRANSPORT_OVER_SSL,
    .event_handler = spotify_http_event_handler,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_http_client_set_header(client, "Authorization", spotify_header);
  esp_http_client_set_method(client, HTTP_METHOD_GET);

  esp_err_t err = esp_http_client_perform(client);

  if (err == ESP_OK)
  {
    ESP_LOGD(TAG, "Status = %d, content_length = %d",
             esp_http_client_get_status_code(client),
             esp_http_client_get_content_length(client));
  }

  // Closing the connection.
  esp_http_client_cleanup(client);
}

static esp_err_t spotify_http_event_handler(esp_http_client_event_t *evt)
{
  static uint32_t response_bytes_count = 0;
  ESP_LOGD(TAG, "Handling response for client addr 0x%p", evt->client);

  switch(evt->event_id) {
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
      ESP_LOGD(TAG, "%s : %s", evt->header_key, evt->header_value);
      break;
    case HTTP_EVENT_ON_DATA:
      ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, chunk size %d", evt->data_len);
      // Collect the data into a buffer. Remember that a response doesn't have to have
      // data. It's HTTP so information can be stored in HEADERs.
      memcpy(&response_buf[response_bytes_count], evt->data, evt->data_len);
      response_bytes_count += evt->data_len;

      if (response_bytes_count >= RESPONSE_BUF_SIZE)
      {
        ESP_LOGE(TAG, "Not enough space in the response_buf... that's a yikes!");
        memset(response_buf, 0, RESPONSE_BUF_SIZE);
        response_bytes_count = 0;
      }

      // if (!esp_http_client_is_chunked_response(evt->client)) {
      //     printf("%.*s", evt->data_len, (char*)evt->data);
      // }
      break;
    case HTTP_EVENT_ON_FINISH:
      ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH, bytes to process %d", response_bytes_count);

      cJSON* response_json = NULL;

      // cJSON_Parse mallocs memory! Remember to run cJSON_Delete.
      response_json = cJSON_Parse(response_buf);

      if (response_json == NULL)
      {
        ESP_LOGW(TAG, "cJSON_Parse failed to parse the response_buf");
        goto bail;
      }

      // First check for a possible failure.
      cJSON* error = cJSON_GetObjectItem(response_json, "error");

      if (error != NULL)
      {
        cJSON* error_message = cJSON_GetObjectItem(response_json, "message");
        if (error_message != NULL)
        {
          char* error_message_value = cJSON_GetStringValue(error_message);
          // It's more common for the token to expired than to mess up the request.
          if (strcmp(error_message_value, "The access token expired") == 0)
          {
            ESP_LOGW(TAG, "The access token expired!");
          }
          else if (strcmp(error_message_value, "Only valid bearer authentication supported") == 0)
          {
            ESP_LOGW(TAG, "The access token is incorrect!");
          }
          spotify.fresh = false;
        }

        goto bail;
      }

      // The response contains the request that was sent under the href key.
      cJSON* href = cJSON_GetObjectItem(response_json, "href");

      if (href != NULL)
      {
        // TODO(michalc): this is fugly. The second strstr is here because otherwise we enter this
        // when reading a song from a playlist but since the response isn't the same this crashes.
        if (strstr(cJSON_GetStringValue(href), "playlists"))
        {
          cJSON* item = cJSON_GetArrayItem(cJSON_GetObjectItem(response_json, "items"), 0);
          // This is where the 'get song from playlist' response goes.
          if (strstr(cJSON_GetStringValue(href), "tracks"))
          {
            cJSON* track = cJSON_GetObjectItem(item, "track");
            ESP_LOGI(TAG, "Storing a track ID %s in slot %d",
                          cJSON_GetStringValue(cJSON_GetObjectItem(track, "id")),
                          (songs_queue_write_counter % MAX_SONGS_IN_QUEUE));
            char* const write_to = songs_queue + (songs_queue_write_counter++ % MAX_SONGS_IN_QUEUE) * MAX_SONG_ID_LENGTH;
            memcpy(write_to, cJSON_GetStringValue(cJSON_GetObjectItem(track, "id")), MAX_SONG_ID_LENGTH);
          }
          // This is where the 'get playlist by index' response goes.
          else
          {
            ESP_LOGI(TAG, "Got a response for the playlist %s ID %s",
                          cJSON_GetStringValue(cJSON_GetObjectItem(item, "name")),
                          cJSON_GetStringValue(cJSON_GetObjectItem(item, "id")));
            strncpy(spotify_context.playlist_id,
                    cJSON_GetStringValue(cJSON_GetObjectItem(item, "id")),
                    MAX_PLAYLIST_ID_LENGTH);
            strncpy(spotify_context.playlist_name,
                    cJSON_GetStringValue(cJSON_GetObjectItem(item, "name")),
                    MAX_PLAYLIST_NAME_LENGTH);
          }
        }
      }

      cJSON* access_token = cJSON_GetObjectItem(response_json, "access_token");

      if (access_token != NULL)
      {
        char* access_token_value = cJSON_GetStringValue(access_token);
        if (access_token_value)
        {
          ESP_LOGI(TAG, "Storing a new access token");
          strncpy(spotify.access_token, access_token_value, strlen(access_token_value));
          spotify.fresh = true;
        }

        goto bail;
      }

      cJSON* is_playing = NULL;

      is_playing = cJSON_GetObjectItem(response_json, "is_playing");
      if (is_playing)
      {
        if (cJSON_IsBool(is_playing))
        {
          if (cJSON_IsTrue(is_playing))
          {
            spotify_context.is_playing = 1;
          }
          else
          {
            spotify_context.is_playing = 0;
          }
        }
      }

      // Print the raw response data.
      ESP_LOGD(TAG, "\n%.*s\n", response_bytes_count, response_buf);

      cJSON* item = cJSON_GetObjectItem(response_json, "item");
      cJSON* artists = cJSON_GetObjectItem(item, "artists");
      artists = cJSON_GetArrayItem(artists, 0);
      cJSON* artist_name = cJSON_GetObjectItem(artists, "name");
      cJSON* song_title = cJSON_GetObjectItem(item, "name");
      cJSON* song_id = cJSON_GetObjectItem(item, "id");

      // NOTE(michalc): the size of spotify_context.artist buffer is 64.
      if (cJSON_GetStringValue(artist_name))
      {
        strncpy(spotify_context.artist, cJSON_GetStringValue(artist_name), MAX_ARTIST_NAME_LENGTH);
      }

      if (cJSON_GetStringValue(song_title))
      {
        strncpy(spotify_context.song_title, cJSON_GetStringValue(song_title), MAX_SONG_TITLE_LENGTH);
      }

      if (cJSON_GetStringValue(song_id))
      {
        strncpy(spotify_context.song_id, cJSON_GetStringValue(song_id), MAX_SONG_ID_LENGTH);
      }

bail:
      if(response_json) cJSON_Delete(response_json);
      memset(response_buf, 0, RESPONSE_BUF_SIZE);
      response_bytes_count = 0;
      break;
    case HTTP_EVENT_DISCONNECTED:
      ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
      break;
  }
  return ESP_OK;
}
