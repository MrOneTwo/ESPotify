# This script generates an URL to give permissions to the app.
# It saves the URL in your clipboard. Just CTRL+V it in your
# browser.

set CLIENT_ID paste_it_here
# set this URI in the apps settings
set REDIRECT_URI paste_it_here

#images
set -a _SCOPES "ugc-image-upload"
# listening history
set -a _SCOPES "user-read-recently-played"
set -a _SCOPES "user-top-read"
set -a _SCOPES "user-read-playback-position"
# spotify connect
set -a _SCOPES "user-read-playback-state"
set -a _SCOPES "user-modify-playback-state"
set -a _SCOPES "user-read-currently-playing"
# playback
set -a _SCOPES "app-remote-control"
set -a _SCOPES "streaming"
# playlists
# set -a _SCOPES "playlist-modify-public"
# set -a _SCOPES "playlist-modify-private"
# set -a _SCOPES "playlist-read-private"
# set -a _SCOPES "playlist-read-collaborative"
# follow
# set -a _SCOPES "user-follow-modify"
set -a _SCOPES "user-follow-read"
# library
# set -a _SCOPES "user-library-modify"
set -a _SCOPES "user-library-read"
# users
# set -a _SCOPES "user-read-email"
# set -a _SCOPES "user-read-private"

set SCOPES (string join "%20" $_SCOPES)
set STATE 34fFs29kd09

echo "https://accounts.spotify.com/authorize?client_id=$CLIENT_ID&response_type=code&redirect_uri=$REDIRECT_URI&scope=$SCOPES&state=$STATE" \
| xclip -sel c
