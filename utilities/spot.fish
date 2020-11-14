set CLIENT_ID paste_it_here
set CLIENT_SECRET paste_it_here
set CODE paste_it_here

set ACCESS_TOKEN paste_it_here
set REFRESH_TOKEN paste_it_here
set STATE 34fFs29kd09

# format (probably not https for you) http://your_url.ddns.net:PORT/endpoint
# be sure to set this in the Spotify's app settings
set REDIRECT_URI paste_it_here

# Exchange code for access token.
curl -i -X POST -d client_id=$CLIENT_ID -d client_secret=$CLIENT_SECRET \
  -d grant_type=authorization_code -d code=$CODE -d redirect_uri=$REDIRECT_URI \
  https://accounts.spotify.com/api/token


# Refresh the access token.
# curl -i -X POST -d client_id=$CLIENT_ID -d client_secret=$CLIENT_SECRET \
#   -d grant_type=refresh_token -d refresh_token=$REFRESH_TOKEN \
#   https://accounts.spotify.com/api/token

