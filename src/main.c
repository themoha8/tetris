#include "tetris.h"

int main()
{
#if FOR_WINDOWS

	init_game_win();
	start_game_win();
	restore_game_win();
	return 0;

#else

	init_game();
	start_game();
	restore_game();
	return 0;

#endif
}
