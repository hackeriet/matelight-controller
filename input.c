#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/joystick.h>
#include <libudev.h>

#include "matelight.h"

static struct joystick joysticks[MAX_JOYSTICKS] = { 0 };
static size_t num_joysticks = 0;

static struct udev *udev_ctx = NULL;

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
}

void init_joystick(const char *path)
{
    size_t i;
    int fd;
    int opt;
    int player = 1;

    if (! path)
        return;

    if (num_joysticks >= MAX_JOYSTICKS) {
        fprintf(stderr, "init_joystick: Max joysticks opened.\n");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < num_joysticks; i++) {
        if (joysticks[i].fd == -1)
            continue;

        if (joysticks[i].player == player) {
            player++;
            i = 0;
        }
    }

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

    joysticks[num_joysticks].fd = fd;
    joysticks[num_joysticks].player = player;
    joysticks[num_joysticks].key_state = 0;
    joysticks[num_joysticks].last_key_idx = KEYPAD_NONE;
    joysticks[num_joysticks].last_key_val = false;
    memset(joysticks[num_joysticks].key_history, '\0', sizeof(joysticks[num_joysticks].key_history));

    num_joysticks++;
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

bool read_joystick(struct joystick **joystick_ptr)
{
    size_t i;
    struct joystick *joystick = NULL;
    struct js_event event = { 0 };
    int key_idx = KEYPAD_NONE;

    if (num_joysticks <= 0)
        return NULL;

    for (i = 0; i < num_joysticks; i++) {
        if (joysticks[i].fd == -1)
            continue;

        if (read(joysticks[i].fd, &event, sizeof(event)) == sizeof(event)) {
            joystick = &joysticks[i];
            break;
        }
    }

    if (! joystick)
        return NULL;

    switch (event.type & ~JS_EVENT_INIT) {
        case JS_EVENT_BUTTON:
            switch (event.number) {
                case 8:
                    key_idx = KEYPAD_SELECT;
                    break;
                case 9:
                    key_idx = KEYPAD_START;
                    break;
                case 0:
                    key_idx = KEYPAD_B;
                    break;
                case 1:
                    key_idx = KEYPAD_A;
                    break;
                default:
                    break;
            }
            if (key_idx != KEYPAD_NONE) {
                joystick->last_key_idx = key_idx;
                joystick->last_key_val = !!event.value;
                if (joystick->last_key_val) {
                    joystick->key_state |= key_idx;
                } else {
                    joystick->key_state &= ~key_idx;
                }
            } else {
                joystick->last_key_idx = KEYPAD_NONE;
                joystick->last_key_val = false;
            }
            break;
        case JS_EVENT_AXIS:
            switch (event.number) {
                case 0:
                    if (event.value <= -1024) {
                        joystick->last_key_idx = KEYPAD_LEFT;
                        joystick->last_key_val = true;
                        joystick->key_state |= KEYPAD_LEFT;
                        joystick->key_state &= ~KEYPAD_RIGHT;
                    } else if (event.value >= 1024) {
                        joystick->last_key_idx = KEYPAD_RIGHT;
                        joystick->last_key_val = true;
                        joystick->key_state &= ~KEYPAD_LEFT;
                        joystick->key_state |= KEYPAD_RIGHT;
                    } else {
                        if (joystick->key_state & KEYPAD_LEFT) {
                            joystick->last_key_idx = KEYPAD_LEFT;
                        } else if (joystick->key_state & KEYPAD_RIGHT) {
                            joystick->last_key_idx = KEYPAD_RIGHT;
                        } else {
                            joystick->last_key_idx = KEYPAD_NONE;
                        }
                        joystick->last_key_val = false;
                        joystick->key_state &= ~KEYPAD_LEFT;
                        joystick->key_state &= ~KEYPAD_RIGHT;
                    }
                    break;
                case 1:
                    if (event.value <= -1024) {
                        joystick->last_key_idx = KEYPAD_UP;
                        joystick->last_key_val = true;
                        joystick->key_state |= KEYPAD_UP;
                        joystick->key_state &= ~KEYPAD_DOWN;
                    } else if (event.value >= 1024) {
                        joystick->last_key_idx = KEYPAD_DOWN;
                        joystick->last_key_val = true;
                        joystick->key_state &= ~KEYPAD_UP;
                        joystick->key_state |= KEYPAD_DOWN;
                    } else {
                        if (joystick->key_state & KEYPAD_UP) {
                            joystick->last_key_idx = KEYPAD_UP;
                        } else if (joystick->key_state & KEYPAD_DOWN) {
                            joystick->last_key_idx = KEYPAD_DOWN;
                        } else {
                            joystick->last_key_idx = KEYPAD_NONE;
                        }
                        joystick->key_state &= ~KEYPAD_UP;
                        joystick->key_state &= ~KEYPAD_DOWN;
                    }
                    break;
                default:
                    joystick->last_key_idx = KEYPAD_NONE;
                    joystick->last_key_val = false;
                    break;
            }
            break;
        default:
            joystick->last_key_idx = KEYPAD_NONE;
            joystick->last_key_val = false;
            break;
    }

    if (joystick->last_key_idx != KEYPAD_NONE && joystick->last_key_val) {
        memmove(&joystick->key_history[1], &joystick->key_history[0], sizeof(joystick->key_history) - sizeof(joystick->key_history[0]));
    }

    *joystick_ptr = joystick;
    return true;
}

bool joystick_is_key_seq(struct joystick *joystick, const int *seq, size_t seq_length)
{
    size_t hi, si;

    if (! joystick)
        return false;

    if (seq_length > KEY_HISTORY_SIZE)
        return false;

    for (hi = seq_length - 1, si = 0; si < seq_length; hi--, si++) {
        if (joystick->key_history[hi] != seq[si])
            return false;
    }

    return true;
}
