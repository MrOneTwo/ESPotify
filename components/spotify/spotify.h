// spotify.h - Michal Ciesielski 2021
//
// ESP32 module for interacting with the Spotify via REST API.

#ifndef SPOTIFY_H
#define SPOTIFY_H

#include <stdbool.h>
#include <stdint.h>

#define MAX_SONG_TITLE_LENGTH       (64U)
#define MAX_SONG_ID_LENGTH          (22U)
#define MAX_PLAYLIST_ID_LENGTH      (22U)
#define MAX_PLAYLIST_NAME_LENGTH    (16U)
#define MAX_ARTIST_NAME_LENGTH      (64U)

typedef struct spotify_access_t
{
  uint8_t fresh;
  char client_id[128];
  char client_secret[128];
  char refresh_token[256];
  char access_token[256];
} spotify_access_t;

typedef struct spotify_context_t
{
  uint8_t is_playing;
  char artist[MAX_ARTIST_NAME_LENGTH];
  char song_title[MAX_SONG_TITLE_LENGTH];
  char song_id[MAX_SONG_ID_LENGTH];
  char playlist_id[MAX_PLAYLIST_ID_LENGTH];
  char playlist_name[MAX_PLAYLIST_ID_LENGTH];
} spotify_context_t;

// TODO(michalc): instead of making it extern and passing it every time it's probably better to
// just operate on it inside the spotify module and never expose it.
extern spotify_context_t spotify_context;

/*
 * Init structures - mostly about setting the access tokens/codes.
 */
void spotify_init(void);

/*
 */
uint8_t spotify_is_fresh_access_token(void);

/*
 * Use the refresh token to update the access token. Access token expires after 1 hour.
 */
void spotify_refresh_access_token(void);

/*
 * This updates the spotify_playback_t static structure.
 */
void spotify_query(void);

/*
 * Push a song to the Spotify's queue.
 */
void spotify_enqueue_song(const char* const song_id, const uint8_t song_id_len);

/*
 * Make Spotify jump to the next song in the queue.
 */
void spotify_next_song(void);

void spotify_get_playlist(const uint32_t playlist_idx);

void spotify_get_playlist_song(const char* playlist_id, const uint32_t song_idx);

#endif // SPOTIFY_H
