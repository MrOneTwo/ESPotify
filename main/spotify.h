#ifndef SPOTIFY_H
#define SPOTIFY_H

#include <stdbool.h>
#include <stdint.h>

#define MAX_SONG_TITLE_LENGTH   (64U)
#define MAX_SONG_ID_LENGTH      (32U)
#define MAX_ARTIST_NAME_LENGTH  (64U)

typedef struct spotify_access_t
{
  bool fresh;
  char client_id[128];
  char client_secret[128];
  char refresh_token[256];
  char access_token[256];
} spotify_access_t;

typedef struct spotify_playback_t
{
  bool is_playing;
  char artist[MAX_ARTIST_NAME_LENGTH];
  char song_title[MAX_SONG_TITLE_LENGTH];
  char song_id[MAX_SONG_ID_LENGTH];
} spotify_playback_t;

// TODO(michalc): instead of making it extern and passing it every time it's probably better to
// just operate on it inside the spotify module and never expose it.
extern spotify_access_t spotify;
extern spotify_playback_t spotify_playback;

/*
 * Init structures - mostly about setting the access tokens/codes.
 */
void spotify_init(spotify_access_t* spotify);

/*
 * Use the refresh token to update the access token. Access token expires after 1 hour.
 */
void spotify_refresh_access_token(spotify_access_t* spotify);

/*
 * This updates the spotify_playback_t static structure.
 */
void spotify_query(spotify_access_t* spotify);

/*
 * Push a song to the Spotify's queue.
 */
void spotify_enqueue_song(spotify_access_t* spotify, const char* const song_id);

/*
 * Make Spotify jump to the next song in the queue.
 */
void spotify_next_song(spotify_access_t* spotify);

#endif // SPOTIFY_H
