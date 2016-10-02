#ifndef GAME_H
#define GAME_H
#include <stdint.h>
#include <msg_structs.h>

typedef struct Minefield Minefield;
Minefield * create_minefield(int64_t cols, int64_t rows, int64_t mines, int64_t players);
int64_t update_minefield(Minefield * m, int64_t x, int64_t y, int64_t player, UpdateStruct * us);
void free_minefield(Minefield * m);
int8_t get_scores(Minefield * m, int64_t player_scores[8]);
int8_t check_win_state(Minefield * m, EndGameStruct * es);
int8_t reset_score(Minefield * m, int64_t playerid);
#endif
