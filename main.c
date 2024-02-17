
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
#include <locale.h>
#include <linux/joystick.h>
#include <pthread.h>

#include "game.h"

int wled_port = 21324;
static struct sockaddr_storage udp_sockaddr = { 0 };
static char wled_ip_new[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)] = { 0 };
const char *wled_ds = NULL;
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
    &announce_game,
    &snake_game,
    &tetris_game,
};
static int cur_game = 0;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static bool async_announce = false;
static char *async_announce_text = NULL;
static unsigned int async_announce_color = COLOR_WHITE;
static unsigned int async_announce_bgcolor = COLOR_BLACK;
static int async_announce_rotate = 1;
static double async_announce_speed = 5.0;

static const struct game *get_game(void)
{
    size_t i;

    for (i = 0; i < ARRAY_LENGTH(games); i++) {
        if (! games[i]->playable && ! games[i]->idle_func()) {
            return games[i];
        }
    }

    return games[cur_game];
}

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

    if (key_idx == KEYPAD_SELECT && key_val && get_game()->playable) {
        if (get_game()->deactivate_func) {
            get_game()->deactivate_func();
        }
        do {
            cur_game++;
            cur_game %= ARRAY_LENGTH(games);
        } while (! get_game()->playable);
        if (get_game()->activate_func) {
            fprintf(stderr, "starting game: %s\n", get_game()->name);
            get_game()->activate_func(true);
        }
    }

    if (get_game()->input_func) {
        get_game()->input_func(key_idx, key_val);
    }
}

static double get_time_val(void)
{
    struct timeval tv = { 0 };
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + ((double)tv.tv_usec / 1000000.0);
}

void do_announce(const char *text, unsigned int color, unsigned int bgcolor, int rotate, double speed)
{
    if (get_game()->idle_func()) {
        if (announce_game.idle_func()) {
            fprintf(stderr, "announcing text: %s\n", text);
            set_announce_text(text, color, bgcolor, rotate, speed);
            announce_game.activate_func(true);
        } else {
            fprintf(stderr, "not announcing text (announce already in progress): %s\n", text);
        }
    } else {
        fprintf(stderr, "not announcing text (game is active): %s\n", text);
    }
}

void do_announce_my_ip(void)
{
    char wled_ip_address[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)] = { 0 };
    char text[100 + MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN) + MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)] = { 0 };

    if (udp_sockaddr.ss_family == AF_UNSPEC)
        return;

    if (udp_sockaddr.ss_family == AF_INET) {
        if (! inet_ntop(AF_INET, &((struct sockaddr_in *)&udp_sockaddr)->sin_addr, wled_ip_address, sizeof(wled_ip_address)))
            return;
    } else if (udp_sockaddr.ss_family == AF_INET6) {
        if (! inet_ntop(AF_INET6, &((struct sockaddr_in6 *)&udp_sockaddr)->sin6_addr, wled_ip_address, sizeof(wled_ip_address)))
            return;
    } else {
        return;
    }

    snprintf(text, sizeof(text), "RPI: %s, WLED: %s", ip_address, wled_ip_address);
    do_announce(text, COLOR_BLUE, COLOR_BLACK, 1, 10.0);
}

void do_announce_async(char *text, unsigned int color, unsigned int bgcolor, int rotate, double speed)
{
    if (pthread_mutex_lock(&mutex) != 0)
        return;

    if (async_announce) {
        (void)pthread_mutex_unlock(&mutex);
        return;
    }

    async_announce = true;
    async_announce_text = text;
    async_announce_color = color;
    async_announce_bgcolor = bgcolor;
    async_announce_rotate = rotate;
    async_announce_speed = speed;

    (void)pthread_mutex_unlock(&mutex);
}

static void handle_announce_async(void)
{
    if (pthread_mutex_lock(&mutex) != 0)
        return;

    if (! async_announce) {
        (void)pthread_mutex_unlock(&mutex);
        return;
    }

    do_announce(async_announce_text, async_announce_color, async_announce_bgcolor, async_announce_rotate, async_announce_speed);

    async_announce = false;
    if (async_announce_text)
        free(async_announce_text);
    async_announce_text = NULL;
    async_announce_color = COLOR_WHITE;
    async_announce_bgcolor = COLOR_BLACK;
    async_announce_rotate = 1;
    async_announce_speed = 5.0;

    (void)pthread_mutex_unlock(&mutex);
}

void update_wled_ip(const char *address)
{
    if (pthread_mutex_lock(&mutex) != 0)
        return;

    strncpy(wled_ip_new, address, sizeof(wled_ip_new));

    (void)pthread_mutex_unlock(&mutex);
}

static void handle_wled_ip_async(void)
{
    int af = AF_UNSPEC;
    struct in_addr addr;
    struct in6_addr addr6;
    bool update = false;

    if (pthread_mutex_lock(&mutex) != 0)
        return;

    if (! *wled_ip_new) {
        (void)pthread_mutex_unlock(&mutex);
        return;
    }

    if (inet_pton(AF_INET, wled_ip_new, &addr)) {
        af = AF_INET;
    } else if (inet_pton(AF_INET6, wled_ip_new, &addr6)) {
        af = AF_INET6;
    } else {
        *wled_ip_new = '\0';
        (void)pthread_mutex_unlock(&mutex);
        return;
    }

    if (af != udp_sockaddr.ss_family) {
        if (af == AF_INET) {
            udp_sockaddr.ss_family = AF_INET;
            ((struct sockaddr_in *)&udp_sockaddr)->sin_addr = addr;
            ((struct sockaddr_in *)&udp_sockaddr)->sin_port = wled_port;
        } else if (af == AF_INET6) {
            udp_sockaddr.ss_family = AF_INET6;
            ((struct sockaddr_in6 *)&udp_sockaddr)->sin6_addr = addr6;
            ((struct sockaddr_in6 *)&udp_sockaddr)->sin6_port = wled_port;
        }
        update = true;
    } else if (af == AF_INET && memcmp(&addr, &((struct sockaddr_in *)&udp_sockaddr)->sin_addr, sizeof(addr))) {
        udp_sockaddr.ss_family = AF_INET;
        memcpy(&((struct sockaddr_in *)&udp_sockaddr)->sin_addr, &addr, sizeof(addr));
        ((struct sockaddr_in *)&udp_sockaddr)->sin_port = wled_port;
        update = true;
    } else if (af == AF_INET6 && memcmp(&addr6, &((struct sockaddr_in6 *)&udp_sockaddr)->sin6_addr, sizeof(addr6))) {
        udp_sockaddr.ss_family = AF_INET6;
        memcpy(&((struct sockaddr_in6 *)&udp_sockaddr)->sin6_addr, &addr6, sizeof(addr6));
        ((struct sockaddr_in6 *)&udp_sockaddr)->sin6_port = wled_port;
        update = true;
    }

    if (update) {
        fprintf(stderr, "using wled controller from mdns: %s\n", wled_ip_new);
        do_announce_my_ip();
    }

    *wled_ip_new = '\0';
    (void)pthread_mutex_unlock(&mutex);
}

static void usage(void)
{
    fprintf(stderr, "Usage: game WLED-IP WLED-PORT JOYSTICK\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    int opt;
    size_t i;

    if (argc != 4) {
        usage();
    }

    fprintf(stderr, "starting game controller\n");

    (void)setlocale(LC_ALL, "C.UTF-8");

    srand(time(NULL));

    wled_port = htons(atoi(argv[2]));

    memset(&udp_sockaddr, '\0', sizeof(udp_sockaddr));
    udp_sockaddr.ss_family = AF_UNSPEC;
    if (strncmp(argv[1], "mdns:", 5) == 0) {
        wled_ds = argv[1] + 5;
    } else {
        udp_sockaddr.ss_family = AF_INET;
        ((struct sockaddr_in *)&udp_sockaddr)->sin_port = wled_port;
        ((struct sockaddr_in *)&udp_sockaddr)->sin_addr.s_addr = inet_addr(argv[1]);
    }

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

    ip_init();
    fprintf(stderr, "my IP-address is: %s\n", ip_address);
    if (wled_ds) {
        mdns_init();
    }
    mqtt_init();

    for (i = 0; i < ARRAY_LENGTH(games); i++) {
        if (games[i]->init_func) {
            games[i]->init_func();
        }
    }

    start_time_val = get_time_val();
    last_tick_val = 0.0;
    ticks = 0;

    while (! get_game()->playable) {
        cur_game++;
        cur_game %= ARRAY_LENGTH(games);
    }

    if (get_game()->activate_func) {
        get_game()->activate_func(false);
    }

    do_announce_my_ip();

    for (;;) {
        handle_input();
        handle_announce_async();
        handle_wled_ip_async();

        time_val = get_time_val() - start_time_val;
        if (get_game()->tick_freq > 0.0 && get_game()->tick_freq <= 1.0) {
            while (time_val >= (last_tick_val + get_game()->tick_freq)) {
                last_tick_val += get_game()->tick_freq;
                ticks++;
                if (get_game()->tick_func) {
                    get_game()->tick_func();
                }
            }
        }

        display = false;
        if (get_game()->render_func) {
            udp_data[0] = WLED_DRGB;
            udp_data[1] = DISPLAY_TIMEOUT;
            get_game()->render_func(&display, udp_data + 2);
        }
        if (display) {
            if (udp_sockaddr.ss_family != AF_UNSPEC) {
                (void)sendto(udp_fd, udp_data, sizeof(udp_data), 0, (struct sockaddr *)&udp_sockaddr, sizeof(udp_sockaddr));
            }
        }

        usleep(100000);
    }
}
