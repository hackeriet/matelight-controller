/* based on this: https://github.com/mamikk/tetris */

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "matelight.h"

#define MODE_GAME               0
#define MODE_DEAD               1

#define TETRIS_WIDTH            GRID_WIDTH
#define TETRIS_HEIGHT           (GRID_HEIGHT + 2)
#define TETRIS_VISIBLE_HEIGHT   (TETRIS_HEIGHT - 2)
#define TETRIS_BLOCK_START_Y    TETRIS_VISIBLE_HEIGHT
#define TETRIS_NUM_BRICKS       7

/* blocks: */
/* I, J, L, O, S, T, and Z */

static const unsigned int tetris_block_colors[TETRIS_NUM_BRICKS] = {
    /* I block */ COLOR_CYAN,
    /* J block */ COLOR_BLUE,
    /* L block */ COLOR_ORANGE,
    /* O block */ COLOR_YELLOW,
    /* S block */ COLOR_GREEN,
    /* T block */ COLOR_MAGENTA,
    /* Z block */ COLOR_RED,
};

struct tetris_block_rotations {
    const char rotations[4][4][4];
};

static const struct tetris_block_rotations tetris_blocks_rotations[TETRIS_NUM_BRICKS] = {
    /* I block */
    {
        {
            {
                "    ",
                "####",
                "    ",
                "    "
            },
            {
                "  # ",
                "  # ",
                "  # ",
                "  # "
            },
            {
                "    ",
                "    ",
                "####",
                "    "
            },
            {
                " #  ",
                " #  ",
                " #  ",
                " #  "
            }
        }
    },

    /* J block */
    {
        {
            {
                "#   ",
                "### ",
                "    ",
                "    "
            },
            {
                " ## ",
                " #  ",
                " #  ",
                "    "
            },
            {
                "    ",
                "### ",
                "  # ",
                "    "
            },
            {
                " #  ",
                " #  ",
                "##  ",
                "    "
            }
        }
    },

    /* L block */
    {
        {
            {
                "  # ",
                "### ",
                "    ",
                "    "
            },
            {
                " #  ",
                " #  ",
                " ## ",
                "    "
            },
            {
                "    ",
                "### ",
                "#   ",
                "    "
            },
            {
                "##  ",
                " #  ",
                " #  ",
                "    "
            }
        }
    },

    /* O block */
    {
        {
            {
                " ## ",
                " ## ",
                "    ",
                "    "
            },
            {
                " ## ",
                " ## ",
                "    ",
                "    "
            },
            {
                " ## ",
                " ## ",
                "    ",
                "    "
            },
            {
                " ## ",
                " ## ",
                "    ",
                "    "
            },
        }
    },

    /* S block */
    {
        {
            {
                " ## ",
                "##  ",
                "    ",
                "    "
            },
            {
                " #  ",
                " ## ",
                "  # ",
                "    "
            },
            {
                "    ",
                " ## ",
                "##  ",
                "    "
            },
            {
                "#   ",
                "##  ",
                " #  ",
                "    "
            }
        }
    },

    /* T block */
    {
        {
            {
                " #  ",
                "### ",
                "    ",
                "    "
            },
            {
                " #  ",
                " ## ",
                " #  ",
                "    "
            },
            {
                "    ",
                "### ",
                " #  ",
                "    "
            },
            {
                " #  ",
                "##  ",
                " #  ",
                "    "
            }
        }
    },

    /* Z block */
    {
        {
            {
                "##  ",
                " ## ",
                "    ",
                "    "
            },
            {
                "  # ",
                " ## ",
                " #  ",
                "    "
            },
            {
                "    ",
                "##  ",
                " ## ",
                "    "
            },
            {
                " #  ",
                "##  ",
                "#   ",
                "    "
            }
        }
    }
};

typedef int tetris_field[TETRIS_WIDTH][TETRIS_HEIGHT];

struct tetris {
    tetris_field field;
    double next_update;
    int curblock;
    int curblock_x, curblock_y;
    int nextblock;
    int rotation;
    double last_tetris;
    unsigned int score;
    unsigned int level;
};

static int game_mode = MODE_DEAD;
static bool game_pause = false;
static double pause_start = 0.0;
static struct tetris _tetris = { 0 };
static struct tetris *tetris = &_tetris;

static void setup_game(bool start)
{
    int x, y;

    game_mode = start ? MODE_GAME : MODE_DEAD;
    game_pause = false;
    pause_start = 0.0;

    memset(tetris, '\0', sizeof(*tetris));

    for (x = 0; x < TETRIS_WIDTH; ++x) {
        for (y = 0; y < TETRIS_HEIGHT; ++y) {
            tetris->field[x][y] = 0;
        }
    }

    tetris->next_update = time_val;

    tetris->curblock = rand() % TETRIS_NUM_BRICKS;
    tetris->curblock_x = (TETRIS_WIDTH / 2) - (4 / 2);
    tetris->curblock_y = TETRIS_BLOCK_START_Y;
    tetris->nextblock = rand() % TETRIS_NUM_BRICKS;
    tetris->rotation = 0;
    tetris->last_tetris = -100.0;
    tetris->score = 0;
    tetris->level = 0;
}

static void activate(bool start)
{
    setup_game(start);
}

static void deactivate(void)
{
    setup_game(false);
}

static void field_apply(tetris_field field1, tetris_field field2)
{
    int x, y;

    for (x = 0; x < TETRIS_WIDTH; ++x) {
        for (y = 0; y < TETRIS_HEIGHT; ++y) {
            if (field2[x][y])
                field1[x][y] = field2[x][y];
        }
    }
}

static void write_block(tetris_field field, int i, int r, int x, int y)
{
    int px, py;

    for (px = 0; px < 4; ++px) {
        for (py = 0; py < 4; ++py) {
            int v = tetris_blocks_rotations[i].rotations[r][py][px] == '#';
            int tx = x + px;
            int ty = y + py;
            if (v && tx >= 0 && tx < TETRIS_WIDTH && ty >= 0 && ty < TETRIS_HEIGHT)
                field[tx][ty] = (i + 1);
        }
    }
}

static bool valid_block(tetris_field tstfield, int i, int r, int x, int y)
{
    int px, py;
    tetris_field curblock = { { 0 } };

    for (px = 0; px < 4; ++px) {
        for (py = 0; py < 4; ++py) {
            int v = tetris_blocks_rotations[i].rotations[r][py][px] == '#';
            int tx = x + px;
            int ty = y + py;
            if (! v)
                continue;
            if (v && tx >= 0 && tx < TETRIS_WIDTH && ty >= 0 && ty < TETRIS_HEIGHT)
                continue;
            return false;
        }
    }

    write_block(curblock, i, r, x, y);

    for (x = 0; x < TETRIS_WIDTH; ++x) {
        for (y = 0; y < TETRIS_HEIGHT; ++y) {
            if (! curblock[x][y]) continue;
            if (tstfield[x][y]) return false;
        }
    }

    return true;
}

static bool block_landed(void)
{
    int x, y;

    tetris_field curblock = { { 0 } };

    write_block(curblock, tetris->curblock, tetris->rotation, tetris->curblock_x, tetris->curblock_y);

    for (x = 0; x < TETRIS_WIDTH; ++x) {
        if (curblock[x][0]) return true;
    }

    for (x = 0; x < TETRIS_WIDTH; ++x) {
        for (y = 0; y < TETRIS_HEIGHT; ++y) {
            if (! tetris->field[x][y]) continue;
            if (curblock[x][y + 1]) return true;
        }
    }

    return false;
}

static bool block_crashed(void)
{
    int x, y;

    tetris_field curblock = { { 0 } };

    write_block(curblock, tetris->curblock, tetris->rotation, tetris->curblock_x, tetris->curblock_y);

    for (x = 0; x < TETRIS_WIDTH; ++x) {
        for (y = 0; y < TETRIS_HEIGHT; ++y) {
            if (! tetris->field[x][y]) continue;
            if (curblock[x][y]) return true;
        }
    }

    return false;
}

static int check_tetris(void)
{
    int ret = 0;

    for (;;) {
        int x, y;

        for (y = 0; y < TETRIS_HEIGHT; ++y) {
            bool allright = true;
            for (x = 0; x < TETRIS_WIDTH; ++x) {
                if (! tetris->field[x][y]) allright = false;
            }
            if (allright) goto doit;
        }
        break;

        doit:
        ++ret;

        for (; y < (TETRIS_HEIGHT - 1); ++y) {
            for (x = 0; x < TETRIS_WIDTH; ++x) {
                tetris->field[x][y] = tetris->field[x][y + 1];
            }
        }

        for (x = 0; x < TETRIS_WIDTH; ++x)
            tetris->field[x][TETRIS_HEIGHT - 1] = 0;
    }

    return ret;
}

static void iterate(void)
{
    tetris->curblock_y--;
}

/* left: move block left */
static void key_left(void)
{
    if (valid_block(tetris->field, tetris->curblock, tetris->rotation, tetris->curblock_x - 1, tetris->curblock_y))
        tetris->curblock_x--;
}

/* right: move block right */
static void key_right(void)
{
    if (valid_block(tetris->field, tetris->curblock, tetris->rotation, tetris->curblock_x + 1, tetris->curblock_y))
        tetris->curblock_x++;
}

/* up: rotate block clockwise */
static void key_up(void)
{
    int next_rotation = tetris->rotation;

    next_rotation++;
    if (next_rotation >= 4) next_rotation = 0;

    if (valid_block(tetris->field, tetris->curblock, next_rotation, tetris->curblock_x, tetris->curblock_y))
        tetris->rotation = next_rotation;
}

/* down: move block down */
static void key_down(void)
{
    if (! block_landed()) iterate();
}

/* space: drop block */
static void key_drop(void)
{
    while (! block_landed()) iterate();
}

static bool doit(void)
{
    bool is_game_over = false;
    double u;

    if (game_pause)
        return ! is_game_over;

    if (time_val >= tetris->next_update) {
        if (block_landed()) {
            tetris_field curblock = { { 0 } };
            int nt;

            write_block(curblock, tetris->curblock, tetris->rotation, tetris->curblock_x, tetris->curblock_y);
            field_apply(tetris->field, curblock);

            tetris->curblock = tetris->nextblock;;
            tetris->curblock_x = (TETRIS_WIDTH / 2) - (4 / 2);
            tetris->curblock_y = TETRIS_BLOCK_START_Y;
            tetris->nextblock = rand() % TETRIS_NUM_BRICKS;
            tetris->rotation = 0;

            nt = check_tetris();
            tetris->level += nt;
            if (nt == 1) tetris->score += 1000;
            if (nt == 2) tetris->score += 2000;
            if (nt == 3) tetris->score += 4000;
            if (nt == 4) {
                tetris->score += 10000;
                tetris->last_tetris = time_val;
            }

            if (block_crashed()) {
                is_game_over = true;
            }
        } else {
            iterate();
        }
        u = 5.0 - log((double)( (tetris->level / 4) + 1));
        u /= 10.0;
        if (u <= 0.05)
            u = 0.05;
        tetris->next_update = time_val + u;

    }

    return ! is_game_over;
}

static void draw(char *screen)
{
    int x, y;

    tetris_field field = { { 0 } };
    tetris_field curblock = { { 0 } };

    for (y = 0; y < GRID_HEIGHT; y++) {
        for (x = 0; x < GRID_WIDTH; x++) {
            set_pixel(screen, y, x, COLOR_BLACK);
        }
    }

    write_block(curblock, tetris->curblock, tetris->rotation, tetris->curblock_x, tetris->curblock_y);
    memcpy(field, tetris->field, sizeof(field));
    field_apply(field, curblock);

    /* playfield */
    for (x = 0; x < TETRIS_WIDTH; ++x) {
        for (y = 0; y < TETRIS_VISIBLE_HEIGHT; ++y) {
            unsigned int color;
            int v;

            v = field[x][y];
            if (v) {
                color = tetris_block_colors[v - 1];
                set_pixel(screen, (GRID_HEIGHT - y) - 1, x, color);
            }
        }
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
    (void)player;
    (void)key_state;

    switch (game_mode) {
        case MODE_GAME:
            if (key_idx == KEYPAD_LEFT && key_val) {
                key_left();
            } else if (key_idx == KEYPAD_RIGHT && key_val) {
                key_right();
            } else if (key_idx == KEYPAD_UP && key_val) {
                key_up();
            } else if (key_idx == KEYPAD_DOWN && key_val) {
                key_down();
            }

            if (key_idx == KEYPAD_START && key_val) {
                if (! game_pause) {
                    game_pause = true;
                    pause_start = time_val;
                } else {
                    game_pause = false;
                    tetris->next_update += (time_val - pause_start);
                    pause_start = 0.0;
                }
            }
            if ((key_idx == KEYPAD_B || key_idx == KEYPAD_A) && key_val) {
                key_drop();
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

const struct game tetris_game = {
    "tetris",
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
