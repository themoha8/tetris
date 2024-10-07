#include <stdio.h>
#include <string.h>
#include <unistd.h> /* isatty, usleep */
#include <sys/ioctl.h> /* ioctl */
#include <stdlib.h> /* exit */
#include <time.h> /* time */
#include <termios.h> /* tcgetattr, tcsetattr */
#include <fcntl.h> /* fcntl */

enum {
	default_attr = 0,
	blinking = 5,
	font_white = 37,
	default_font = 39,
	back_red = 41,
	back_green = 42,
	back_yellow = 43,
	back_blue = 44,
	back_magenta = 45,
	back_cyan = 46,
	back_light_gray = 47,
	default_back = 49
};

enum { key_esc = 27, key_down = 0x425b1b, key_space = 32,
	   key_right = 0x435b1b, key_left = 0x445b1b };

enum { field_width = 26, field_height = 20 };

enum { sec_as_millisec = 1000, sec_as_nanosec = 1000000000,
	   sec_as_microsec = 1000000, fall_delay = 500 };   

enum { w_game_over = 58, h_game_over = 5 };

struct block_t {
	int x, y;
};

struct tetromino_t {
	struct block_t blocks[4];
};

struct map_t {
	int x, y;
	int max_x, max_y;
	int color;
};

struct current_tetromino_t {
	int which;
	int x, y;
	int color;
};

static struct map_t map;
static struct current_tetromino_t curr_tetromino;
static char grid[field_height][field_width] = { 0 };

static const struct tetromino_t tetromines[28] = {
	/* O-tetromino (square) */
	{{ {0, 0}, {0, 1}, {2, 0}, {2, 1} }},
	{{ {0, 0}, {0, 1}, {2, 0}, {2, 1} }},
	{{ {0, 0}, {0, 1}, {2, 0}, {2, 1} }},
	{{ {0, 0}, {0, 1}, {2, 0}, {2, 1} }},

	/* I-tetromino */
	{{ {2, 0}, {2, 1}, {2, 2}, {2, 3} }},
	{{ {0, 0}, {2, 0}, {4, 0}, {6, 0} }},
	{{ {4, 0}, {4, 1}, {4, 2}, {4, 3} }},
	{{ {0, 0}, {2, 0}, {4, 0}, {6, 0} }},

	/* S-tetromino */
	{{ {2, 0}, {4, 0}, {0, 1}, {2, 1} }},
	{{ {0, 0}, {0, 1}, {2, 1}, {2, 2} }},
	{{ {2, 0}, {4, 0}, {0, 1}, {2, 1} }},
	{{ {0, 0}, {0, 1}, {2, 1}, {2, 2} }},

	/* Z-tetromino */
	{{ {0, 0}, {2, 0}, {2, 1}, {4, 1} }},
	{{ {2, 0}, {0, 1}, {2, 1}, {0, 2} }},
	{{ {0, 0}, {2, 0}, {2, 1}, {4, 1} }},
	{{ {2, 0}, {0, 1}, {2, 1}, {0, 2} }},

	/* T-tetromino */
	{{ {0, 0}, {2, 0}, {4, 0}, {2, 1} }},
	{{ {2, 0}, {0, 1}, {2, 1}, {2, 2} }},
	{{ {2, 0}, {0, 1}, {2, 1}, {4, 1} }},
	{{ {0, 0}, {0, 1}, {2, 1}, {0, 2} }},

	/* J-tetromino */
	{{ {2, 0}, {2, 1}, {0, 2}, {2, 2} }},
	{{ {0, 0}, {0, 1}, {2, 1}, {4, 1} }},
	{{ {0, 0}, {2, 0}, {0, 1}, {0, 2} }},
	{{ {0, 0}, {2, 0}, {4, 0}, {4, 1} }},

	/* L-tetromino */
	{{ {0, 0}, {0, 1}, {0, 2}, {2, 2} }},
	{{ {0, 0}, {2, 0}, {4, 0}, {0, 1} }},
	{{ {0, 0}, {2, 0}, {2, 1}, {2, 2} }},
	{{ {4, 0}, {0, 1}, {2, 1}, {4, 1} }}
};

static const char *game_over_logo[5] = {
"   ____                            ___                    ",
"  / ___|  __ _  _ __ ___    ___   / _ \\ __   __ ___  _ __ ",
" | |  _  / _` || '_ ` _ \\  / _ \\ | | | |\\ \\ / // _ \\| '__|",
" | |_| || (_| || | | | | ||  __/ | |_| | \\ V /|  __/| |   ",
"  \\____| \\__,_||_| |_| |_| \\___|  \\___/   \\_/  \\___||_|   " };                                                        

inline void hide_cursor()
{
	printf("\x1b[?25l");
}

inline void set_cursor(int x, int y)
{
	printf("\x1b[%d;%dH", y, x);
}

inline void show_cursor()
{
	printf("\x1b[?25h");
}

inline void clear_screen()
{
	printf("\x1b[2J");
}

inline void set_color(int color)
{
	printf("\x1b[%dm", color);
}

static void print_map()
{
	int x, y;

	clear_screen();
	set_color(map.color);
	for (y = map.y; y <= map.max_y; y++) {
		for (x = map.x; x <= map.max_x; x++) {
			if (x == map.x || x == map.max_x) {
				set_cursor(x, y);
				putchar('|');
			}
			else
				putchar('.');
		}
	}

	set_color(default_font);
}

static void clean_tetromino()
{
	int i, w = curr_tetromino.which;

	for (i = 0; i < 4; i++) {
		set_cursor(tetromines[w].blocks[i].x + curr_tetromino.x,
				   tetromines[w].blocks[i].y + curr_tetromino.y);
		printf("..");
	}
}

static void print_tetromino()
{
	int i, w = curr_tetromino.which;

	set_color(curr_tetromino.color);
	for (i = 0; i < 4; i++) {
		set_cursor(tetromines[w].blocks[i].x + curr_tetromino.x,
				   tetromines[w].blocks[i].y + curr_tetromino.y);
		printf("  ");
	}
	set_color(default_back);
}

static void set_terminal()
{
	struct termios ts;

    tcgetattr(0, &ts);
   	ts.c_lflag &= ~(ICANON|ECHO);
    ts.c_cc[VMIN] = 0;
    tcsetattr(0, TCSANOW, &ts);
}

static int is_collision(int dx, int dy)
{
	int i, w = curr_tetromino.which;

	for (i = 0; i < 4; i++) {
		int x = tetromines[w].blocks[i].x + dx;
		int y = tetromines[w].blocks[i].y + dy;
		
		/* -1 to make the index in the array start from 0 */
		int field_x = x - map.x-1;
		int field_y = y - map.y;

		/* map.max_y+1 because there is no visible border */
		if (map.x >= x || x >= map.max_x || y >= map.max_y+1
			|| grid[field_y][field_x])
			return 1;
	}

	return 0;
}

static void new_tetromino()
{
	int which, seed;

	/* If the remainder is 0, then add one to offset from the map border.
	 * If the remainder is max, then result (if field_width = 26) is 18 (17+1),
	 * but the result must be an odd number for tetramino to align correctly.
	 */
	seed = (rand() % (field_width-8)) + 1;
	which = rand() % 28;
	
	if (seed % 2 == 0)
	 		seed++;

	curr_tetromino.which = which;
	curr_tetromino.x = map.x + seed;
	curr_tetromino.y = map.y;
	curr_tetromino.color = back_red + (which / 4);
}

void clean_full_lines()
{
	int x, y, field_x, field_y;

	for (y = map.y; y <= map.max_y; y++) {
		field_y = y - map.y;
		for (x = map.x+1; x < map.max_x; x += 2) {
			field_x = x - map.x-1;
			if (grid[field_y][field_x]) {
				set_cursor(x, y);
				set_color(grid[field_y][field_x]);
				printf("  ");
				set_color(default_back);
			}
			else {
				set_cursor(x, y);
				printf("..");

			}
		}
	}
}

void remove_full_lines()
{
	int x, y, x2, y2, field_x, field_y;

	/* walk across entire map field and find full lines */
	for (y = map.y; y <= map.max_y; y++) {
		int full = 1;
		field_y = y - map.y;
		for (x = map.x+1; x < map.max_x; x += 2) {
			/* -1 to make the index in the array start from 0 */
			field_x = x - map.x-1;
			if (!grid[field_y][field_x]) {
				full = 0;
				break;
			}
		}

		if (full) {
			/* instead of full line, place all top lines, except map.y */
			for (y2 = y; y2 > map.y; y2--) {
				field_y = y2 - map.y;
				for (x2 = map.x+1; x2 < map.max_x; x2 += 2) {
					field_x = x2 - map.x-1;
					grid[field_y][field_x] = grid[field_y-1][field_x];
				}
			}
			/* clear the topmost line (necessary when tetramino fills
			 * the topmost line and any bottom line is erased
			 */
			for (x2 = map.x+1; x2 < map.max_x; x2 += 2) {
				int field_x = x2 - map.x-1;
				grid[0][field_x] = 0;
			}
		}
	}
	clean_full_lines();
}

static void lock_tetromino()
{
	int i, w = curr_tetromino.which;

	for (i = 0; i < 4; i++) {
		/* -1 to make the index in the array start from 0 */
		int field_x = (tetromines[w].blocks[i].x + curr_tetromino.x) - map.x-1;
		int field_y = (tetromines[w].blocks[i].y + curr_tetromino.y) - map.y;
		
		if (field_y >= 0)
			grid[field_y][field_x] = curr_tetromino.color;
	}
}

void init_game()
{
	struct winsize w;
	int offset_x, offset_y;

	if (!isatty(0)) {
        fprintf(stderr, "init_game: not a terminal\n");
        exit(1);
    }

    ioctl(0, TIOCGWINSZ, &w);

	if (w.ws_col < 64 || w.ws_row < 23) {
		fprintf(stderr, "init_game: increase the window size. " 
			"Minimum screen size 65x24\n");
		exit(1);
	}

    set_terminal();

    /* offset_x + field_width+2 + offset_x */
	offset_x = (w.ws_col - (field_width+2))/2;
	map.x = offset_x + 1;
	map.max_x = offset_x + field_width+2;

	/* offset_y + field_height + offset_y */
	offset_y = (w.ws_row - field_height)/2;
	map.y = offset_y + 1;
	map.max_y = offset_y + field_height;

	map.color = font_white;

	print_map();

	srand(time(NULL));
	new_tetromino();
	print_tetromino();
	hide_cursor();
	fflush(stdout);
}

void restore_game(void)
{
	struct termios ts;

    tcgetattr(0, &ts);
   	ts.c_lflag |= (ICANON|ECHO);
    ts.c_cc[VMIN] = 1;
    ts.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &ts);

    set_cursor(0, 0);
    show_cursor();
    clear_screen();
}

void rotate_tetromino()
{
	int old_w, w;

	old_w = curr_tetromino.which;

	w = (curr_tetromino.which+1) % 4;
	if (w == 0)
		curr_tetromino.which -= 3;
	else
		curr_tetromino.which++;

	if(is_collision(curr_tetromino.x, curr_tetromino.y))
		curr_tetromino.which = old_w;
	
	set_cursor(map.x, map.max_y+2);
	printf("old_w: %d", old_w);
	set_cursor(map.x, map.max_y+3);
	printf("new_w: %d", curr_tetromino.which);
	fflush(stdout);
}

static int diff_timestamps(const struct timespec *start, const struct timespec *end)
{
	struct timespec tmp;

	if (end->tv_nsec - start->tv_nsec < 0) {
		tmp.tv_sec = end->tv_sec - start->tv_sec - 1;
		tmp.tv_nsec = end->tv_nsec - start->tv_nsec + sec_as_nanosec;
	}
	else {
		tmp.tv_sec = end->tv_sec - start->tv_sec;
		tmp.tv_nsec = end->tv_nsec - start->tv_nsec;
	}

	return tmp.tv_sec * sec_as_millisec + tmp.tv_nsec / sec_as_microsec;
}

static void pause_game()
{
	int pause_game = 1;
	char c, n;

	set_cursor(map.x-5, map.y-2);
	printf("game is paused, press space to continue");
	fflush(stdout);

	while (pause_game) {
		n = read(0, &c, 1);

		if (n)
			switch(c) {
			case key_space:
				pause_game = 0;
				break;
			case 'Q':
			case 'q':
				restore_game();
				exit(0);
				break;
			}
		
		usleep(30000);
	}

	set_cursor(map.x-5, map.y-2);
	printf("                                       ");
	fflush(stdout);
}

static void end_game()
{
	struct winsize w;
	int i, x, y;

	ioctl(0, TIOCGWINSZ, &w);

	x = ((w.ws_col - w_game_over) / 2) + 1;
	y = ((w.ws_row - h_game_over) / 2) + 1;

	clear_screen();

	/* set_color(blinking); */
	for (i = 0; i < h_game_over; i++) {
		set_cursor(x, y+i);
		printf("%s", game_over_logo[i]);
	}
	/* set_color(default_attr); */
	fflush(stdout);
	sleep(2);
}

/* 1 step of the cycle takes 30 ms, the reaction to key pressed occurs every 30
 * ms, after 17 steps of the cycle (30 ms * 17 = 510 ms + ~3 ms (cumulative
 * effect)) (the first step of the cycle will require 18 steps of the cycle
 * since the first step there is no 30 ms delay and the difference will be
 * 0 ms) tetromino falls down 1 cell
 */
void start_game()
{
	int n, elapsed_ms, game_over = 0, key = 0;
	struct timespec t1, t2;

	clock_gettime(CLOCK_MONOTONIC, &t1);

	while (!game_over) {
		n = read(0, &key, 3);
		if (n) {
			switch(key) {
			case 's':
			case 'S':
			case key_down:
				if (!is_collision(curr_tetromino.x, curr_tetromino.y+1)) {
					clean_tetromino();
					curr_tetromino.y++;
					print_tetromino();
				}
				break;
			case 'd':
			case 'D':
			case key_right:
				if (!is_collision(curr_tetromino.x+2, curr_tetromino.y)) {
					clean_tetromino();
					curr_tetromino.x += 2;
					print_tetromino();
				}
				break;
			case 'a':
			case 'A':
			case key_left:
				if (!is_collision(curr_tetromino.x-2, curr_tetromino.y)) {
					clean_tetromino();
					curr_tetromino.x -= 2;
					print_tetromino();
				}
				break;
			case key_esc:
			case 'q':
			case 'Q':
				game_over = 1;
				break;
			case 'r':
			case 'R':
				clean_tetromino();
				rotate_tetromino();
				print_tetromino();
				break;
			case key_space:
				pause_game();
				break;
			}
			key = 0;
		}

        clock_gettime(CLOCK_MONOTONIC, &t2);
        elapsed_ms = diff_timestamps(&t1, &t2);

        if (elapsed_ms > fall_delay) {
            if (!is_collision(curr_tetromino.x, curr_tetromino.y+1)) {
				clean_tetromino();
				curr_tetromino.y++;
				print_tetromino();
			} else {
			 	lock_tetromino();
			 	remove_full_lines();
			 	new_tetromino();
			 	if (is_collision(curr_tetromino.x, curr_tetromino.y)) {
			 		end_game();
			 		game_over = 1;
			 	} else
			 		print_tetromino();
			}
			t1 = t2;
		}

		fflush(stdout);
		/* 30 ms (30000 microsec = 30 000 * 10^-3 ms = 30 ms) */
		usleep(30000);
	}
}
