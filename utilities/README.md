1. Start the httpd.py: `python3 httpd.py`.
2. Use the `spotify_build_app_permissions_link.fish` script to build a link. Use that link
in a browser. Remember to paste the `CLIENT_ID` and the `REDIRECT_URI` in that script.
3. The server should get the `CODE`.
4. Update the `spot.fish` with all the credentials and do `fish spot.fish` to get the
`ACCESS_TOKEN` and `REFRESH_TOKEN`.

