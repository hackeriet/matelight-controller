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

// mode 13h palette
#define COLOR_BLACK     0
#define COLOR_BLUE      1
#define COLOR_GREEN     2
#define COLOR_CYAN      3
#define COLOR_RED       4
#define COLOR_MAGENTA   5
#define COLOR_ORANGE    6
#define COLOR_GREY      7
#define COLOR_WHITE     15

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
};

extern const char vga_palette[16 * 3];

extern double time_val;
extern int ticks;
extern int key_state;

extern const struct game snake_game;

#endif /* GAME_H */
