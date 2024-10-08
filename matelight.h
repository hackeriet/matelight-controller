#ifndef MATELIGHT_H
#define MATELIGHT_H

#include <stddef.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <linux/limits.h>

#define ARRAY_LENGTH(array) (sizeof((array)) / sizeof((array)[0]))

#define MIN(a, b) ((a) > (b) ? (b) : (a))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// Grid
//#define DEFAULT_GRID_WIDTH  10
//#define DEFAULT_GRID_HEIGHT 20
#define DEFAULT_GRID_WIDTH  20
#define DEFAULT_GRID_HEIGHT 12

#define MIN_GRID_WIDTH  10
#define MAX_GRID_WIDTH  20
#define MIN_GRID_HEIGHT 10
#define MAX_GRID_HEIGHT 20

// WLED
#define WLED_WARLS      1
#define WLED_DRGB       2
#define WLED_DRGBW      3
#define WLED_DNRGB      4

// 490 is the maximum number of LEDs which can fit into 1472 bytes which is the max size of an unfragmented UDP datagram over IPv4 on 1500 MTU Ethernet
#define WLED_DRGB_MAX_LEDS  490

#define MAX_GRID_SIZE       MAX(WLED_DRGB_MAX_LEDS, (MAX_GRID_WIDTH * MAX_GRID_HEIGHT))

// Display
#define DISPLAY_TIMEOUT 3

// RGB
#define COLOR_RGB(r, g, b)    (((r) << 16) | ((g) << 8) | (b))

// 13h palette
#define COLOR_BLACK             COLOR_RGB(0x00, 0x00, 0x00)
#define COLOR_BLUE              COLOR_RGB(0x00, 0x00, 0x80)
#define COLOR_GREEN             COLOR_RGB(0x00, 0x80, 0x00)
#define COLOR_CYAN              COLOR_RGB(0x00, 0x80, 0x80)
#define COLOR_RED               COLOR_RGB(0x80, 0x00, 0x00)
#define COLOR_MAGENTA           COLOR_RGB(0x80, 0x00, 0x80)
#define COLOR_BROWN             COLOR_RGB(0x80, 0x80, 0x00)
#define COLOR_LIGHT_GRAY        COLOR_RGB(0xc0, 0xc0, 0xc0)
#define COLOR_DARK_GRAY         COLOR_RGB(0x80, 0x80, 0x80)
#define COLOR_LIGHT_BLUE        COLOR_RGB(0x00, 0x00, 0xff)
#define COLOR_LIGHT_GREEN       COLOR_RGB(0x00, 0xff, 0x00)
#define COLOR_LIGHT_CYAN        COLOR_RGB(0x00, 0xff, 0xff)
#define COLOR_LIGHT_RED         COLOR_RGB(0xff, 0x00, 0x00)
#define COLOR_LIGHT_MAGENTA     COLOR_RGB(0xff, 0x00, 0xff)
#define COLOR_YELLOW            COLOR_RGB(0xff, 0xff, 0x00)
#define COLOR_WHITE             COLOR_RGB(0xff, 0xff, 0xff)

// Other colors
#define COLOR_ORANGE            COLOR_RGB(0xff, 0x7f, 0x00)

#define INPUT_KEYBOARD  0
#define INPUT_JOYSTICK  1

// Keys
#define KEYPAD_NONE     0
#define KEYPAD_LEFT     (1 << 0)
#define KEYPAD_RIGHT    (1 << 1)
#define KEYPAD_UP       (1 << 2)
#define KEYPAD_DOWN     (1 << 3)
#define KEYPAD_SELECT   (1 << 4)
#define KEYPAD_START    (1 << 5)
#define KEYPAD_B        (1 << 6)
#define KEYPAD_A        (1 << 7)

#define MAX_JOYSTICKS 64
#define KEY_HISTORY_SIZE 16

struct joystick {
    int type;
    int fd;
    char devnode[PATH_MAX];
    dev_t dev;

    char axes;
    char buttons;
    char name[128];

    int player;

    int key_state;
    int last_key_idx;
    bool last_key_val;
    int key_history[KEY_HISTORY_SIZE];
};

struct game {
    const char *name;
    bool playable;
    bool non_interruptable;
    double tick_freq;
    void (*init_func)(void);
    void (*activate_func)(bool start);
    void (*deactivate_func)(void);
    void (*input_func)(int player, int key_idx, bool key_val, int key_state);
    void (*tick_func)();
    void (*render_func)(bool *display, char *screen);
    bool (*idle_func)(void);
};

extern int grid_width;
extern int grid_height;
extern bool grid_widescreen;

extern double time_val;
extern int ticks;

extern const char *wled_ds;
extern void update_wled_ip(const char *address);

extern void do_announce(const char *text, unsigned int color, unsigned int bgcolor, double speed);
extern void do_announce_async(char *text, unsigned int color, unsigned int bgcolor, double speed);

extern const struct game announce_game;
extern void set_announce_text(const char *text, unsigned int color, unsigned int bgcolor, double speed);

extern const struct game debug_game;

extern const struct game snake_game;
extern const struct game tetris_game;
extern const struct game flappy_game;
extern const struct game pong_game;
extern const struct game breakout_game;
extern const struct game invaders_game;

extern char ip_address[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)];
extern void ip_init(void);
extern void mdns_init(void);
extern void input_reset(void);
extern void init_joystick(const char *devnode);
extern void init_udev_hotplug(void);
extern void init_keyboard(void);
extern bool read_joystick(struct joystick **joystick_ptr);
extern int count_joysticks(void);
extern bool joystick_is_key_seq(struct joystick *joystick, const int *seq, size_t seq_length);
extern bool has_player(int player);
extern void mqtt_init(void);
extern bool wled_api_check(const char *addr);

// Set pixel
static inline void set_pixel(char *screen, int y, int x, unsigned int color)
{
    unsigned char r = (color >> 16) & 0xff;
    unsigned char g = (color >> 8) & 0xff;
    unsigned char b = color & 0xff;

    screen[(((y * grid_width) + x)*3) + 0] = r;
    screen[(((y * grid_width) + x)*3) + 1] = g;
    screen[(((y * grid_width) + x)*3) + 2] = b;
}

#endif /* MATELIGHT_H */
