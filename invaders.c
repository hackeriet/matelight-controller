/* space invaders */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "matelight.h"

#define MODE_GAME               0
#define MODE_DEAD               1

#define SHOOTER_WIDTH           3
#define SHOOTER_Y               (grid_height - 2)
#define MOVE_NONE               0
#define MOVE_LEFT               -1
#define MOVE_RIGHT              1

#define INVADERS_START_ROW      2
#define INVADERS_ROWS           8
#define INVADERS_COLS           8
#define INVADERS_X_SPEED        4
#define INVADERS_Y_SPEED        50
#define INVADERS_BULLET_FREQ    20

static int game_mode = MODE_DEAD;
static bool game_pause = false;
static int tick_count = 0;

static int shooter_x = 0.0;
static int shooter_dir = MOVE_NONE;

static uint8_t invaders[INVADERS_ROWS * MAX_GRID_WIDTH] = { 0 };
static size_t num_invaders = 0;
static uint32_t invaders_colors[INVADERS_ROWS * MAX_GRID_WIDTH] = { 0 };
static int invaders_y = INVADERS_START_ROW;
static int invaders_dir = MOVE_RIGHT;

static int player_bullet_y = -1;
static int player_bullet_x = -1;
static int invaders_bullet_y = -1;
static int invaders_bullet_x = -1;
static int invaders_bullet_last_tick = 0;

static const char invader[INVADERS_ROWS][INVADERS_COLS] = {
    "   ##   ",
    "  ####  ",
    " ###### ",
    "## ## ##",
    "########",
    "  #  #  ",
    " # ## # ",
    "# #  # #"
};

static const uint32_t invader_colors[] = {
    COLOR_BLUE,
    COLOR_GREEN,
    COLOR_CYAN,
    //COLOR_RED,
    COLOR_MAGENTA,
    COLOR_YELLOW,
};

static void setup_game(bool start)
{
    int y, x;
    uint32_t rnd_color = invader_colors[rand() % ARRAY_LENGTH(invader_colors)];

    game_mode = start ? MODE_GAME : MODE_DEAD;
    game_pause = false;
    tick_count = 0;

    shooter_x = (grid_width / 2) - (SHOOTER_WIDTH / 2);
    shooter_dir = MOVE_NONE;

    num_invaders = 0;
    invaders_y = INVADERS_START_ROW;
    invaders_dir = MOVE_RIGHT;

    for (y = 0; y < INVADERS_ROWS; y++) {
        for (x = 0; x < grid_width; x++) {
            invaders[(y * grid_width) + x] = 0;
            invaders_colors[(y * grid_width) + x] = 0;
        }
    }

    for (y = 0; y < INVADERS_ROWS; y++) {
        for (x = 0; x < INVADERS_COLS; x++) {
            if (invader[y][x] == '#') {
                invaders[(y * grid_width) + ((grid_width - INVADERS_COLS) / 2) + x] = 1;
                num_invaders++;
                invaders_colors[(y * grid_width) + ((grid_width - INVADERS_COLS) / 2) + x] = rnd_color;
            }
        }
    }

    player_bullet_y = -1;
    player_bullet_x = -1;
    invaders_bullet_y = -1;
    invaders_bullet_x = -1;
    invaders_bullet_last_tick = tick_count;
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
    int y, x, rnd;
    bool turn = false;

    if (game_pause)
        return true;

    tick_count++;

    /* move shooter */
    shooter_x += shooter_dir;
    if (shooter_x < 0) shooter_x = 0;
    if (shooter_x > (grid_width - SHOOTER_WIDTH)) shooter_x = grid_width - SHOOTER_WIDTH;

    if ((tick_count % INVADERS_X_SPEED) == 0) {
        /* turn invaders */
        turn = false;
        if (invaders_dir == MOVE_LEFT) {
            for (y = 0; y < INVADERS_ROWS; y++) {
                if (invaders[(y * grid_width) + 0]) {
                    turn = true;
                    break;
                }
            }
        } else if (invaders_dir == MOVE_RIGHT) {
            for (y = 0; y < INVADERS_ROWS; y++) {
                if (invaders[(y * grid_width) + (grid_width - 1)]) {
                    turn = true;
                    break;
                }
            }
        }
        if (turn)
            invaders_dir *= -1;

        /* move invaders */
        if (invaders_dir == MOVE_LEFT) {
            for (y = 0; y < INVADERS_ROWS; y++) {
                memmove(&invaders[(y * grid_width) + 0], &invaders[(y * grid_width) + 1], sizeof(*invaders) * (grid_width - 1));
                invaders[(y * grid_width) + (grid_width - 1)] = 0;
                memmove(&invaders_colors[(y * grid_width) + 0], &invaders_colors[(y * grid_width) + 1], sizeof(*invaders_colors) * (grid_width - 1));
                invaders_colors[(y * grid_width) + (grid_width - 1)] = 0;
            }
        } else if (invaders_dir == MOVE_RIGHT) {
            for (y = 0; y < INVADERS_ROWS; y++) {
                memmove(&invaders[(y * grid_width) + 1], &invaders[(y * grid_width) + 0], sizeof(*invaders) * (grid_width - 1));
                invaders[(y * grid_width) + 0] = 0;
                memmove(&invaders_colors[(y * grid_width) + 1], &invaders_colors[(y * grid_width) + 0], sizeof(*invaders_colors) * (grid_width - 1));
                invaders_colors[(y * grid_width) + 0] = 0;
            }
        }
    }

    /* move invaders down */
    if ((tick_count % INVADERS_Y_SPEED) == 0) {
        invaders_y++;
    }

    /* move player bullet */
    if (player_bullet_y >= 0 && player_bullet_x >= 0) {
        player_bullet_y--;
    }

    /* check if player bullet is outside grid */
    if (player_bullet_y < 0) {
        player_bullet_y = -1;
        player_bullet_x = -1;
    }

    /* move invaders bullet */
    if (invaders_bullet_y >= 0 && invaders_bullet_x >= 0) {
        invaders_bullet_y++;
        invaders_bullet_last_tick = tick_count;
    }

    /* launch invaders bullet */
    if (invaders_bullet_y < 0 && invaders_bullet_x < 0 && (tick_count - invaders_bullet_last_tick) >= INVADERS_BULLET_FREQ && num_invaders > 0) {
        rnd = rand() % grid_width;
        for (y = INVADERS_ROWS - 1; y >= 0; y--) {
            for (x = 0; x < grid_width; x++) {
                if (invaders[(y * grid_width) + ((x + rnd) % grid_width)]) {
                    invaders_bullet_y = invaders_y + y + 1;
                    invaders_bullet_x = (x + rnd) % grid_width;
                    break;
                }
            }
            if (invaders_bullet_y >= 0 && invaders_bullet_x >= 0)
                break;
        }
    }

    /* check bullet collisions */
    if (player_bullet_y >= 0 && player_bullet_x >= 0 && invaders_bullet_y >= 0 && invaders_bullet_x >= 0) {
        if (player_bullet_y == invaders_bullet_y && player_bullet_x == invaders_bullet_x) {
            player_bullet_y = -1;
            player_bullet_x = -1;
            invaders_bullet_y = -1;
            invaders_bullet_x = -1;
        }
    }

    /* check if invaders bullet is outside grid */
    if (invaders_bullet_y >= grid_height) {
        invaders_bullet_y = -1;
        invaders_bullet_x = -1;
    }

    /* check invaders outside grid or shooter/invaders collision */
    if ((invaders_y + INVADERS_ROWS) > (SHOOTER_Y - 1)) {
        for (y = INVADERS_ROWS - 1; y >= 0; y--) {
            for (x = 0; x < grid_width; x++) {
                if (invaders[(y * grid_width) + x]) {
                    /* invaders outside grid */
                    if ((invaders_y + y) >= grid_height) {
                        player_bullet_y = -1;
                        player_bullet_x = -1;
                        invaders_bullet_y = -1;
                        invaders_bullet_x = -1;
                        return false;
                    }
                    /* shooter/invaders collision */
                    if ((invaders_y + y) == (SHOOTER_Y - 1)) {
                        if (x == shooter_x + (SHOOTER_WIDTH / 2)) {
                            player_bullet_y = -1;
                            player_bullet_x = -1;
                            invaders_bullet_y = -1;
                            invaders_bullet_x = -1;
                            return false;
                        }
                    } else if ((invaders_y + y) == SHOOTER_Y) {
                        if (x >= shooter_x && x < (shooter_x + SHOOTER_WIDTH)) {
                            player_bullet_y = -1;
                            player_bullet_x = -1;
                            invaders_bullet_y = -1;
                            invaders_bullet_x = -1;
                            return false;
                        }
                    }
                }
            }
        }
    }

    /* check invaders bullet/shooter collision */
    if (invaders_bullet_y >= 0 && invaders_bullet_x >= 0) {
        if (invaders_bullet_y == (SHOOTER_Y - 1)) {
            if (invaders_bullet_x == shooter_x + (SHOOTER_WIDTH / 2)) {
                player_bullet_y = -1;
                player_bullet_x = -1;
                invaders_bullet_y = -1;
                invaders_bullet_x = -1;
                return false;
            }
        } else if (invaders_bullet_y == SHOOTER_Y) {
            if (invaders_bullet_x >= shooter_x && invaders_bullet_x < (shooter_x + SHOOTER_WIDTH)) {
                player_bullet_y = -1;
                player_bullet_x = -1;
                invaders_bullet_y = -1;
                invaders_bullet_x = -1;
                return false;
            }
        }
    }

    /* check player bullet/invaders collision */
    if (player_bullet_y >= 0 && player_bullet_x >= 0) {
        if (player_bullet_y >= invaders_y && player_bullet_y < (invaders_y + INVADERS_ROWS)) {
            if (invaders[((player_bullet_y - invaders_y) * grid_width) + player_bullet_x]) {
                invaders[((player_bullet_y - invaders_y) * grid_width) + player_bullet_x] = 0;
                if (num_invaders > 0)
                    num_invaders--;
                player_bullet_y = -1;
                player_bullet_x = -1;
            }
        }
    }

    if (num_invaders == 0) {
        player_bullet_y = -1;
        player_bullet_x = -1;
        invaders_bullet_y = -1;
        invaders_bullet_x = -1;
        return false;
    }

    return true;
}

static void move(int direction)
{
    shooter_dir = direction;
}

static void shoot(void)
{
    if (player_bullet_y >= 0 && player_bullet_x >= 0) {
        return;
    }

    player_bullet_y = SHOOTER_Y - 1;
    player_bullet_x = shooter_x + (SHOOTER_WIDTH / 2);
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

            if (key_idx == KEYPAD_LEFT && key_val) {
                move(MOVE_LEFT);
            } else if (key_idx == KEYPAD_RIGHT && key_val) {
                move(MOVE_RIGHT);
            } else {
                move(MOVE_NONE);
            }

            if ((key_idx == KEYPAD_B || key_idx == KEYPAD_A) && key_val) {
                shoot();
            }
            break;

        case MODE_DEAD:
            switch (key_idx) {
                case KEYPAD_LEFT:
                case KEYPAD_RIGHT:
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

    // background
    for (y = 0; y < grid_height; y++) {
        for (x = 0; x < grid_width; x++) {
            set_pixel(screen, y, x, COLOR_BLACK);
        }
    }

    // invaders
    for (y = 0; y < INVADERS_ROWS; y++) {
        for (x = 0; x < grid_width; x++) {
            if (invaders[(y * grid_width) + x]) {
                set_pixel(screen, y + invaders_y, x, invaders_colors[(y * grid_width) + x]);
            }
        }
    }

    // shooter
    set_pixel(screen, SHOOTER_Y - 1, shooter_x + (SHOOTER_WIDTH / 2), COLOR_WHITE);
    for (x = 0; x < SHOOTER_WIDTH; x++) {
        set_pixel(screen, SHOOTER_Y, shooter_x + x, COLOR_WHITE);
    }

    // player bullet
    if (player_bullet_y >= 0 && player_bullet_x >= 0) {
        set_pixel(screen, player_bullet_y, player_bullet_x, COLOR_RED);
    }

    // invaders bulett
    if (invaders_bullet_y >= 0 && invaders_bullet_x >= 0) {
        set_pixel(screen, invaders_bullet_y, invaders_bullet_x, COLOR_BROWN);
    }
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

const struct game invaders_game = {
    "invaders",
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
