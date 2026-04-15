# Raw Display (rawd) helper

A minimal helper for outputing raw frames to the RP-HUB75 display over USB CDC.

It works by piping raw frames from an input file descriptor and inserting all required sync and config boilerplate into the output stream.
Tested on Linux and may possibly work on other unix derivitives like BSDs and Macs.

## Usage

The helper can be compiled directly by your system cc:
```sh
cd sw/rawd/
gcc -o rawd rawd.c
```

To use it, point the helper to the target CDC block device, the source file, source pixel format and target refresh rate:
```sh
# pipe a raw nv12 video file to the display
./rawd /dev/ttyACM0 vid.bin nv12 60

# pipe a rgb565 video stream from stdin
... | ./rawd /dev/ttyACM0 - rgb565 60
```

> [!warning]
> On linux, the ACM block devices are by default only accesible to the root. You will need to modify your udev rules to allow usage by a non-superuser.

## ffmpeg examples

Generating a raw video stream is not the most straigth forward thing, but `ffmpeg` makes it pretty simple.

> [!note]
> Some params might need to be modified for your setup. If ffmpeg is failing to start streaming, check if all block devices exist.

Play an input video using RGB888 at 60Hz:
```sh
ffmpeg -i [input file] -vcodec rawvideo -f rawvideo -vf "scale=64:64, format=rgb24" - | ./rawd /dev/ttyACM0 - rgb888 60
```

Play an input video rotated by 90° using NV12 at 60Hz:
```sh
ffmpeg -i [input file] -vcodec rawvideo -f rawvideo -vf "scale=64:64, transpose=1, format=nv12" - | ./rawd /dev/ttyACM0 - nv12 60
```

Play an input video with audio on the source PC (default output) at 60Hz: (Linux only)
```sh
ffmpeg -i [input file] -f alsa default -vcodec rawvideo -f rawvideo -vf "scale=64:64, transpose=1, format=nv12" - | ./rawd /dev/ttyACM0 - nv12 60
```

Capture desktop and stream to the display at 60Hz: (Linux **only**, requires root, AMD/Intel GPU only)
```sh
sudo ffmpeg -device /dev/dri/card1 -f kmsgrab -r 60 -i - -vf 'hwmap=derive_device=vaapi, scale_vaapi=w=128:h=128:format=nv12, hwdownload, format=nv12, scale=64:64, transpose=1' -vcodec rawvideo -f rawvideo - | ./rawd /dev/ttyACM0 - nv12 60
```
