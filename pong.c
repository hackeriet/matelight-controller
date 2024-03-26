/* pong */

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "matelight.h"

#define MODE_GAME               0
#define MODE_DEAD               1

#define PADDLE_HIGH             0
#define PADDLE_LOW              1
#define PADDLE_WIDTH            4
#define PADDLE_HIGH_Y           1
#define PADDLE_LOW_Y            (GRID_HEIGHT - 2)
#define MOVE_NONE               0
#define MOVE_LEFT               -1
#define MOVE_RIGHT              1

#define DIR_UP                  (M_PI * 1.5)
#define DIR_DOWN                (M_PI * 0.5)

static int game_mode = MODE_DEAD;
static bool game_pause = false;

static int paddle_high_x = (GRID_WIDTH / 2) - (PADDLE_WIDTH / 2);
static int paddle_high_dir = MOVE_NONE;
static int paddle_low_x = (GRID_WIDTH / 2) - (PADDLE_WIDTH / 2);
static int paddle_low_dir = MOVE_NONE;

static double ball_y = (double)(GRID_HEIGHT / 2);
static double ball_x = (double)(GRID_WIDTH / 2);
static double ball_dir = DIR_DOWN;

static void setup_game(bool start)
{
    game_mode = start ? MODE_GAME : MODE_DEAD;
    game_pause = false;

    paddle_high_x = (GRID_WIDTH / 2) - (PADDLE_WIDTH / 2);
    paddle_high_dir = MOVE_NONE;
    paddle_low_x = (GRID_WIDTH / 2) - (PADDLE_WIDTH / 2);
    paddle_low_dir = MOVE_NONE;

    ball_y = (double)(GRID_HEIGHT / 2);
    ball_x = (double)(GRID_WIDTH / 2);
    ball_dir = DIR_DOWN;
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

    /* move high paddle */
    paddle_high_x += paddle_high_dir;
    if (paddle_high_x < 0) paddle_high_x = 0;
    if (paddle_high_x > (GRID_WIDTH - PADDLE_WIDTH)) paddle_high_x = GRID_WIDTH - PADDLE_WIDTH;

    /* move low paddle */
    paddle_low_x += paddle_low_dir;
    if (paddle_low_x < 0) paddle_low_x = 0;
    if (paddle_low_x > (GRID_WIDTH - PADDLE_WIDTH)) paddle_low_x = GRID_WIDTH - PADDLE_WIDTH;

    /* move ball */
    ball_x += cos(ball_dir);
    ball_y += sin(ball_dir);

    /* check if ball is outside grid */
    if (ball_y < 0.0 || ball_y >= (double)GRID_HEIGHT) {
        ball_y = -1.0;
        ball_x = -1.0;
        return false;
    }

    /* check ball/high paddle collision */
    if (lround(ball_y) == PADDLE_HIGH_Y && lround(ball_x) >= paddle_high_x && lround(ball_x) < (paddle_high_x + PADDLE_WIDTH)) {
        hit_offset = ((((ball_x - (double)paddle_high_x) + 0.5) / (double)PADDLE_WIDTH) * 2.0) - 1.0;

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

    /* check ball/low paddle collision */
    if (lround(ball_y) == PADDLE_LOW_Y && lround(ball_x) >= paddle_low_x && lround(ball_x) < (paddle_low_x + PADDLE_WIDTH)) {
        hit_offset = ((((ball_x - (double)paddle_low_x) + 0.5) / (double)PADDLE_WIDTH) * 2.0) - 1.0;

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

    /* check ball/left wall collision */
    if (ball_x <= 0.0) {
        ball_dir = atan2(sin(ball_dir), cos(ball_dir) * -1.0);
        ball_x = 0.0;
    }

    /* check ball/right wall collision */
    if (ball_x >= (double)GRID_WIDTH) {
        ball_dir = atan2(sin(ball_dir), cos(ball_dir) * -1.0);
        ball_x = (double)(GRID_WIDTH - 1);
    }

    return true;
}

static void move(int paddle, int direction)
{
    if (paddle == PADDLE_HIGH) {
        paddle_high_dir = direction;
    } else if (paddle == PADDLE_LOW) {
        paddle_low_dir = direction;
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
    int paddle1, paddle2;

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
                paddle1 = PADDLE_HIGH;
                paddle2 = PADDLE_LOW;
            } else {
                paddle1 = PADDLE_LOW;
                paddle2 = PADDLE_HIGH;
            }

            if (key_idx == KEYPAD_LEFT || key_idx == KEYPAD_RIGHT) {
                if (key_idx == KEYPAD_LEFT && key_val) {
                    move(paddle1, MOVE_LEFT);
                } else if (key_idx == KEYPAD_RIGHT && key_val) {
                    move(paddle1, MOVE_RIGHT);
                } else {
                    move(paddle1, MOVE_NONE);
                }
            }

            if (key_idx == KEYPAD_B || key_idx == KEYPAD_A) {
                if (key_idx == KEYPAD_B && key_val) {
                    move(paddle2, MOVE_LEFT);
                } else if (key_idx == KEYPAD_A && key_val) {
                    move(paddle2, MOVE_RIGHT);
                } else {
                    move(paddle2, MOVE_NONE);
                }
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
    for (y = 0; y < GRID_HEIGHT; y++) {
        for (x = 0; x < GRID_WIDTH; x++) {
            set_pixel(screen, y, x, COLOR_BLACK);
        }
    }

    // high paddle
    for (x = 0; x < PADDLE_WIDTH; x++) {
        set_pixel(screen, PADDLE_HIGH_Y, paddle_high_x + x, COLOR_WHITE);
    }

    // low paddle
    for (x = 0; x < PADDLE_WIDTH; x++) {
        set_pixel(screen, PADDLE_LOW_Y, paddle_low_x + x, COLOR_WHITE);
    }

    // ball
    y = lround(ball_y);
    x = lround(ball_x);
    if (y >= 0 && y < GRID_HEIGHT && x >= 0 && x < GRID_WIDTH) {
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
