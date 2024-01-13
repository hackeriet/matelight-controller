
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/joystick.h>

#include "game.h"

// VGA Palette
const char vga_palette[16 * 3] = {
    0x00, 0x00, 0x00,   /* Black */
    0x00, 0x00, 0x80,   /* Blue */
    0x00, 0x80, 0x00,   /* Green */
    0x00, 0x80, 0x80,   /* Cyan */
    0x80, 0x00, 0x00,   /* Red */
    0x80, 0x00, 0x80,   /* Magenta */
    0x80, 0x80, 0x00,   /* Brown */
    0xc0, 0xc0, 0xc0,   /* Light Gray */
    0x80, 0x80, 0x80,   /* Dark Gray */
    0x00, 0x00, 0xff,   /* Light Blue */
    0x00, 0xff, 0x00,   /* Light Green */
    0x00, 0xff, 0xff,   /* Light Cyan */
    0xff, 0x00, 0x00,   /* Light Red */
    0xff, 0x00, 0xff,   /* Light Magenta */
    0xff, 0xff, 0x00,   /* Yellow */
    0xff, 0xff, 0xff,   /* White */
};

static struct sockaddr_in udp_sockaddr = { 0 };
static int udp_fd = -1;
static int js_fd = -1;

static double start_time_val = 0.0;
double time_val = 0.0;
static double last_tick_val = 0.0;
int ticks = 0;
int key_state = 0;

static bool display = false;
static char udp_data[2 + (GRID_WIDTH * GRID_HEIGHT * 3)] = { 0 };

static const struct game *games[] = {
    &snake_game,
    &tetris_game,
};
static int cur_game = 1;

static void handle_input(void)
{
    struct js_event event;
    int key_idx = KEYPAD_NONE;
    bool key_val = false;

    if (read(js_fd, &event, sizeof(event)) != sizeof(event))
        return;

    switch (event.type) {
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
                    key_idx = KEYPAD_NONE;
                    break;
            }
            if (key_idx != KEYPAD_NONE) {
                key_val = !!event.value;
                if (key_val) {
                    key_state |= key_idx;
                } else {
                    key_state &= ~key_idx;
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

    if (games[cur_game]->input_func) {
        games[cur_game]->input_func(key_idx, key_val);
    }
}

static double get_time_val(void)
{
    struct timeval tv = { 0 };
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + ((double)tv.tv_usec / 1000000.0);
}

static void usage(void)
{
    fprintf(stderr, "Usage: game IP PORT JOYSTICK\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    int opt;
    size_t i;

    if (argc != 4) {
        usage();
    }

    srand(time(NULL));

    memset(&udp_sockaddr, '\0', sizeof(udp_sockaddr));
    udp_sockaddr.sin_family = AF_INET;
    udp_sockaddr.sin_port = htons(atoi(argv[2]));
    udp_sockaddr.sin_addr.s_addr = inet_addr(argv[1]);

    udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    js_fd = open(argv[3], O_RDONLY);
    if (js_fd == -1) {
        perror(argv[3]);
        exit(EXIT_FAILURE);
    }

    opt = fcntl(js_fd, F_GETFL);
    if (opt >= 0) {
        opt |= O_NONBLOCK;
        fcntl(js_fd, F_SETFL, opt);
    }

    for (i = 0; i < ARRAY_LENGTH(games); i++) {
        if (games[i]->init_func) {
            games[i]->init_func();
        }
    }

    start_time_val = get_time_val();
    last_tick_val = 0.0;
    ticks = 0;

    if (games[cur_game]->activate_func) {
        games[cur_game]->activate_func(false);
    }

    for (;;) {
        handle_input();

        time_val = get_time_val() - start_time_val;
        if (games[cur_game]->tick_freq > 0.0 && games[cur_game]->tick_freq <= 1.0) {
            while (time_val >= (last_tick_val + games[cur_game]->tick_freq)) {
                last_tick_val += games[cur_game]->tick_freq;
                ticks++;
                if (games[cur_game]->tick_func) {
                    games[cur_game]->tick_func();
                }
            }
        }

        display = false;
        if (games[cur_game]->render_func) {
            udp_data[0] = WLED_DRGB;
            udp_data[1] = DISPLAY_TIMEOUT;
            games[cur_game]->render_func(&display, udp_data + 2);
        }
        if (display) {
            (void)sendto(udp_fd, udp_data, sizeof(udp_data), 0, (struct sockaddr *)&udp_sockaddr, sizeof(udp_sockaddr));
        }

        usleep(100000);
    }
}
