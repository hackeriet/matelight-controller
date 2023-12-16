/* snake */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

#define GRID_WIDTH      10
#define GRID_HEIGHT     20

#define MODE_GAME       0
#define MODE_DEAD       1
#define MODE_EXIT       2

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

#define COLOR_EMPTY     COLOR_BLACK
#define COLOR_WALL      COLOR_WHITE
#define COLOR_SNAKE     COLOR_GREY
#define COLOR_SNAKEHEAD COLOR_ORANGE
#define COLOR_FOOD      COLOR_GREEN
#define COLOR_SUPERFOOD COLOR_MAGENTA
#define COLOR_POISON    COLOR_RED

// palette
static const char rgb_color[16 * 3] = {
    0x00, 0x00, 0x00,   /* Black */
    0x00, 0x00, 0x80,   /* Blue */
    0x00, 0x80, 0x00,   /* Green */
    0x00, 0x80, 0x80,   /* Cyan */
    0x80, 0x00, 0x00,   /* Red */
    0x80, 0x00, 0x80,   /* Magenta */
    0x80, 0x80, 0x00,   /* Brown */
    0xc0, 0xc0, 0xc0,   /* Light Gray */
    0x80, 0x80, 0x80,   /* Dark Gray */
    0x00, 0x00, 0xff,   /* Light Blue */
    0x00, 0xff, 0x00,   /* Light Green */
    0x00, 0xff, 0xff,   /* Light Cyan */
    0xff, 0x00, 0x00,   /* Light Red */
    0xff, 0x00, 0xff,   /* Light Magenta */
    0xff, 0xff, 0x00,   /* Yellow */
    0xff, 0xff, 0xff,   /* White */
};

static struct sockaddr_in udp_sockaddr;
static int udp_fd;
static unsigned char game_mode;
static unsigned char game_pause;
static unsigned char move;
static unsigned int thistick;
static unsigned int lasttick;
static short diry;
static short dirx;
static short numfood;
static short numpoison;
static short rotatecnt;
static unsigned char alldirty;
static unsigned short dirtylen;
static unsigned short snakelen;
static unsigned short wantedlen;
static unsigned char grid[GRID_WIDTH * GRID_HEIGHT];
static unsigned char snake[GRID_WIDTH * GRID_HEIGHT * 2];
static unsigned char objs[GRID_WIDTH * GRID_HEIGHT * 2];
static unsigned char dirtycells[GRID_WIDTH * GRID_HEIGHT * 2];
static unsigned char udp_data[2 + (GRID_WIDTH * GRID_HEIGHT * 3)] = { 2 /* DGRB */, 1 /* seconds */ };
static struct termios told, tnew;

static void init()
{
    int opt;
    srand(time(NULL));

    tcgetattr(0, &told);
    tnew = told;
    tnew.c_lflag &= ~ICANON;
    tnew.c_cc[VMIN] = 1;
    tnew.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &tnew);


    opt = fcntl(0, F_GETFL);
    if (opt >= 0) {
        opt |= O_NONBLOCK;
        fcntl(0, F_SETFL, opt);
    }

    udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_fd == -1) perror("socket");
}

static void grid_set(short y, short x, unsigned char type)
{
    grid[(y * GRID_WIDTH) + x] = type;
    dirtycells[(dirtylen * 2) + 0] = y;
    dirtycells[(dirtylen * 2) + 1] = x;
    dirtylen++;
}

static void add_object(unsigned char type)
{
    short ry, rx;
    short yloop, xloop;
    short objy, objx;

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

static void remove_obj(short objy, short objx)
{
    short i, j;
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
    short objy, objx;

    if ((numfood + numpoison) <= 0) return;

    objy = objs[0];
    objx = objs[1];

    remove_obj(objy, objx);
}

static void new_game(void)
{
    game_mode = MODE_GAME;
    game_pause = 0;
    move = MOVE_NONE;
    thistick = 0;
    lasttick = 0;
    diry = 0;
    dirx = 0;
    numfood = 0;
    numpoison = 0;
    rotatecnt = 0;
    alldirty = 1;
    dirtylen = 0;
    snakelen = 0;
    wantedlen = SNAKE_START;
    memset(grid, '\0', sizeof(grid));
    //memset(snake, '\0', sizeof(snake));
    //memset(dirtycells, '\0', sizeof(dirtycells));

    add_object(OBJ_SNAKEHEAD);
}

static void handle_input(void)
{
    unsigned char key;

    if (read(0, &key, 1) != 1)
        return;

    switch (key) {
        case 'q':
        case 'Q':
            game_mode = MODE_EXIT;
            break;

        case 'w': 
        case 'W': 
            move = MOVE_UP;
            break;
        case 'a': 
        case 'A': 
            move = MOVE_LEFT;
            break;
        case 'd': 
        case 'D': 
            move = MOVE_RIGHT;
            break;
        case 's': 
        case 'S': 
            move = MOVE_DOWN;
            break;

        case 'p': 
        case 'P': 
            game_pause = ! game_pause;
            break;

        case 'r': 
        case 'R': 
            new_game();
            break;
    }
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
    short heady = snake[((snakelen - 1) * 2) + 0];
    short headx = snake[((snakelen - 1) * 2) + 1];
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
                short taily = snake[0];
                short tailx = snake[1];
                short i;

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
                short bodyy = snake[((snakelen - 2) * 2) + 0];
                short bodyx = snake[((snakelen - 2) * 2) + 1];

                grid_set(bodyy, bodyx, OBJ_SNAKE);
            }

            break;
    }
}

static void update_game(void)
{
    if (game_mode != MODE_GAME) return;

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

static void draw_block(short y, short x, unsigned char color)
{
    int n = (y * GRID_WIDTH) + x;
    udp_data[2 + (n*3)] = rgb_color[(color * 3) + 0];
    udp_data[3 + (n*3)] = rgb_color[(color * 3) + 1];
    udp_data[4 + (n*3)] = rgb_color[(color * 3) + 2];
}

static void draw_obj(short y, short x)
{
    unsigned char type, color;

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
            draw_block(y, x, color);
            break;

        /* box with 1px border */
        case OBJ_WALL:
        case OBJ_SNAKE:
            draw_block(y, x, color);
            break;

        case OBJ_SNAKEHEAD:
            draw_block(y, x, color);
            break;

        /* circle */
        case OBJ_FOOD:
        case OBJ_SUPERFOOD:
        case OBJ_POISON:
            draw_block(y, x, color);
            break;
    }

}

static void draw_game(void)
{
    short y, x;
    short i;

    if (alldirty) {
        for (y = 0; y < GRID_HEIGHT; y++) {
            for (x = 0; x < GRID_WIDTH; x++) {
                draw_obj(y, x);
            }
        }
    } else {
        for (i = 0; i < dirtylen; i++) {
            y = dirtycells[(i * 2) + 0];
            x = dirtycells[(i * 2) + 1];

            draw_obj(y, x);
        }
    }

    alldirty = 0;
    dirtylen = 0;

    sendto(udp_fd, udp_data, sizeof(udp_data), 0, (struct sockaddr *)&udp_sockaddr, sizeof(udp_sockaddr));
}

static int get_ticks(void)
{
    struct timeval tv = { 0 };
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

static void usage(void)
{
    fprintf(stderr, "Usage: snake IP PORT\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        usage();
    }
    memset(&udp_sockaddr, '\0', sizeof(udp_sockaddr));
    udp_sockaddr.sin_family = AF_INET;
    udp_sockaddr.sin_port = htons(atoi(argv[2]));
    udp_sockaddr.sin_addr.s_addr = inet_addr(argv[1]);
    init();
    new_game();
    while (game_mode != MODE_EXIT) {
        handle_input();
        if (game_mode == MODE_EXIT) break;
        thistick = get_ticks() / 500;
        if (thistick != lasttick) {
            lasttick = thistick;
            if (! game_pause) {
                update_game();
            }
        }
        draw_game();
        usleep(100000);
    }
    tcsetattr(0, TCSANOW, &told);
}
