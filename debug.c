/* debug */

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "matelight.h"

#define MODE_GAME       0
#define MODE_DEAD       1

#define MAX_PLAYERS     ((grid_height - 1) / 4)
#define MAX_MAX_PLAYERS ((MAX_GRID_HEIGHT - 1) / 4)

#define TIMEOUT_TICKS   50 /* 5 secs */

static int game_mode = MODE_DEAD;
static int tick_count = 0;
static int last_key_press = 0;
static int key_states[MAX_MAX_PLAYERS] = { 0 };

static void setup_game(bool start)
{
    game_mode = start ? MODE_GAME : MODE_DEAD;
    tick_count = 0;
    last_key_press = 0;
}

static void activate(bool start)
{
    setup_game(start);
}

static void deactivate(void)
{
    setup_game(false);
}


static void tick(void)
{
    tick_count++;

    if (game_mode != MODE_GAME) return;

    if ((tick_count - last_key_press) >= TIMEOUT_TICKS)
        game_mode = MODE_DEAD;
}

static void input(int player, int key_idx, bool key_val, int key_state)
{
    int player_idx = player - 1;

    (void)key_idx;
    (void)key_val;

    if (game_mode != MODE_GAME) return;

    last_key_press = tick_count;
    if (player_idx < MAX_PLAYERS) {
        key_states[player_idx] = key_state;
    }
}

static void draw(char *screen)
{
    int x, y;
    int player_idx, player;
    int key_state;

    // background
    for (y = 0; y < grid_height; y++) {
        for (x = 0; x < grid_width; x++) {
            set_pixel(screen, y, x, COLOR_BLACK);
        }
    }

    // players
    for (player_idx = 0; player_idx < MAX_PLAYERS; player_idx++) {
        player = player_idx + 1;
        if (! has_player(player)) continue;
        key_state = key_states[player_idx];

        for (y = (player_idx * 4) + 1; y < ((player_idx * 4) + 4); y++) {
            for (x = 0; x < grid_width; x++) {
                set_pixel(screen, y, x, COLOR_RGB(128, 128, 128));
            }
        }

        set_pixel(screen, (player_idx * 4) + 2, 0, (key_state & KEYPAD_LEFT)    ? COLOR_YELLOW : COLOR_BLACK);
        set_pixel(screen, (player_idx * 4) + 2, 2, (key_state & KEYPAD_RIGHT)   ? COLOR_YELLOW : COLOR_BLACK);
        set_pixel(screen, (player_idx * 4) + 1, 1, (key_state & KEYPAD_UP)      ? COLOR_YELLOW : COLOR_BLACK);
        set_pixel(screen, (player_idx * 4) + 3, 1, (key_state & KEYPAD_DOWN)    ? COLOR_YELLOW : COLOR_BLACK);
        set_pixel(screen, (player_idx * 4) + 2, 4, (key_state & KEYPAD_SELECT)  ? COLOR_YELLOW : COLOR_BLACK);
        set_pixel(screen, (player_idx * 4) + 2, 5, (key_state & KEYPAD_START)   ? COLOR_YELLOW : COLOR_BLACK);
        set_pixel(screen, (player_idx * 4) + 2, 7, (key_state & KEYPAD_B)       ? COLOR_YELLOW : COLOR_RED);
        set_pixel(screen, (player_idx * 4) + 2, 9, (key_state & KEYPAD_A)       ? COLOR_YELLOW : COLOR_RED);
    }

    // footer
    set_pixel(screen, grid_height - 1, ((tick_count / 2) + 0) % grid_width, COLOR_RGB(255, 0, 0));
    set_pixel(screen, grid_height - 1, ((tick_count / 2) + 1) % grid_width, COLOR_RGB(0, 255, 0));
    set_pixel(screen, grid_height - 1, ((tick_count / 2) + 2) % grid_width, COLOR_RGB(0, 0, 255));
}

static void render(bool *display, char *screen)
{
    if (game_mode == MODE_GAME) {
        *display = true;
        draw(screen);
    } else {
        *display = false;
    }
}

static bool idle(void)
{
    return game_mode != MODE_GAME;
}

const struct game debug_game = {
    "debug",
    false,
    true,
    0.1,
    NULL,
    activate,
    deactivate,
    input,
    tick,
    render,
    idle,
};
