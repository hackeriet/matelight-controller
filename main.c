
#include <stddef.h>
#include <stdbool.h>
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
#include <getopt.h>
#include <pthread.h>

#include "matelight.h"

int grid_width = DEFAULT_GRID_WIDTH;
int grid_height = DEFAULT_GRID_HEIGHT;
bool grid_widescreen = true;
static char *address = NULL;
static int wled_port = 21324;
static char *mdns_description = NULL;
static char *joypad_dev = NULL;
static bool joypad_udev = false;
static bool keyboard = false;
static int start_game = -1;
static bool start_on_startup = false;
static bool debug = false;
static bool mqtt = false;

static struct sockaddr_storage udp_sockaddr = { 0 };
static char wled_ip_new[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)] = { 0 };
const char *wled_ds = NULL;
static int udp_fd = -1;

static int joystick_cnt = 0;

static double start_time_val = 0.0;
double time_val = 0.0;
static double last_tick_val = 0.0;
int ticks = 0;

static bool display = false;
//static char udp_data[65536];
static char udp_data[2 + (MAX_GRID_SIZE * 3)] = { 0 };

#define UDP_DATA_SIZE (2 + (grid_width * grid_height * 3))

static const struct game *games[] = {
    &debug_game,
    &announce_game,
    &snake_game,
    &tetris_game,
    &flappy_game,
    &pong_game,
    &breakout_game,
    &invaders_game,
};
static int cur_game = 0;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static bool async_announce = false;
static char *async_announce_text = NULL;
static unsigned int async_announce_color = COLOR_WHITE;
static unsigned int async_announce_bgcolor = COLOR_BLACK;
static double async_announce_speed = 5.0;

static const int konami_code[] = {
    KEYPAD_UP,
    KEYPAD_UP,
    KEYPAD_DOWN,
    KEYPAD_DOWN,
    KEYPAD_LEFT,
    KEYPAD_RIGHT,
    KEYPAD_LEFT,
    KEYPAD_RIGHT,
    KEYPAD_B,
    KEYPAD_A,
    KEYPAD_START
};

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
    int new_joystick_cnt = 0;
    char text[100] = { 0 };
    struct joystick *joystick = NULL;

    while (read_joystick(&joystick)) {
        if (joystick->last_key_idx == KEYPAD_SELECT && joystick->last_key_val && get_game()->playable && (! get_game()->non_interruptable)) {
            if (get_game()->deactivate_func) {
                get_game()->deactivate_func();
            }
            if (joystick->key_state & KEYPAD_START) {
                fprintf(stderr, "starting debug game\n");
                debug_game.activate_func(true);
            } else {
                do {
                    cur_game++;
                    cur_game %= ARRAY_LENGTH(games);
                } while (! get_game()->playable);
                if (get_game()->activate_func) {
                    fprintf(stderr, "starting game: %s\n", get_game()->name);
                    get_game()->activate_func(true);
                }
            }
        }

        if (joystick_is_key_seq(joystick, konami_code, ARRAY_LENGTH(konami_code))) {
            fprintf(stderr, "konami code activated\n");
            if (get_game()->deactivate_func) {
                get_game()->deactivate_func();
            }
            if (get_game()->activate_func) {
                get_game()->activate_func(false);
            }
            do_announce("HACK THE PLANET", COLOR_BLACK, COLOR_YELLOW, 10.0);
        }

        if (get_game()->input_func) {
            get_game()->input_func(joystick->player, joystick->last_key_idx, joystick->last_key_val, joystick->key_state);
        }
    }

    new_joystick_cnt = count_joysticks();
    if (joystick_cnt != new_joystick_cnt) {
        if (new_joystick_cnt == 1) {
            snprintf(text, sizeof(text), "1 joypad");
        } else {
            snprintf(text, sizeof(text), "%d joypads", new_joystick_cnt);
        }
        if (new_joystick_cnt > joystick_cnt) {
            do_announce(text, COLOR_GREEN, COLOR_BLACK, 10.0);
        } else {
            do_announce(text, COLOR_RED, COLOR_BLACK, 10.0);
        }
        joystick_cnt = new_joystick_cnt;
    }
}

static double get_time_val(void)
{
    struct timeval tv = { 0 };
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + ((double)tv.tv_usec / 1000000.0);
}

void do_announce(const char *text, unsigned int color, unsigned int bgcolor, double speed)
{
    if (get_game()->idle_func()) {
        if (announce_game.idle_func()) {
            fprintf(stderr, "announcing text: %s\n", text);
            set_announce_text(text, color, bgcolor, speed);
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
    do_announce(text, COLOR_BLUE, COLOR_BLACK, 10.0);
}

void do_announce_async(char *text, unsigned int color, unsigned int bgcolor, double speed)
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

    do_announce(async_announce_text, async_announce_color, async_announce_bgcolor, async_announce_speed);

    async_announce = false;
    if (async_announce_text)
        free(async_announce_text);
    async_announce_text = NULL;
    async_announce_color = COLOR_WHITE;
    async_announce_bgcolor = COLOR_BLACK;
    async_announce_speed = 5.0;

    (void)pthread_mutex_unlock(&mutex);
}

void update_wled_ip(const char *address)
{
    if (pthread_mutex_lock(&mutex) != 0)
        return;

    strncpy(wled_ip_new, address, sizeof(wled_ip_new));
    wled_ip_new[sizeof(wled_ip_new) - 1] = '\0';

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
            ((struct sockaddr_in *)&udp_sockaddr)->sin_port = htons(wled_port);
        } else if (af == AF_INET6) {
            udp_sockaddr.ss_family = AF_INET6;
            ((struct sockaddr_in6 *)&udp_sockaddr)->sin6_addr = addr6;
            ((struct sockaddr_in6 *)&udp_sockaddr)->sin6_port = htons(wled_port);
        }
        update = true;
    } else if (af == AF_INET && memcmp(&addr, &((struct sockaddr_in *)&udp_sockaddr)->sin_addr, sizeof(addr))) {
        udp_sockaddr.ss_family = AF_INET;
        memcpy(&((struct sockaddr_in *)&udp_sockaddr)->sin_addr, &addr, sizeof(addr));
        ((struct sockaddr_in *)&udp_sockaddr)->sin_port = htons(wled_port);
        update = true;
    } else if (af == AF_INET6 && memcmp(&addr6, &((struct sockaddr_in6 *)&udp_sockaddr)->sin6_addr, sizeof(addr6))) {
        udp_sockaddr.ss_family = AF_INET6;
        memcpy(&((struct sockaddr_in6 *)&udp_sockaddr)->sin6_addr, &addr6, sizeof(addr6));
        ((struct sockaddr_in6 *)&udp_sockaddr)->sin6_port = htons(wled_port);
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
    fprintf(stderr, "Usage: matelight [options]\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -W, --width\t\t\tgrid witdh\n");
    fprintf(stderr, "  -H, --height\t\t\tgrid height\n");
    fprintf(stderr, "  -a, --address\t\t\tWLED address\n");
    fprintf(stderr, "  -p, --port\t\t\tWLED port\n");
    fprintf(stderr, "  -m, --mdns-description\tWLED MDNS description\n");
    fprintf(stderr, "  -j, --joystick-device\t\tjoystick device\n");
    fprintf(stderr, "  -u, --udev-hotplug\t\thotpluggable joystick devices\n");
    fprintf(stderr, "  -k, --keyboard\t\tkeyboard input\n");
    fprintf(stderr, "  -g, --game\t\t\tgame name\n");
    fprintf(stderr, "  -d, --debug\t\t\tdebug mode\n");
    fprintf(stderr, "  -S, --start\t\t\tstart game on startup\n");
    fprintf(stderr, "  -M, --mqtt\t\t\tenable MQTT\n");
    fprintf(stderr, "  -h, --help\t\t\thelp\n");
    exit(EXIT_FAILURE);
}

static struct option long_options[] = {
    {"width",               required_argument,  NULL,   'W'},
    {"height",              required_argument,  NULL,   'H'},
    {"address",             required_argument,  NULL,   'a'},
    {"port",                required_argument,  NULL,   'p'},
    {"mdns-description",    required_argument,  NULL,   'm'},
    {"joystick-device",     required_argument,  NULL,   'j'},
    {"udev-hotplug",        no_argument,        NULL,   'u'},
    {"keyboard",            no_argument,        NULL,   'k'},
    {"game",                required_argument,  NULL,   'g'},
    {"start",               no_argument,        NULL,   'S'},
    {"debug",               no_argument,        NULL,   'd'},
    {"mqtt",                no_argument,        NULL,   'M'},
    {"help",                no_argument,        NULL,   'h'},
    {NULL,                  0,                  NULL,   0}
};

int main(int argc, char *argv[])
{
    int c;
    size_t i;

    for (;;) {
        c = getopt_long(argc, argv, "W:H:a:p:m:j:ukg:dSMh", long_options, NULL);
        if (c == -1)
            break;

        switch (c) {
            case 'W':
                grid_width = atoi(optarg);
                if (grid_width < MIN_GRID_WIDTH || grid_width > MAX_GRID_WIDTH) {
                    fprintf(stderr, "Grid width must be within %d and %d\n", MIN_GRID_WIDTH, MAX_GRID_WIDTH);
                    usage();
                }
                break;

            case 'H':
                grid_height = atoi(optarg);
                if (grid_height < MIN_GRID_HEIGHT || grid_height > MAX_GRID_HEIGHT) {
                    fprintf(stderr, "Grid height must be within %d and %d\n", MIN_GRID_HEIGHT, MAX_GRID_HEIGHT);
                    usage();
                }
                break;

            case 'a':
                address = optarg;
                break;

            case 'p':
                wled_port = atoi(optarg);
                if (wled_port <= 0 || wled_port >= 65536) {
                    fprintf(stderr, "WLED port must be within 1 and 65535\n");
                    usage();
                }
                break;

            case 'm':
                mdns_description = optarg;
                break;

            case 'j':
                joypad_dev = optarg;
                break;

            case 'u':
                joypad_udev = true;
                break;

            case 'k':
                keyboard = true;
                break;

            case 'g':
                start_game = -1;
                for (i = 0; i < ARRAY_LENGTH(games); i++) {
                    if (strcmp(games[i]->name, optarg) == 0) {
                        start_game = i;
                        break;
                    }
                }
                if (start_game == -1) {
                    fprintf(stderr, "Game \"%s\" not found.\n", optarg);
                    usage();
                }
                break;

            case 'S':
                start_on_startup = true;
                break;

            case 'd':
                debug = true;
                break;

            case 'M':
                mqtt = true;
                break;

            case 'h':
            case '?':
            default:
                usage();
                break;
        }
    }

    if (optind < argc)
        usage();

    grid_widescreen = (grid_width > grid_height || (grid_width >= 16 && grid_height >= 10));

    if (! address && ! mdns_description) {
        fprintf(stderr, "Either WLED address or WLED MDNS description must be specified.\n");;
        usage();
    }

    if ((joypad_dev && joypad_udev) || (joypad_dev && keyboard) || (joypad_udev && keyboard)) {
        fprintf(stderr, "Either joystick device, hotpluggable joystick mode or keyboard mode must be used.\n");;
        usage();
    }

    if (! joypad_dev && ! joypad_udev && ! keyboard) {
        fprintf(stderr, "Either joystick device, hotpluggable joystick mode or keyboard mode must be used.\n");;
        usage();
    }

    fprintf(stderr, "starting matelight controller\n");
    fprintf(stderr, "grid resolution: %d x %d, grid type: %s\n", grid_width, grid_height, grid_widescreen ? "widescreen" : "highscreen");

    (void)setlocale(LC_ALL, "C.UTF-8");

    srand(time(NULL));

    memset(&udp_sockaddr, '\0', sizeof(udp_sockaddr));
    udp_sockaddr.ss_family = AF_UNSPEC;
    if (address) {
        udp_sockaddr.ss_family = AF_INET;
        ((struct sockaddr_in *)&udp_sockaddr)->sin_port = htons(wled_port);
        ((struct sockaddr_in *)&udp_sockaddr)->sin_addr.s_addr = inet_addr(address);
    } else {
        wled_ds = mdns_description;
    }

    udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    input_reset();
    if (joypad_dev) {
        init_joystick(joypad_dev);
    } else if (joypad_udev) {
        init_udev_hotplug();
    } else {
        init_keyboard();
    }
    joystick_cnt = count_joysticks();

    ip_init();
    fprintf(stderr, "my IP-address is: %s\n", ip_address);
    if (wled_ds) {
        mdns_init();
    }
    if (mqtt) {
        mqtt_init();
    }

    for (i = 0; i < ARRAY_LENGTH(games); i++) {
        if (games[i]->init_func) {
            games[i]->init_func();
        }
    }

    start_time_val = get_time_val();
    last_tick_val = 0.0;
    ticks = 0;

    cur_game = 0;
    if (start_game != -1)
        cur_game = start_game;

    while (! get_game()->playable) {
        cur_game++;
        cur_game %= ARRAY_LENGTH(games);
    }

    if (get_game()->activate_func) {
        get_game()->activate_func(start_on_startup);
    }

    if (! start_on_startup) {
        do_announce_my_ip();
    }

    if (debug) {
        debug_game.activate_func(true);
    }

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
                (void)sendto(udp_fd, udp_data, UDP_DATA_SIZE, 0, (struct sockaddr *)&udp_sockaddr, sizeof(udp_sockaddr));
            }
        }

        usleep(100000);
    }
}
