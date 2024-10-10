#ifndef SENTRY_H_TETRIS
#define SENTRY_H_TETRIS

#if FOR_WINDOWS

void init_game_win();
void start_game_win();
void restore_game_win();

#else

void init_game();
void start_game();
void restore_game();

#endif
#endif
