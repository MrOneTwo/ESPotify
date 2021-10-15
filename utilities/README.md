1. Grab the Client ID from Spotify's Dashboard.
2. In the Spotify's Dashboard set the Redirect URI.
3. Set Client ID and Redirect URI in the `spotify_build_app_permissions_link.fish`.
4. Start the httpd.py: `python3 httpd.py`.
5. Use the `spotify_build_app_permissions_link.fish` script to build a link. Use that link
in a browser.
6. The server should get the `CODE`.
7. Update the `spot.fish` with all the credentials and do `fish spot.fish` to get the
`ACCESS_TOKEN` and `REFRESH_TOKEN`.

