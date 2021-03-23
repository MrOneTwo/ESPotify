This is the test application - a basic app which gets all the components tests compiled in and, when
uploaded to the board, runs through them.

Testing here is pretty difficult since A LOT of functionality here depends on the RFID reader and
the PICC present in the reader's RF field. Treat it as an excuse for why the code isn't well tested.

When in this directory:

```
idf.py flash
idf.py monitor
```

Change the RFID reader in the *main/CMakeLists.txt*:

```
idf_build_set_property(COMPILE_DEFINITIONS -DCONFIG_RC522 APPEND)
```
