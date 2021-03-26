import sys
import spidriver
from struct import *

PN532_PREAMBLE = 0x0
PN532_START_CODE1 = 0x0
PN532_START_CODE2 = 0xff
PN532_POSTAMBLE = 0x0

PN532_HOST_TO_PN532 = 0xd4
PN532_PN532_TO_HOST = 0xd5

PN532_CMD_GET_FIRMWARE_VERSION = 0x02

PN532_SPI_STAT_READ = 0x02
PN532_SPI_DATA_WRITE = 0x01
PN532_SPI_DATA_READ = 0x03
PN532_SPI_READY = 0x01

# The protocol:
# https://www.nxp.com/docs/en/user-guide/141520.pdf
# 
# - half duplex

class PN532():
  def __init__(self, spi: spidriver.SPIDriver):
    self.spi = spi

  def write_command(self, cmd):
    buf = pack('B', PN532_SPI_DATA_WRITE)

    buf += pack('B', PN532_PREAMBLE)
    buf += pack('B', PN532_START_CODE1)
    buf += pack('B', PN532_START_CODE2)
    # Data length plus TFI (Frame Identifier).
    buf += pack('B', len(cmd) + 1)
    # LCS which satisfies the relation (len(cmd) + 1) + LCS = 0
    buf += pack('b', -(len(cmd) + 1))
    TFI = PN532_HOST_TO_PN532
    buf += pack('B', TFI)

    for c in cmd:
      buf += pack('B', c)

    DCS = -TFI
    for c in cmd:
      DCS -= c

    buf += pack('B', (DCS & 0x000000ff))

    # DCS which satisfies the realtion TFI + PD0 + ... + PDn + DCS = 0
    buf += pack('B', PN532_POSTAMBLE)

    print(f"Writing command: {buf}")

    self.spi.sel()
    self.spi.write(bytes(buf))
    self.spi.unsel()



def com(port: str):
  spi = spidriver.SPIDriver(port)

  pn532 = PN532(spi)

  pn532.write_command([PN532_CMD_GET_FIRMWARE_VERSION])


  # spi.sel()
  # print(spi.writeread([PN532_SPI_STAT_READ]))
  # spi.unsel()


if __name__ == "__main__":
  if len(sys.argv) != 2:
    print("Please call this script with serial port as an argument!")
    quit()

  PORT = sys.argv[1]

  com(PORT)
