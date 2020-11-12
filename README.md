# ESPotify

This little experiment is about using RFID tags to control Spotify's playback (queue, etc.).

## Building

Follow ESP32's toolchain setup guide. After sourcing the `~/esp/esp-idf/export.sh`
you should be able to use the `idf.py` tool:

```
idf.py menuconfig
idf.py build
idf.py flash
```

The `idf.py menuconfig` step is there so you can set Spotify's client ID, client secret,
and refresh token. I'd like the ESP32 to be able to fetch the refresh token by itself
but I'm not there yet.

## Features/TODO

- [x] Support for tags with 4 byte UID
- [ ] Support for tags with 7 byte UID
- [ ] Support for tags with 10 byte UID
