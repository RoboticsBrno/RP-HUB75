#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "../fw/src/proto.h"

/* Saturn display over USB CDC driver */

// Serial device helpers //

static int rawd_open_fd(const char *device_path) {
    // open serial device //

    int fd = open(device_path, O_RDWR, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0)
        return fd;

    // switch the device into raw mode //

    struct termios tty;
    if (tcgetattr(fd, &tty)) {
        printf("failed to tcgetattr for device: %s", strerror(errno));

        close(fd);
        return -1;
    }

    cfmakeraw(&tty);

    if (tcsetattr(fd, TCSANOW, &tty)) {
        printf("failed to tcsetattr for device: %s", strerror(errno));

        close(fd);
        return -1;
    }

    return fd;
}

static int rawd_write_all(const int fd, const void *buf, const size_t len) {
    const char *data = buf;
    size_t written = 0;
    ssize_t n;

    while (written < len) {
        n = write(fd, data + written, len - written);
        if (n > 0) {
            written += n;
        } else if (n != -1) {
            /* C library bug, should never occur */
            errno = EIO;
            return -1;
        } else {
            /* Error; n == -1, so errno is already set. */
            return -1;
        }
    }

    /* Success. */
    return 0;
}

static int rawd_read_all(const int fd, void *buf, const size_t len) {
    char *const ptr = buf;
    size_t have = 0;
    ssize_t n;

    /* This function is to be used with half-duplex query-response protocol,
       so make sure we have transmitted everything before trying to
       receive a response. Also assumes c_cc[VTIME] is properly set for
       both the first byte of the response, and interbyte response interval
       in deciseconds. */
    tcdrain(fd);

    while (have < len) {
        n = read(fd, ptr + have, len - have);
        if (n > 0) {
            have += n;
        } else if (n == 0) {
            /* Timeout or disconnect */
            errno = ETIMEDOUT;
            return -1;
        } else if (n != -1) {
            /* C library bug, should never occur */
            errno = EIO;
            return -1;
        } else {
            /* Read error; errno set by read(). */
            return -1;
        }
    }

    /* Success; no errors. */
    return 0;
}

// Output driver //

struct rawd_display {
    int fd;

    uint32_t frame_interval;
    struct disp_mode mode;
};

static uint32_t rawd_get_frame_size(struct disp_mode *mode) {
    switch (mode->format) {
    case DISP_FORMAT_RGB565:
        return mode->width * 64 * 2;

    case DISP_FORMAT_RGB888:
        return mode->width * 64 * 2;

    case DISP_FORMAT_NV12:
        return mode->width * 64 + (mode->width / 2 * 64 / 2) * 2;

    default:
        return 0; // unsupported format
    }
}

static void rawd_submit_sync(struct rawd_display *disp) {
    rawd_write_all(disp->fd, disp_sync_seq, sizeof(disp_sync_seq));
    rawd_write_all(disp->fd, &disp->mode, sizeof(disp->mode));
}

/*
 * Pipe frames from a srouce file descriptor.
 *
 * All frames are submited using the disp->mode config and will block until
 * the soruce or device desciptors cause an error or end-of-file.
 */

static void rawd_pipe_frames(struct rawd_display *disp, int src_fd) {
    const uint32_t frame_size = rawd_get_frame_size(&disp->mode);
    if (!frame_size)
        return;

    // allocate a buffer for frame submition
    void *fb = malloc(frame_size);

    // sample current time for frame pacing
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);

    while (true) {
        // pipe next frame

        int n = rawd_read_all(src_fd, fb, frame_size);
        if (n < 0)
            break;

        rawd_submit_sync(disp);
        rawd_write_all(disp->fd, fb, frame_size);

        tcdrain(disp->fd);

        // await for the frame interval

        t.tv_nsec += disp->frame_interval;
        if (t.tv_nsec >= 1000000000) {
            t.tv_nsec -= 1000000000;
            t.tv_sec += 1;
        }

        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);
    }

    // clean up
    free(fb);
}

int main(int argc, char **argv) {
    if (argc != 5) {
        printf("rawd usage: %s [dev path] [src path] [format] [refresh rate]\n", argv[0]);
        exit(-1);
    }

    // open device fd //

    struct rawd_display device = {};
    device.fd = rawd_open_fd(argv[1]);

    if (device.fd < 0) {
        printf("rawd: failed to open display fd: %s\n", strerror(errno));
        exit(-1);
    }

    // open source fd //

    int input_fd = -1;
    if (argc < 2 || strcmp(argv[2], "-") == 0) {
        input_fd = STDIN_FILENO;
    } else {
        input_fd = open(argv[2], O_RDONLY);
    }

    if (input_fd < 0) {
        printf("rawd: failed to open %s: %s\n", argv[2], strerror(errno));
        exit(-1);
    }

    // setup mode //

    if (strcmp(argv[3], "rgb565") == 0) {
        device.mode.format = DISP_FORMAT_RGB565;
    } else if (strcmp(argv[3], "rgb888") == 0) {
        device.mode.format = DISP_FORMAT_RGB888;
    } else if (strcmp(argv[3], "nv12") == 0) {
        device.mode.format = DISP_FORMAT_NV12;
    } else {
        printf("rawd: unknown format %s\n", argv[3]);
        exit(-1);
    }

    int freq = atoi(argv[4]);
    device.frame_interval = 1000000000 / freq;

    device.mode.brightness = 255;
    device.mode.width = 64;
    device.mode.magic = disp_mode_magic_value;

    // pipe frames //
    rawd_pipe_frames(&device, input_fd);
}
