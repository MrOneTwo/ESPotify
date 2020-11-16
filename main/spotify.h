#ifndef SPOTIFY_H
#define SPOTIFY_H

#include <stdbool.h>
#include <stdint.h>


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
  char artist[64];
  char song_title[64];
  char song_id[32];
} spotify_playback_t;

extern spotify_access_t spotify;
extern spotify_playback_t spotify_playback;

void spotify_init(spotify_access_t* spotify);
void spotify_query(spotify_access_t* spotify);
void spotify_refresh_access_token(spotify_access_t* spotify);

void spotify_enqueue_song(spotify_access_t* spotify, const char* const song_id);
// https://api.spotify.com/v1/me/player/queue?uri=spotify:track:ID

void spotify_next_song(spotify_access_t* spotify);

#endif // SPOTIFY_H
