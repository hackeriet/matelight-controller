#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/joystick.h>
#include <libudev.h>

#include "matelight.h"

#define MAX_JOYSTICKS 64

struct joystick {
    int fd;
};

static struct joystick joysticks[MAX_JOYSTICKS] = { 0 };
static size_t num_joysticks = 0;

static struct udev *udev_ctx = NULL;

int key_state = 0;

void input_reset(void)
{
    size_t i;

    for (i = 0; i < num_joysticks; i++) {
        if (joysticks[i].fd != -1) {
            close(joysticks[i].fd);
            joysticks[i].fd = -1;
        }
    }
    num_joysticks = 0;

    if (udev_ctx != NULL) {
        udev_unref(udev_ctx);
        udev_ctx = NULL;
    }

    key_state = 0;
}

void init_joystick(const char *path)
{
    int fd;
    int opt;

    if (path) {
        fd = open(path, O_RDONLY);
        if (fd == -1) {
            perror(path);
            exit(EXIT_FAILURE);
        }

        opt = fcntl(fd, F_GETFL);
        if (opt >= 0) {
            opt |= O_NONBLOCK;
            fcntl(fd, F_SETFL, opt);
        }

        if (num_joysticks >= MAX_JOYSTICKS) {
            fprintf(stderr, "init_joystick: Max joysticks opened.\n");
            close(fd);
            exit(EXIT_FAILURE);
        }

        joysticks[num_joysticks].fd = fd;
        num_joysticks++;
    }
}

void init_udev_hotplug(void)
{
    if (udev_ctx != NULL) {
        udev_unref(udev_ctx);
        udev_ctx = NULL;
    }

    udev_ctx = udev_new();

    if (! udev_ctx) {
        fprintf(stderr, "init_udev_hotplug: Unable to initialize udev.\n");
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "init_udev_hotplug: Not implemented.\n");
    exit(EXIT_FAILURE);
}

bool read_joystick(int *key_idx, bool *key_val)
{
    size_t i;
    bool has_event = false;
    struct js_event event = { 0 };

    *key_idx = KEYPAD_NONE;
    *key_val = false;

    if (num_joysticks <= 0)
        return false;

    for (i = 0; i < num_joysticks; i++) {
        if (joysticks[i].fd == -1)
            continue;

        if (read(joysticks[i].fd, &event, sizeof(event)) == sizeof(event)) {
            has_event = true;
            break;
        }
    }

    if (! has_event)
        return false;

    switch (event.type) {
        case JS_EVENT_BUTTON:
            switch (event.number) {
                case 8:
                    *key_idx = KEYPAD_SELECT;
                    break;
                case 9:
                    *key_idx = KEYPAD_START;
                    break;
                case 0:
                    *key_idx = KEYPAD_B;
                    break;
                case 1:
                    *key_idx = KEYPAD_A;
                    break;
                default:
                    break;
            }
            if (*key_idx != KEYPAD_NONE) {
                *key_val = !!event.value;
                if (*key_val) {
                    key_state |= *key_idx;
                } else {
                    key_state &= ~*key_idx;
                }
            }
            break;
        case JS_EVENT_AXIS:
            switch (event.number) {
                case 0:
                    if (event.value <= -1024) {
                        key_state |= KEYPAD_LEFT;
                        key_state &= ~KEYPAD_RIGHT;
                    } else if (event.value >= 1024) {
                        key_state &= ~KEYPAD_LEFT;
                        key_state |= KEYPAD_RIGHT;
                    } else {
                        key_state &= ~KEYPAD_LEFT;
                        key_state &= ~KEYPAD_RIGHT;
                    }
                    break;
                case 1:
                    if (event.value <= -1024) {
                        key_state |= KEYPAD_UP;
                        key_state &= ~KEYPAD_DOWN;
                    } else if (event.value >= 1024) {
                        key_state &= ~KEYPAD_UP;
                        key_state |= KEYPAD_DOWN;
                    } else {
                        key_state &= ~KEYPAD_UP;
                        key_state &= ~KEYPAD_DOWN;
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }

    return true;
}
