/* breakout */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "matelight.h"

#define MODE_GAME               0
#define MODE_DEAD               1

#define PADDLE_WIDTH            4
#define PADDLE_Y                (grid_height - 2)
#define MOVE_NONE               0
#define MOVE_LEFT               -1
#define MOVE_RIGHT              1

#define DIR_UP                  (M_PI * 1.5)
#define DIR_DOWN                (M_PI * 0.5)

#define BRICK_START_ROW         3
#define BRICK_ROWS              6

static int game_mode = MODE_DEAD;
static bool game_pause = false;

static int paddle_x = 0;
static int paddle_dir = MOVE_NONE;

static double ball_y = 0.0;
static double ball_x = 0.0;
static double ball_dir = DIR_DOWN;

static uint8_t bricks[BRICK_ROWS * MAX_GRID_WIDTH] = { 0 };
static size_t num_bricks = 0;
static uint32_t brick_colors[BRICK_ROWS * MAX_GRID_WIDTH] = { 0 };

static const uint32_t brick_row_colors[BRICK_ROWS] = {
    /* pride */
    COLOR_RGB(228, 3, 3),
    COLOR_RGB(255, 140, 0),
    COLOR_RGB(255, 237, 0),
    COLOR_RGB(0, 128, 38),
    COLOR_RGB(36, 64, 142),
    COLOR_RGB(115, 41, 130),
};

static void setup_game(bool start)
{
    int y, x;

    game_mode = start ? MODE_GAME : MODE_DEAD;
    game_pause = false;

    paddle_x = (grid_width / 2) - (PADDLE_WIDTH / 2);
    paddle_dir = MOVE_NONE;

    ball_y = (double)(grid_height / 2);
    ball_x = (double)(grid_width / 2);
    ball_dir = DIR_DOWN;
    num_bricks = 0;

    for (y = 0; y < BRICK_ROWS; y++) {
        for (x = 0; x < grid_width; x++) {
            bricks[(y * grid_width) + x] = 1;
            num_bricks++;
            brick_colors[(y * grid_width) + x] = brick_row_colors[y];
        }
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
    int ball_yi, ball_xi;

    if (game_pause)
        return true;

    /* move paddle */
    paddle_x += paddle_dir;
    if (paddle_x < 0) paddle_x = 0;
    if (paddle_x > (grid_width - PADDLE_WIDTH)) paddle_x = grid_width - PADDLE_WIDTH;

    /* move ball */
    ball_x += cos(ball_dir);
    ball_y += sin(ball_dir);

    /* check if ball is outside grid */
    if (ball_y >= (double)grid_height) {
        ball_y = -1.0;
        ball_x = -1.0;
        return false;
    }

    /* check ball/paddle collision */
    if (lround(ball_y) == PADDLE_Y && lround(ball_x) >= paddle_x && lround(ball_x) < (paddle_x + PADDLE_WIDTH)) {
        hit_offset = ((((ball_x - (double)paddle_x) + 0.5) / (double)PADDLE_WIDTH) * 2.0) - 1.0;

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

    /* check ball/bricks collision */
    if (lround(floor(ball_y)) >= BRICK_START_ROW && lround(floor(ball_y)) < (BRICK_START_ROW + BRICK_ROWS)) {
        ball_yi = lround(floor(ball_y)) - BRICK_START_ROW;
        ball_xi = lround(floor(ball_x));

        if (bricks[(ball_yi * grid_width) + ball_xi]) {
            bricks[(ball_yi * grid_width) + ball_xi] = 0;
            if (num_bricks > 0)
                num_bricks--;
            ball_dir = atan2(sin(ball_dir) * -1, cos(ball_dir));
        }
    }

    if (num_bricks == 0) {
        ball_y = -1.0;
        ball_x = -1.0;
        return false;
    }

    /* check ball/top wall collision */
    if (ball_y < 0.0) {
        ball_dir = atan2(sin(ball_dir) * -1, cos(ball_dir));
        ball_y = 0.0;
    }

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

    return true;
}

static void move(int direction)
{
    paddle_dir = direction;
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

    // bricks
    for (y = 0; y < BRICK_ROWS; y++) {
        for (x = 0; x < grid_width; x++) {
            if (bricks[(y * grid_width) + x]) {
                set_pixel(screen, y + BRICK_START_ROW, x, brick_colors[(y * grid_width) + x]);
            }
        }
    }

    // paddle
    for (x = 0; x < PADDLE_WIDTH; x++) {
        set_pixel(screen, PADDLE_Y, paddle_x + x, COLOR_WHITE);
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

const struct game breakout_game = {
    "breakout",
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
