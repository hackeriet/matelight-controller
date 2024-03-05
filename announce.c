#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "matelight.h"

#include "font8x8/font8x8.h"

#define FONT_SIZE       8

#define MODE_GAME       0
#define MODE_DEAD       1

static int game_mode = MODE_DEAD;
static int tick_count = 0;
static char *announce_text = NULL;
static size_t announce_wlen = '\0';
static wchar_t *announce_wtext = NULL;
static unsigned int announce_color = COLOR_RGB(0xff, 0xff, 0xff);
static unsigned int announce_bgcolor = COLOR_RGB(0x00, 0x00, 0x00);
static int announce_rotate = 0;
static double announce_speed = 1.0;
static int announce_pos = 0;

static void reset(void)
{
    game_mode = MODE_DEAD;
    if (announce_text) {
        free(announce_text);
        announce_text = NULL;
    }
    announce_wlen = 0;
    if (announce_wtext) {
        free(announce_wtext);
        announce_wtext = NULL;
    }
}

void set_announce_text(const char *text, unsigned int color, unsigned int bgcolor, int rotate, double speed)
{
    mbstate_t state = { 0 };
    size_t len;
    const char *src;

    reset();
    announce_text = strdup(text);
    if (! announce_text) {
        reset();
        return;
    }

    memset(&state, '\0', sizeof(state));
    src = text;
    len = mbsrtowcs(NULL, &src, 0, &state);
    if (len == (size_t)-1) {
        reset();
        return;
    }

    announce_wlen = len;
    announce_wtext = malloc(sizeof(wchar_t) * (announce_wlen + 1));
    if (! announce_wtext) {
        reset();
        return;
    }

    memset(&state, '\0', sizeof(state));
    src = text;
    len = mbsrtowcs(announce_wtext, &src, announce_wlen + 1, &state);
    if (len != announce_wlen) {
        reset();
        return;
    }

    announce_color = color;
    announce_bgcolor = bgcolor;
    announce_rotate = rotate;
    announce_speed = speed;
    announce_pos = 0;
}

// https://github.com/dhepper/font8x8
static const char *get_font8x8(wchar_t ch)
{
    // Contains an 8x8 font map for unicode points U+0000 - U+007F (basic latin)
    if (/* ch >= 0x0000 && */ ch <= 0x007f) {
        return font8x8_basic[ch - 0x0000];

    // Contains an 8x8 font map for unicode points U+0080 - U+009F (C1/C2 control)
    } else if (ch >= 0x0080 && ch <= 0x009f) {
        return font8x8_control[ch - 0x0080];

    // Contains an 8x8 font map for unicode points U+00A0 - U+00FF (extended latin)
    } else if (ch >= 0x00a0 && ch <= 0x00ff) {
        return font8x8_ext_latin[ch - 0x00a0];

    // Contains an 8x8 font map for unicode points U+0390 - U+03C9 (greek characters)
    } else if (ch >= 0x0390 && ch <= 0x03c9) {
        return font8x8_greek[ch - 0x0390];

    // Contains an 8x8 font map for unicode points U+2500 - U+257F (box drawing)
    } else if (ch >= 0x2500 && ch <= 0x257f) {
        return font8x8_box[ch - 0x2500];

    // Contains an 8x8 font map for unicode points U+2580 - U+259F (block elements)
    } else if (ch >= 0x2580 && ch <= 0x259f) {
        return font8x8_block[ch - 0x2580];

    // Contains an 8x8 font map for unicode points U+3040 - U+309F (Hiragana)
    } else if (ch >= 0x3040 && ch <= 0x309f) {
        return font8x8_hiragana[ch - 0x3040];

    } else {
        return font8x8_basic['?'];
    }
}

static void setup_game(bool start)
{
    game_mode = start ? MODE_GAME : MODE_DEAD;
    tick_count = 0;
    announce_pos = 0;
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

    announce_pos = -GRID_HEIGHT + (int)(((double)tick_count * announce_game.tick_freq) * announce_speed);

    if (announce_pos > ((int)announce_wlen*FONT_SIZE)) {
        game_mode = MODE_DEAD;
    }
}

static void input(int player, int key_idx, bool key_val, int key_state)
{
    (void)player;
    (void)key_state;

    switch (game_mode) {
        case MODE_GAME:
            switch (key_idx) {
                case KEYPAD_SELECT:
                case KEYPAD_START:
                case KEYPAD_B:
                case KEYPAD_A:
                    if (key_val) {
                        setup_game(false);
                    }
                    break;
                default:
                    break;
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
            break;
    }
}

static bool get_glyph_pix(const char *glyph, int y, int x, int rotate)
{
    if (rotate) {
        return (glyph[(FONT_SIZE - 1) - x] >> y) & 1;
    } else {
        return (glyph[y] >> x) & 1;
    }
}

static void draw(char *screen)
{
    int y, x;
    int font_idx;
    int font_off;
    wchar_t ch;
    const char *glyph;
    bool pix;

    for (y = 0; y < GRID_HEIGHT; y++) {
        font_idx = (announce_pos + y) / FONT_SIZE;
        font_off = (announce_pos + y) % FONT_SIZE;
        if (font_idx >= 0 && font_idx < (int)announce_wlen) {
            ch = announce_wtext[font_idx];
        } else {
            ch = (wchar_t)' ';
        }
        glyph = get_font8x8(ch);

        for (x = 0; x < GRID_WIDTH; x++) {
            pix = false;
            if (x >= ((GRID_WIDTH - FONT_SIZE) / 2) && x < FONT_SIZE + ((GRID_WIDTH - FONT_SIZE) / 2)) {
                pix = get_glyph_pix(glyph, font_off, x - ((GRID_WIDTH - FONT_SIZE) / 2), announce_rotate);
            }
            if (pix) {
                set_pixel(screen, y, x, announce_color);
            } else {
                set_pixel(screen, y, x, announce_bgcolor);
            }
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

const struct game announce_game = {
    "announce",
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
