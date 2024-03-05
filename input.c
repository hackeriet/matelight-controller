#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/joystick.h>
#include <libudev.h>

#include "matelight.h"

static int js_fd = -1;
static struct udev *udev_ctx = NULL;

int key_state = 0;

void input_reset(void)
{
    if (js_fd != -1) {
        close(js_fd);
        js_fd = -1;
    }
    if (udev_ctx != NULL) {
        udev_unref(udev_ctx);
        udev_ctx = NULL;
    }

    key_state = 0;
}

void init_joystick(const char *path)
{
    int opt;

    input_reset();

    if (path) {
        js_fd = open(path, O_RDONLY);
        if (js_fd == -1) {
            perror(path);
            exit(EXIT_FAILURE);
        }

        opt = fcntl(js_fd, F_GETFL);
        if (opt >= 0) {
            opt |= O_NONBLOCK;
            fcntl(js_fd, F_SETFL, opt);
        }
    }
}

void init_udev_hotplug(void)
{
    input_reset();

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
    struct js_event event;

    *key_idx = KEYPAD_NONE;
    *key_val = false;

    if (js_fd == -1)
        return false;

    if (read(js_fd, &event, sizeof(event)) != sizeof(event))
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
