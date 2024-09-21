/* flappy bird */

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "matelight.h"

#define MODE_GAME               0
#define MODE_DEAD               1
#define NUM_PIPES               (grid_width / 3)
#define MAX_NUM_PIPES           (MAX_GRID_WIDTH / 3)
#define PIPE_WIDTH              2
#define PIPE_SEPARATOR          7
#define FLOOR_HEIGHT            2
#define BIRD_X                  2
#define START_OFFSET            grid_width

static int game_mode = MODE_DEAD;
static int game_pause = false;
static int button = 0;
static int bird_y = 0;
static int pipes_high[MAX_NUM_PIPES] = { 0 };
static int pipes_low[MAX_NUM_PIPES] = { 0 };
static int pipes_off = 0;

static void setup_game(bool start)
{
    int i, r;

    game_mode = start ? MODE_GAME : MODE_DEAD;
    game_pause = false;
    button = 0;
    bird_y = (grid_height / 2);
    for (i = 0; i < NUM_PIPES; i++) {
        r = rand() % ((grid_height - FLOOR_HEIGHT) - (grid_height / 2));
        pipes_high[i] = 2 + r;
        pipes_low[i] = 2 + r + 8;
    }
    pipes_off = START_OFFSET;
}

static void activate(bool start)
{
    setup_game(start);
}

static void deactivate(void)
{
    setup_game(false);
}

static bool doit(void)
{
    int r;
    int x;
    int ix;
    int pipe_idx;

    if (game_pause)
        return true;

    // move bird
    if (button) {
        bird_y--;
        button--;
    } else {
        bird_y++;
    }

    while (bird_y < 0) {
        bird_y++;
    }
    if ((bird_y + 1) >= (grid_height - FLOOR_HEIGHT)) {
        while ((bird_y + 1) >= grid_height) {
            bird_y--;
        }
        return false;
    }

    // move pipes
    pipes_off--;
    if ((pipes_off + PIPE_WIDTH) < 0) {
        memmove(pipes_high, &pipes_high[1], sizeof(pipes_high[0]) * (NUM_PIPES - 1));
        memmove(pipes_low, &pipes_low[1], sizeof(pipes_low[0]) * (NUM_PIPES - 1));
        pipes_off += (PIPE_WIDTH + PIPE_SEPARATOR);
        r = rand() % ((grid_height - FLOOR_HEIGHT) - (grid_height / 2));
        pipes_high[NUM_PIPES - 1] = 2 + r;
        pipes_low[NUM_PIPES - 1] = 2 + r + 8;
    }

    // collision detection
    for (pipe_idx = 0; pipe_idx < NUM_PIPES; pipe_idx++) {
        for (ix = 0; ix < PIPE_WIDTH; ix++) {
            x = pipes_off + (pipe_idx * (PIPE_WIDTH + PIPE_SEPARATOR)) + ix;
            if (x == BIRD_X || x == (BIRD_X + 1)) {
                if (bird_y <= pipes_high[pipe_idx] || (bird_y + 1) >= pipes_low[pipe_idx]) {
                    return false;
                }
            }
        }
    }

    return true;
}

static void key_button(void)
{
    button = 4;
}

static void tick(void)
{
    if (game_mode != MODE_GAME) return;

    if (! doit())
        game_mode = MODE_DEAD;
}

static void input(int player, int key_idx, bool key_val, int key_state)
{
    (void)player;
    (void)key_state;

    switch (game_mode) {
        case MODE_GAME:
            if (key_idx == KEYPAD_START && key_val) {
                if (! game_pause) {
                    game_pause = true;
                } else {
                    game_pause = false;
                }
            }
            if ((key_idx == KEYPAD_B || key_idx == KEYPAD_A) && key_val) {
                key_button();
            }
            break;

        case MODE_DEAD:
            switch (key_idx) {
                case KEYPAD_SELECT:
                case KEYPAD_START:
                case KEYPAD_B:
                case KEYPAD_A:
                    if (key_val) {
                        setup_game(true);
                    }
                    break;
                default:
                    break;
            }
    }
}

static void draw(char *screen)
{
    int x, y;
    int ix;
    int pipe_idx;

    // background and floor
    for (y = 0; y < grid_height; y++) {
        for (x = 0; x < grid_width; x++) {
            if (y < (grid_height - FLOOR_HEIGHT)) {
                set_pixel(screen, y, x, COLOR_BLUE);
            } else {
                set_pixel(screen, y, x, 0x964b00);
            }
        }
    }

    // pipes
    for (pipe_idx = 0; pipe_idx < NUM_PIPES; pipe_idx++) {
        for (ix = 0; ix < PIPE_WIDTH; ix++) {
            x = pipes_off + (pipe_idx * (PIPE_WIDTH + PIPE_SEPARATOR)) + ix;
            if (x >= 0 && x < grid_width) {
                for (y = 0; y < pipes_high[pipe_idx]; y++) {
                    set_pixel(screen, y, x, 0x1fd655);
                }
                set_pixel(screen, pipes_high[pipe_idx], x, 0x83f28f);

                set_pixel(screen, pipes_low[pipe_idx], x, 0x83f28f);
                for (y = pipes_low[pipe_idx] + 1; y < (grid_height - FLOOR_HEIGHT); y++) {
                    set_pixel(screen, y, x, 0x1fd655);
                }
            }
        }
    }

    // bird
    set_pixel(screen, bird_y + 0, BIRD_X + 0, COLOR_YELLOW);
    set_pixel(screen, bird_y + 1, BIRD_X + 0, COLOR_YELLOW);
    set_pixel(screen, bird_y + 0, BIRD_X + 1, COLOR_WHITE);
    set_pixel(screen, bird_y + 1, BIRD_X + 1, COLOR_RED);
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

const struct game flappy_game = {
    "flappy bird",
    true,
    false,
    0.1,
    NULL,
    activate,
    deactivate,
    input,
    tick,
    render,
    idle,
};
