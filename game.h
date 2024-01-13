#ifndef GAME_H
#define GAME_H

#include <stddef.h>
#include <stdbool.h>

#define ARRAY_LENGTH(array) (sizeof((array)) / sizeof((array)[0]))

// Grid
#define GRID_WIDTH      10
#define GRID_HEIGHT     20

// WLED
#define WLED_WARLS      1
#define WLED_DRGB       2
#define WLED_DRGBW      3
#define WLED_DNRGB      4

// Display
#define DISPLAY_TIMEOUT 1

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

struct game {
    const char *name;
    double tick_freq;
    void (*init_func)(void);
    void (*activate_func)(bool start);
    void (*deactivate_func)(void);
    void (*input_func)(int key_idx, bool key_val);
    void (*tick_func)();
    void (*render_func)(bool *display, char *screen);
    bool (*idle_func)(void);
};

extern const char vga_palette[16 * 3];

extern double time_val;
extern int ticks;
extern int key_state;

extern const struct game snake_game;
extern const struct game tetris_game;

void mqtt_init(void);

#endif /* GAME_H */
