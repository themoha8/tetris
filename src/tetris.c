#if FOR_WINDOWS

#include <Windows.h>
#include <stdio.h>
#include <time.h> /* time */
#include <conio.h>

#else

#include <stdio.h>
#include <unistd.h> /* isatty, usleep */
#include <sys/ioctl.h> /* ioctl */
#include <stdlib.h> /* exit */
#include <time.h> /* time */
#include <termios.h> /* tcgetattr, tcsetattr */
#include <fcntl.h> /* fcntl */

#endif

enum {
	default_attr = 0,
	blinking = 5,
	font_white = 37,
	font_cyan = 34,
	font_red = 31,
	font_purple = 35,
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

enum { key_esc = 27, key_up = 0x415b1b, key_down = 0x425b1b, key_space = 32,
	   key_right = 0x435b1b, key_left = 0x445b1b };

enum { field_width = 26, field_height = 20, frame_width = 9, frame_height = 5 };

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
	int font_color;
};

struct current_tetromino_t {
	int which;
	int x, y;
	int back_color;
};

struct score_t {
	int sc;
	int multip;
	int x, y;
	int font_color;
};

struct frame_t {
	int x, y;
	int font_color;
};

#if FOR_WINDOWS

/* _getch */
enum { c_up = 72, c_left = 75, c_right = 77, c_down = 80 };

struct win_t {
	int width, height;
	HANDLE out;
	DWORD cls_mode_out;
};

static struct win_t win;

#endif

static struct map_t map;
static struct current_tetromino_t curr_tetromino;
static struct current_tetromino_t next_tetromino;
static char grid[field_height][field_width] = { 0 };
static struct score_t score = { 0 };
static struct frame_t frame = { 0 };

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

inline static void hide_cursor()
{
	printf("\x1b[?25l");
}

inline static void set_cursor(int x, int y)
{
	printf("\x1b[%d;%dH", y, x);
}

inline static void show_cursor()
{
	printf("\x1b[?25h");
}

inline static void clear_screen()
{
	printf("\x1b[2J");
}

inline static void set_color(int color)
{
	printf("\x1b[%dm", color);
}

static void clean_tetromino_frame()
{
	int i, w = next_tetromino.which;
	int offset_x = 2, offset_y = 1;

	for (i = 0; i <4; i++) {
		if (tetromines[w].blocks[i].x > 2)
			offset_x = 0;
		if (tetromines[w].blocks[i].y > 2)
			offset_y = 0;
	}

	for (i = 0; i < 4; i++) {
		set_cursor(tetromines[w].blocks[i].x + frame.x+1+offset_x,
				   tetromines[w].blocks[i].y + frame.y+1+offset_y);
		printf("  ");
	}
}

static void print_tetromino_frame()
{
	int i, w = next_tetromino.which;
	int offset_x = 2, offset_y = 1;

	for (i = 0; i <4; i++) {
		if (tetromines[w].blocks[i].x > 2)
			offset_x = 0;
		if (tetromines[w].blocks[i].y > 2)
			offset_y = 0;
	}

	set_color(next_tetromino.back_color);
	for (i = 0; i < 4; i++) {
		set_cursor(tetromines[w].blocks[i].x + frame.x+1+offset_x,
				   tetromines[w].blocks[i].y + frame.y+1+offset_y);
		printf("  ");
	}
	set_color(default_back);
}

static void print_frame()
{
	int x, y, max_x, max_y;

	max_x = frame.x + frame_width;
	max_y = frame.y + frame_height;

	set_color(frame.font_color);
	/* 4 is "Next" */
	set_cursor((frame.x + ((frame_width-4)/2+1)), frame.y-1);
	printf("Next");

	for (y = frame.y; y <= max_y; y++) {
		for (x = frame.x; x <= max_x; x++) {
			set_cursor(x, y);

#if FOR_WINDOWS

			if (x == frame.x && y == frame.y)
				printf("*");
			else if (x == max_x && y == frame.y)
				printf("*");
			else if (x == frame.x && y == max_y)
				printf("*");
			else if (x == max_x && y == max_y)
				printf("*");
			else if (x == frame.x || x == max_x) {
				printf("|");
			}
			else if (y == frame.y || y == max_y) {
				printf("-");
			}
			else
				putchar(' ');

#else

			if (x == frame.x && y == frame.y)
				printf("┌");
			else if (x == max_x && y == frame.y)
				printf("┐");
			else if (x == frame.x && y == max_y)
				printf("└");
			else if (x == max_x && y == max_y)
				printf("┘");
			else if (x == frame.x || x == max_x) {
				printf("│");
			}
			else if (y == frame.y || y == max_y) {
				printf("─");
			}
			else
				putchar(' ');

#endif
		}
	}
	set_color(default_font);
}

static void print_score()
{
	score.sc = score.sc + (score.multip * 100);
	score.multip = 0;

	/* clean score */
	set_cursor(score.x, score.y);
	printf("               ");

	set_color(score.font_color);
	set_cursor(score.x, score.y);
	printf("Score: %d", score.sc);
	set_color(default_font);
}

static void print_help()
{
	set_cursor(map.x-25, map.y);
	printf("L-arrow or a: left");
	set_cursor(map.x-25, map.y+1);
	printf("R-arrow or d: right");
	set_cursor(map.x-25, map.y+2);
	printf("D-arrow or s: down");
	set_cursor(map.x-25, map.y+3);
	printf("U-arrow or r: rotate");
	set_cursor(map.x-25, map.y+4);
	printf("Enter or q: quit");
	set_cursor(map.x-25, map.y+5);
	printf("Space: pause");
}

static void print_map()
{
	int x, y;

	clear_screen();
	set_color(map.font_color);
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

	set_color(curr_tetromino.back_color);
	for (i = 0; i < 4; i++) {
		set_cursor(tetromines[w].blocks[i].x + curr_tetromino.x,
				   tetromines[w].blocks[i].y + curr_tetromino.y);
		printf("  ");
	}
	set_color(default_back);
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

static void new_tetromino(struct current_tetromino_t *curr_tetro)
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

	curr_tetro->which = which;
	curr_tetro->x = map.x + seed;
	curr_tetro->y = map.y;
	curr_tetro->back_color = back_red + (which / 4);
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
			score.multip++;
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
			grid[field_y][field_x] = (char)curr_tetromino.back_color;
	}
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
}

#if FOR_WINDOWS

static void set_terminal_win()
{
	DWORD out_cls_mode;

	/* setup console (enable sequence control) */
	if (!GetConsoleMode(win.out, &out_cls_mode)) {
		fprintf(stderr, "GetConsoleModeOut");
		exit(GetLastError());
	}

	win.cls_mode_out = out_cls_mode;

	out_cls_mode = out_cls_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;

	if (!SetConsoleMode(win.out, out_cls_mode)) {
		fprintf(stderr, "SetConsoleModeOut");
		exit(GetLastError());
	}

	/* set console title */
	printf("\x1b]0;Snake\x1b\x5c");
}

void restore_game_win()
{
	set_cursor(0, 0);
    show_cursor();
    clear_screen();

	SetConsoleMode(win.out, win.cls_mode_out);
}

void init_game_win()
{
	CONSOLE_SCREEN_BUFFER_INFO win_info;
	int offset_x, offset_y;

	win.out = GetStdHandle(STD_OUTPUT_HANDLE);
	if (win.out == INVALID_HANDLE_VALUE) {
		printf("GetStdHandleOut\n");
		exit(GetLastError());
	}

	GetConsoleScreenBufferInfo(win.out, &win_info);
	win.width = win_info.srWindow.Right - win_info.srWindow.Left + 1;
	win.height = win_info.srWindow.Bottom - win_info.srWindow.Top + 1;

	if (win.width < 79 || win.height < 23) {
		fprintf(stderr, "init_game: increase the window size. "
			"Minimum screen size 80x24\n");
		Sleep(3000);
		exit(1);
	}

	set_terminal_win();

    /* offset_x + field_width+2 + offset_x */
	offset_x = (win.width - (field_width+2))/2;
	map.x = offset_x + 1;
	map.max_x = offset_x + field_width+2;

	/* offset_y + field_height + offset_y */
	offset_y = (win.height - field_height)/2;
	map.y = offset_y + 1;
	map.max_y = offset_y + field_height;

	map.font_color = font_white;

	print_map();

	srand((int)time(NULL));
	new_tetromino(&curr_tetromino);
	new_tetromino(&next_tetromino);
	print_tetromino();

	frame.x = map.max_x + 2;
	frame.y = map.y+1; /* +1 for "Next" text */
	frame.font_color = font_purple;
	print_frame();
	print_tetromino_frame();

	score.x = frame.x;
	score.y = frame.y + frame_height + 2;
	score.font_color = font_red;
	print_score();

	print_help();
	hide_cursor();
	fflush(stdout);
}

static void end_game_win()
{
	int i, x, y;

	x = ((win.width - w_game_over) / 2) + 1;
	y = ((win.height - h_game_over) / 2) + 1;

	clear_screen();

	for (i = 0; i < h_game_over; i++) {
		set_cursor(x, y+i);
		printf("%s", game_over_logo[i]);
	}
	fflush(stdout);
	Sleep(2000);
}

static void pause_game_win()
{
	int pause_game = 1;
	int c;

	set_cursor(map.x-5, map.y-2);
	printf("game is paused, press space to continue");
	fflush(stdout);

	while (pause_game) {
		if (_kbhit() != 0) {
			c = _getch();
			switch(c) {
			case key_space:
				pause_game = 0;
				break;
			case 'Q':
			case 'q':
			case key_esc:
				restore_game_win();
				exit(0);
				break;
			}
		}
		Sleep(30);
	}

	set_cursor(map.x-5, map.y-2);
	printf("                                       ");
	fflush(stdout);
}

void start_game_win()
{
	int game_over = 0, key;
	int elapsed_ms = 0;

	while (!game_over) {
		if (_kbhit() != 0) {
			/* getch returns 0 or 224 to indicate that next key is special */
			key = _getch();
			if (key == 0 || key == 224) {
				key = _getch();
				switch(key) {
				case c_up:
					clean_tetromino();
					rotate_tetromino();
					print_tetromino();
					break;
				case c_down:
					if (!is_collision(curr_tetromino.x, curr_tetromino.y+1)) {
						clean_tetromino();
						curr_tetromino.y++;
						print_tetromino();
					}
					break;
				case c_right:
					if (!is_collision(curr_tetromino.x+2, curr_tetromino.y)) {
						clean_tetromino();
						curr_tetromino.x += 2;
						print_tetromino();
					}
					break;
				case c_left:
					if (!is_collision(curr_tetromino.x-2, curr_tetromino.y)) {
						clean_tetromino();
						curr_tetromino.x -= 2;
						print_tetromino();
					}
					break;
				}
			} else {
				switch(key) {
				case 's':
				case 'S':
					if (!is_collision(curr_tetromino.x, curr_tetromino.y+1)) {
						clean_tetromino();
						curr_tetromino.y++;
						print_tetromino();
					}
					break;
				case 'd':
				case 'D':
					if (!is_collision(curr_tetromino.x+2, curr_tetromino.y)) {
						clean_tetromino();
						curr_tetromino.x += 2;
						print_tetromino();
					}
					break;
				case 'a':
				case 'A':
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
					pause_game_win();
					break;
				}
			}

		}

        if (elapsed_ms > fall_delay) {
            if (!is_collision(curr_tetromino.x, curr_tetromino.y+1)) {
				clean_tetromino();
				curr_tetromino.y++;
				print_tetromino();
			} else {
				lock_tetromino();
				remove_full_lines();
				print_score();
				curr_tetromino = next_tetromino;
				clean_tetromino_frame();
				new_tetromino(&next_tetromino);
				print_tetromino_frame();
				if (is_collision(curr_tetromino.x, curr_tetromino.y)) {
					end_game_win();
					game_over = 1;
				} else
					print_tetromino();
			}
			elapsed_ms = 0;
		}

		fflush(stdout);
		/* 30 ms (30000 microsec = 30 000 * 10^-3 ms = 30 ms) */
		Sleep(30);
		elapsed_ms += 30;
	}
}

#else

static void set_terminal()
{
	struct termios ts;

    tcgetattr(0, &ts);
	ts.c_lflag &= ~(ICANON|ECHO);
    ts.c_cc[VMIN] = 0;
    tcsetattr(0, TCSANOW, &ts);
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

	if (w.ws_col < 79 || w.ws_row < 23) {
		fprintf(stderr, "init_game: increase the window size. " 
			"Minimum screen size 80x24\n");
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

	map.font_color = font_white;

	print_map();

	srand(time(NULL));
	new_tetromino(&curr_tetromino);
	new_tetromino(&next_tetromino);
	print_tetromino();

	frame.x = map.max_x + 2;
	frame.y = map.y+1; /* +1 for "Next" text */
	frame.font_color = font_purple;
	print_frame();
	print_tetromino_frame();

	score.x = frame.x;
	score.y = frame.y + frame_height + 2;
	score.font_color = font_red;
	print_score();

	print_help();
	hide_cursor();
	fflush(stdout);
}

void restore_game()
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

static time_t diff_timestamps(const struct timespec *start, const struct timespec *end)
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
			case key_esc:
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

	for (i = 0; i < h_game_over; i++) {
		set_cursor(x, y+i);
		printf("%s", game_over_logo[i]);
	}
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
	int n, game_over = 0, key = 0;
	struct timespec t1, t2;
	time_t elapsed_ms;

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
			case key_up:
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
			 	print_score();
			 	curr_tetromino = next_tetromino;
			 	clean_tetromino_frame();
			 	new_tetromino(&next_tetromino);
			 	print_tetromino_frame();
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

#endif
