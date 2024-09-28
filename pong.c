/* pong */

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "matelight.h"

#define MODE_GAME               0
#define MODE_DEAD               1

#define PADDLE_1                0
#define PADDLE_2                1
#define PADDLE_WIDTH            4
#define PADDLE_1_Y              1
#define PADDLE_2_Y              (grid_height - 2)
#define PADDLE_1_X              1
#define PADDLE_2_X              (grid_width - 2)
#define MOVE_NONE               0
#define MOVE_LEFT               -1
#define MOVE_RIGHT              1

#define DIR_UP                  (M_PI * 1.5)
#define DIR_DOWN                (M_PI * 0.5)
#define DIR_LEFT                (M_PI * 1.0)
#define DIR_RIGHT               (M_PI * 0.0)

static int game_mode = MODE_DEAD;
static bool game_pause = false;

static int paddle_1_pos = 0;
static int paddle_1_dir = MOVE_NONE;
static int paddle_2_pos = 0;
static int paddle_2_dir = MOVE_NONE;

static double ball_y = 0.0;
static double ball_x = 0.0;
static double ball_dir = 0.0;

static void setup_game(bool start)
{
    game_mode = start ? MODE_GAME : MODE_DEAD;
    game_pause = false;

    if (grid_widescreen) {
        paddle_1_pos = (grid_height / 2) - (PADDLE_WIDTH / 2);
        paddle_2_pos = (grid_height / 2) - (PADDLE_WIDTH / 2);
    } else {
        paddle_1_pos = (grid_width / 2) - (PADDLE_WIDTH / 2);
        paddle_2_pos = (grid_width / 2) - (PADDLE_WIDTH / 2);
    }
    paddle_1_dir = MOVE_NONE;
    paddle_2_dir = MOVE_NONE;

    ball_y = (double)(grid_height / 2);
    ball_x = (double)(grid_width / 2);
    if (grid_widescreen) {
        ball_dir = DIR_RIGHT;
    } else {
        ball_dir = DIR_DOWN;
    }
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
    double hit_offset;

    if (game_pause)
        return true;

    /* move 1. paddle */
    paddle_1_pos += paddle_1_dir;
    if (paddle_1_pos < 0) paddle_1_pos = 0;
    if (grid_widescreen) {
        if (paddle_1_pos > (grid_height - PADDLE_WIDTH)) paddle_1_pos = grid_height - PADDLE_WIDTH;
    } else {
        if (paddle_1_pos > (grid_width - PADDLE_WIDTH)) paddle_1_pos = grid_width - PADDLE_WIDTH;
    }

    /* move 2. paddle */
    paddle_2_pos += paddle_2_dir;
    if (paddle_2_pos < 0) paddle_2_pos = 0;
    if (grid_widescreen) {
        if (paddle_2_pos > (grid_height - PADDLE_WIDTH)) paddle_2_pos = grid_height - PADDLE_WIDTH;
    } else {
        if (paddle_2_pos > (grid_width - PADDLE_WIDTH)) paddle_2_pos = grid_width - PADDLE_WIDTH;
    }

    /* move ball */
    ball_x += cos(ball_dir);
    ball_y += sin(ball_dir);

    /* check if ball is outside grid */
    if (grid_widescreen) {
        if (ball_x < 0.0 || ball_x >= (double)grid_width) {
            ball_y = -1.0;
            ball_x = -1.0;
            return false;
        }
    } else {
        if (ball_y < 0.0 || ball_y >= (double)grid_height) {
            ball_y = -1.0;
            ball_x = -1.0;
            return false;
        }
    }

    if (grid_widescreen) {
        /* check ball/1. paddle collision */
        if (lround(ball_x) == PADDLE_1_X && lround(ball_y) >= paddle_1_pos && lround(ball_y) < (paddle_1_pos + PADDLE_WIDTH)) {
            hit_offset = ((((ball_y - (double)paddle_1_pos) + 0.5) / (double)PADDLE_WIDTH) * 2.0) - 1.0;

            if (hit_offset > 5.0) {
                ball_dir = DIR_RIGHT - ((M_PI * 0.3) * ((double)rand() / (double)RAND_MAX));
            } else if (hit_offset > 0.0) {
                ball_dir = DIR_RIGHT - ((M_PI * 0.2) * ((double)rand() / (double)RAND_MAX));
            } else if (hit_offset <= 0.5) {
                ball_dir = DIR_RIGHT + ((M_PI * 0.2) * ((double)rand() / (double)RAND_MAX));
            } else {
                ball_dir = DIR_RIGHT + ((M_PI * 0.3) * ((double)rand() / (double)RAND_MAX));
            }
            ball_x += 1.0;
        }

        /* check ball/2. paddle collision */
        if (lround(ball_x) == PADDLE_2_X && lround(ball_y) >= paddle_2_pos && lround(ball_y) < (paddle_2_pos + PADDLE_WIDTH)) {
            hit_offset = ((((ball_y - (double)paddle_2_pos) + 0.5) / (double)PADDLE_WIDTH) * 2.0) - 1.0;

            if (hit_offset > 0.5) {
                ball_dir = DIR_LEFT + ((M_PI * 0.3) * ((double)rand() / (double)RAND_MAX));
            } else if (hit_offset > 0.0) {
                ball_dir = DIR_LEFT + ((M_PI * 0.2) * ((double)rand() / (double)RAND_MAX));
            } else if (hit_offset <= 0.5) {
                ball_dir = DIR_LEFT - ((M_PI * 0.2) * ((double)rand() / (double)RAND_MAX));
            } else {
                ball_dir = DIR_LEFT - ((M_PI * 0.3) * ((double)rand() / (double)RAND_MAX));
            }
            ball_x -= 1.0;
        }
    } else {
        /* check ball/1. paddle collision */
        if (lround(ball_y) == PADDLE_1_Y && lround(ball_x) >= paddle_1_pos && lround(ball_x) < (paddle_1_pos + PADDLE_WIDTH)) {
            hit_offset = ((((ball_x - (double)paddle_1_pos) + 0.5) / (double)PADDLE_WIDTH) * 2.0) - 1.0;

            if (hit_offset > 5.0) {
                ball_dir = DIR_DOWN - ((M_PI * 0.3) * ((double)rand() / (double)RAND_MAX));
            } else if (hit_offset > 0.0) {
                ball_dir = DIR_DOWN - ((M_PI * 0.2) * ((double)rand() / (double)RAND_MAX));
            } else if (hit_offset <= 0.5) {
                ball_dir = DIR_DOWN + ((M_PI * 0.2) * ((double)rand() / (double)RAND_MAX));
            } else {
                ball_dir = DIR_DOWN + ((M_PI * 0.3) * ((double)rand() / (double)RAND_MAX));
            }
            ball_y += 1.0;
        }

        /* check ball/2. paddle collision */
        if (lround(ball_y) == PADDLE_2_Y && lround(ball_x) >= paddle_2_pos && lround(ball_x) < (paddle_2_pos + PADDLE_WIDTH)) {
            hit_offset = ((((ball_x - (double)paddle_2_pos) + 0.5) / (double)PADDLE_WIDTH) * 2.0) - 1.0;

            if (hit_offset > 0.5) {
                ball_dir = DIR_UP + ((M_PI * 0.3) * ((double)rand() / (double)RAND_MAX));
            } else if (hit_offset > 0.0) {
                ball_dir = DIR_UP + ((M_PI * 0.2) * ((double)rand() / (double)RAND_MAX));
            } else if (hit_offset <= 0.5) {
                ball_dir = DIR_UP - ((M_PI * 0.2) * ((double)rand() / (double)RAND_MAX));
            } else {
                ball_dir = DIR_UP - ((M_PI * 0.3) * ((double)rand() / (double)RAND_MAX));
            }
            ball_y -= 1.0;
        }
    }

    if (grid_widescreen) {
        /* check ball/top wall collision */
        if (ball_y <= 0.0) {
            ball_dir = atan2(sin(ball_dir) * -1.0, cos(ball_dir));
            ball_y = 0.0;
        }

        /* check ball/bottom wall collision */
        if (ball_y >= (double)grid_height) {
            ball_dir = atan2(sin(ball_dir) * -1.0, cos(ball_dir));
            ball_y = (double)(grid_height - 1);
        }
    } else {
        /* check ball/left wall collision */
        if (ball_x <= 0.0) {
            ball_dir = atan2(sin(ball_dir), cos(ball_dir) * -1.0);
            ball_x = 0.0;
        }

        /* check ball/right wall collision */
        if (ball_x >= (double)grid_width) {
            ball_dir = atan2(sin(ball_dir), cos(ball_dir) * -1.0);
            ball_x = (double)(grid_width - 1);
        }
    }

    return true;
}

static void move(int paddle, int direction)
{
    if (paddle == PADDLE_1) {
        paddle_1_dir = direction;
    } else if (paddle == PADDLE_2) {
        paddle_2_dir = direction;
    }
}

static void tick(void)
{
    if (game_mode != MODE_GAME) return;

    if (! doit())
        game_mode = MODE_DEAD;
}

static void input(int player, int key_idx, bool key_val, int key_state)
{
    int paddle_idx_1, paddle_idx_2;

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

            if ((player % 2) == 0) {
                paddle_idx_1 = PADDLE_1;
                paddle_idx_2 = PADDLE_2;
            } else {
                paddle_idx_1 = PADDLE_2;
                paddle_idx_2 = PADDLE_1;
            }

            if (key_idx == KEYPAD_LEFT || key_idx == KEYPAD_RIGHT || key_idx == KEYPAD_UP || key_idx == KEYPAD_DOWN) {
                if ((key_idx == KEYPAD_LEFT || key_idx == KEYPAD_UP) && key_val) {
                    move(paddle_idx_1, MOVE_LEFT);
                } else if ((key_idx == KEYPAD_RIGHT || key_idx == KEYPAD_DOWN) && key_val) {
                    move(paddle_idx_1, MOVE_RIGHT);
                } else {
                    move(paddle_idx_1, MOVE_NONE);
                }
            }

            if (key_idx == KEYPAD_B || key_idx == KEYPAD_A) {
                if (key_idx == KEYPAD_B && key_val) {
                    move(paddle_idx_2, MOVE_LEFT);
                } else if (key_idx == KEYPAD_A && key_val) {
                    move(paddle_idx_2, MOVE_RIGHT);
                } else {
                    move(paddle_idx_2, MOVE_NONE);
                }
            }
            break;

        case MODE_DEAD:
            switch (key_idx) {
                case KEYPAD_LEFT:
                case KEYPAD_RIGHT:
                case KEYPAD_UP:
                case KEYPAD_DOWN:
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

    if (grid_widescreen) {
        // 1. paddle
        for (y = 0; y < PADDLE_WIDTH; y++) {
            set_pixel(screen, paddle_1_pos + y, PADDLE_1_X, COLOR_WHITE);
        }

        // 2. paddle
        for (y = 0; y < PADDLE_WIDTH; y++) {
            set_pixel(screen, paddle_2_pos + y, PADDLE_2_X, COLOR_WHITE);
        }
    } else {
        // 1. paddle
        for (x = 0; x < PADDLE_WIDTH; x++) {
            set_pixel(screen, PADDLE_1_Y, paddle_1_pos + x, COLOR_WHITE);
        }

        // 2. paddle
        for (x = 0; x < PADDLE_WIDTH; x++) {
            set_pixel(screen, PADDLE_2_Y, paddle_2_pos + x, COLOR_WHITE);
        }
    }

    // ball
    y = lround(ball_y);
    x = lround(ball_x);
    if (y >= 0 && y < grid_height && x >= 0 && x < grid_width) {
        set_pixel(screen, y, x, COLOR_BLUE);
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

const struct game pong_game = {
    "pong",
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
