#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "matelight.h"

#define MODE_GAME       0
#define MODE_DEAD       1

#define MOVE_NONE       0
#define MOVE_UP         1
#define MOVE_LEFT       2
#define MOVE_RIGHT      3
#define MOVE_DOWN       4

#define SNAKE_START     3
#define SNAKE_FOOD      2
#define SNAKE_SUPERFOOD 6

#define OBJ_EMPTY       0
#define OBJ_WALL        1
#define OBJ_SNAKE       2
#define OBJ_SNAKEHEAD   3
#define OBJ_FOOD        4
#define OBJ_SUPERFOOD   5
#define OBJ_POISON      6

#define WANTED_FOOD     3
#define WANTED_POISON   2
#define SUPERFOOD_FREQ  5

#define ROTATE_TICKS    25

#define COLOR_EMPTY     COLOR_BLACK
#define COLOR_WALL      COLOR_WHITE
#define COLOR_SNAKE     COLOR_LIGHT_GRAY
#define COLOR_SNAKEHEAD COLOR_ORANGE
#define COLOR_FOOD      COLOR_GREEN
#define COLOR_SUPERFOOD COLOR_MAGENTA
#define COLOR_POISON    COLOR_RED

static int game_mode = MODE_DEAD;
static bool game_pause = false;
static int game_speed = 0;
static int game_level = 0;
static int tick_count = 0;
static int move = MOVE_NONE;
static int diry;
static int dirx;
static int numfood;
static int numpoison;
static int rotatecnt;
static int snakelen;
static int wantedlen;
static unsigned char grid[GRID_WIDTH * GRID_HEIGHT];
static unsigned char snake[GRID_WIDTH * GRID_HEIGHT * 2];
static unsigned char objs[GRID_WIDTH * GRID_HEIGHT * 2];

static void grid_set(int y, int x, unsigned char type)
{
    grid[(y * GRID_WIDTH) + x] = type;
}

static void add_object(unsigned char type)
{
    int ry, rx;
    int yloop, xloop;
    int objy, objx;

    if (type == OBJ_SNAKEHEAD) {
        ry = GRID_HEIGHT / 2;
        rx = GRID_WIDTH / 2;
    } else {
        ry = rand() % GRID_HEIGHT;
        rx = rand() % GRID_WIDTH;
    }

    for (yloop = 0; yloop < GRID_HEIGHT; yloop++) {
        for (xloop = 0; xloop < GRID_WIDTH; xloop++) {
            objy = (ry + yloop) % GRID_HEIGHT;
            objx = (rx + xloop) % GRID_WIDTH;

            if (grid[(objy * GRID_WIDTH) + objx] == OBJ_EMPTY) {
                grid_set(objy, objx, type);

                if (type == OBJ_FOOD || type == OBJ_SUPERFOOD) {
                    objs[((numfood + numpoison) * 2) + 0] = objy;
                    objs[((numfood + numpoison) * 2) + 1] = objx;
                    numfood++;
                } else if (type == OBJ_POISON) {
                    objs[((numfood + numpoison) * 2) + 0] = objy;
                    objs[((numfood + numpoison) * 2) + 1] = objx;
                    numpoison++;
                } else if (type == OBJ_SNAKEHEAD) {
                    snake[0] = objy;
                    snake[1] = objx;
                    snakelen = 1;
                }

                return;
            }
        }
    }
}

static void remove_obj(int objy, int objx)
{
    int i, j;
    unsigned char type = grid[(objy * GRID_WIDTH) + objx];

    for (i = 0; i < (numfood + numpoison); i++) {
        if (objs[(i * 2) + 0] == objy && objs[(i * 2) + 1] == objx) {
            for (j = i; j < ((numfood + numpoison) - 1); j++) {
                objs[(j * 2) + 0] = objs[(j * 2) + 2];
                objs[(j * 2) + 1] = objs[(j * 2) + 3];
            }
            break;
        }
    }

    if (type == OBJ_FOOD || type == OBJ_SUPERFOOD) {
        numfood--;
    } else if (type == OBJ_POISON) {
        numpoison--;
    }

    grid_set(objy, objx, OBJ_EMPTY);
}

static void rotate_objects(void)
{
    int objy, objx;

    if ((numfood + numpoison) <= 0) return;

    objy = objs[0];
    objx = objs[1];

    remove_obj(objy, objx);
}

static void setup_game(bool start)
{
    int y, x;

    game_mode = start ? MODE_GAME : MODE_DEAD;
    game_pause = false;
    game_speed = 0;
    tick_count = 0;
    move = MOVE_NONE;
    diry = 0;
    dirx = 1;
    numfood = 0;
    numpoison = 0;
    rotatecnt = 0;
    snakelen = 0;
    wantedlen = SNAKE_START;
    memset(grid, '\0', sizeof(grid));
    memset(snake, '\0', sizeof(snake));

    if (game_level) {
        for (x = 0; x < GRID_WIDTH; x++) {
            grid[(0 * GRID_WIDTH) + x] = OBJ_WALL;
            grid[((GRID_HEIGHT - 1) * GRID_WIDTH) + x] = OBJ_WALL;
        }
        for (y = 1; y < (GRID_HEIGHT - 1); y++) {
            grid[(y * GRID_WIDTH) + 0] = OBJ_WALL;
            grid[(y * GRID_WIDTH) + (GRID_WIDTH - 1)] = OBJ_WALL;
        }
    }

    add_object(OBJ_SNAKEHEAD);
}

static void activate(bool start)
{
    setup_game(start);
}

static void deactivate(void)
{
    setup_game(false);
}

static void update_dir(void)
{
    switch (move) {
        case MOVE_UP:
            if (diry == 0) {
                diry = -1;
                dirx = 0;
            }
            break;
        case MOVE_LEFT:
            if (dirx == 0) {
                diry = 0;
                dirx = -1;
            }
            break;
        case MOVE_RIGHT:
            if (dirx == 0) {
                diry = 0;
                dirx = 1;
            }
            break;
        case MOVE_DOWN:
            if (diry == 0) {
                diry = 1;
                dirx = 0;
            }
            break;
    }

    move = MOVE_NONE;
}

static void move_snake(void)
{
    int heady = snake[((snakelen - 1) * 2) + 0];
    int headx = snake[((snakelen - 1) * 2) + 1];
    unsigned char type;

    heady = heady + diry;
    if (heady < 0) heady = GRID_HEIGHT - 1;
    else if (heady >= GRID_HEIGHT) heady = 0;

    headx = headx + dirx;
    if (headx < 0) headx = GRID_WIDTH - 1;
    else if (headx >= GRID_WIDTH) headx = 0;

    type = grid[(heady * GRID_WIDTH) + headx];

    switch (type) {
        case OBJ_WALL:
        case OBJ_SNAKE:
        case OBJ_SNAKEHEAD:
        case OBJ_POISON:
            game_mode = MODE_DEAD;
            break;
        case OBJ_FOOD:
        case OBJ_SUPERFOOD:
            /* eat food */
            remove_obj(heady, headx);
            wantedlen += (type == OBJ_SUPERFOOD ? SNAKE_SUPERFOOD : SNAKE_FOOD);
            /* fall through */

        case OBJ_EMPTY:
        default:
            /* move snake */
            if (snakelen < wantedlen) {
                snakelen++;
            } else {
                int taily = snake[0];
                int tailx = snake[1];
                int i;

                grid_set(taily, tailx, OBJ_EMPTY);

                if (snakelen >= 2) {
                    //__builtin_memmove(snake, snake + 2, snakelen - 1);
                    for (i = 0; i < (snakelen - 1); i++) {
                        snake[(i * 2) + 0] = snake[(i * 2) + 2];
                        snake[(i * 2) + 1] = snake[(i * 2) + 3];
                    }
                }
            }

            snake[((snakelen - 1) * 2) + 0] = heady;
            snake[((snakelen - 1) * 2) + 1] = headx;
            grid_set(heady, headx, OBJ_SNAKEHEAD);

            if (snakelen >= 2) {
                int bodyy = snake[((snakelen - 2) * 2) + 0];
                int bodyx = snake[((snakelen - 2) * 2) + 1];

                grid_set(bodyy, bodyx, OBJ_SNAKE);
            }

            break;
    }
}

static void tick(void)
{
    if (game_mode != MODE_GAME) return;

    if (game_pause) return;

    tick_count++;
    if (game_speed && (tick_count % 4) != 0) return;

    update_dir();

    /* move_snake */
    if (diry != 0 || dirx != 0) {
        move_snake();
    }

    if (game_mode != MODE_GAME) return;

    rotatecnt++;
    if (rotatecnt >= ROTATE_TICKS) {
        rotate_objects();
        rotatecnt = 0;
    }

    /* add food */
    while (numfood < WANTED_FOOD) {
        add_object((rand() % SUPERFOOD_FREQ) == 0 ? OBJ_SUPERFOOD : OBJ_FOOD);
    }

    /* add poison */
    while (numpoison < WANTED_POISON) {
        add_object(OBJ_POISON);
    }
}

static void draw_block(char *screen, int y, int x, unsigned int color, int fade)
{
    unsigned char r = (color >> 16) & 0xff;
    unsigned char g = (color >> 8) & 0xff;
    unsigned char b = color & 0xff;

    if (fade) {
        int pos = (int)(time_val * 1000.0) % 1000;
        if (pos > 500) pos = 500 - (pos - 500);
        pos *= 2;

        r = ((int)r*pos) / 1000;
        g = ((int)g*pos) / 1000;
        b = ((int)b*pos) / 1000;
    }

    set_pixel(screen, y, x, COLOR_RGB(r, g, b));
}

static void draw_obj(char *screen, int y, int x)
{
    unsigned char type;
    int color;

    type = grid[(y * GRID_WIDTH) + x];
    color = COLOR_EMPTY;

    switch (type) {
        case OBJ_WALL:
            color = COLOR_WALL;
            break;
        case OBJ_SNAKE:
            color = COLOR_SNAKE;
            break;
        case OBJ_SNAKEHEAD:
            color = COLOR_SNAKEHEAD;
            break;
        case OBJ_FOOD:
            color = COLOR_FOOD;
            break;
        case OBJ_SUPERFOOD:
            color = COLOR_SUPERFOOD;
            break;
        case OBJ_POISON:
            color = COLOR_POISON;
            break;
    }

    switch (type) {
        /* clear all */
        default:
            draw_block(screen, y, x, color, 0);
            break;

        /* box with 1px border */
        case OBJ_WALL:
        case OBJ_SNAKE:
            draw_block(screen, y, x, color, 0);
            break;

        case OBJ_SNAKEHEAD:
            draw_block(screen, y, x, color, 0);
            break;

        /* circle */
        case OBJ_FOOD:
        case OBJ_SUPERFOOD:
        case OBJ_POISON:
            draw_block(screen, y, x, color, 1);
            break;
    }
}

static void input(int player, int key_idx, bool key_val, int key_state)
{
    (void)player;
    (void)key_state;

    switch (game_mode) {
        case MODE_GAME:
            if (key_idx == KEYPAD_LEFT && key_val) {
                move = MOVE_LEFT;
            } else if (key_idx == KEYPAD_RIGHT && key_val) {
                move = MOVE_RIGHT;
            } else if (key_idx == KEYPAD_UP && key_val) {
                move = MOVE_UP;
            } else if (key_idx == KEYPAD_DOWN && key_val) {
                move = MOVE_DOWN;
            }

            if (key_idx == KEYPAD_START && key_val) {
                if (! game_pause) {
                    game_pause = true;
                } else {
                    game_pause = false;
                }
            }
            if (key_idx == KEYPAD_B && key_val) {
                game_speed = ! game_speed;
            }
            if (key_idx == KEYPAD_A && key_val) {
                game_level = ! game_level;
                setup_game(true);
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

static void render(bool *display, char *screen)
{
    int y, x;

    if (game_mode == MODE_GAME) {
        *display = true;

        for (y = 0; y < GRID_HEIGHT; y++) {
            for (x = 0; x < GRID_WIDTH; x++) {
                draw_obj(screen, y, x);
            }
        }
    } else {
        *display = false;
    }
}

static bool idle(void)
{
    return game_mode != MODE_GAME;
}

const struct game snake_game = {
    "snake",
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
