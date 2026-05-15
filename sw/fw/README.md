# Saturn Ring (RP-HUB75) Firmware

Firmware for the Saturn display driver board.

Currently, only USB CDC and QSPI (soon) are supported for video input. For more detailed info and the video protocol, see `src/proto.h`.

> [!note]
> To use the display board in the USB CDC mode, see `sw/rawd` for how to interact with it.

## Building

> [!note]
> You will need to have `pico-sdk` setup on your system and `PICO_SDK_PATH` must be properly exported.

To build the firmware package in the USB mode, execute:
```sh
cmake -B build/ . -G Ninja -DCMAKE_BUILD_TYPE=Release -DSR_INPUT_USB=ON
ninja -C build/
```

To flash, you can either enter the rp2350 bootsel and upload a u2f binary package located at `build/src/disp.uf2`.

Alternatively you can flash at any time using `picotool`:
```sh
picotool load build/src/disp.elf -fux
```
