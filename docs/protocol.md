# Protocol

Board comunicates with the control board over SPI. 

Display board is **slave**, controling board is **host**,

## Command code

8-bit value determining the command

| Code     | Description |
| -------- | ----------- |
| 0x00     | Ignore      |
| 0x0X     | Ping        |
| 0x1X     | Screen      |
| 0x2X     | USB         |

## Packet lenght

16-bit / uint16 number determining the lenght in bytes of the comming data (not including the command code or packet lenght)

**TODO** - little/big endian

## Data

Predetermined lenght of bytes representing what should be done / what happened. For example the display buffer or usb device feedback.

## Padding (optional)

Few bytes as a padding to make sure the recieving device recieves the correct amount of data