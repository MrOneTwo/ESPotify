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

If you want to skip using `idf.py` you can first install the toolchain:

```sh
cd esp-idf
pip3 install -r requirements.txt
./install.sh
```

And then build the project:

```sh
source ~/esp/esp-idf/export.sh
mkdir build && cd build
cmake .. -G Ninja
ninja
```

## Features/TODO

### RFID

Currently, readers based on two chips are targetted: MFRC522, PN532. The listed PICC types aren't
100% covered whtn it comes to their functionality. If they are checked that means they work for
this projects.

- [ ] MFRC522 driver
  - [x] Mifare PICCs (ISO14443A) with 4 byte UID
  - [ ] Mifare PICCs (ISO14443A) with 7 byte UID (implemented; not tested)
  - [ ] Mifare PICCs (ISO14443A) with 10 byte UID (implemented; not tested)
  - [x] NTAG213 PICCs with 4 byte UID

- [ ] PN532
  - [ ] Mifare PICCs with 4 byte UID
  - [ ] Mifare PICCs with 7 byte UID (implemented; not tested)
  - [ ] Mifare PICCs with 10 byte UID (implemented; not tested)

### Spotify

- [x] Refreshing the access token
  - [x] Refreshing the access token only when previous expired

## FAQ

**Why ESP-IDF and not Arduino?**

Because I value performance and I like learning new things. This my first (serious) **ESP-IDF**
project. **Arduino** framework is boring - objectively true.

**Why custom MFRC522 driver?**

Because I couldn't find a quality driver for **ESP-IDF** which supports everything I need. It's
also a great learning experience to write your own driver.
