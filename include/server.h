#ifndef SERVER_H
#define SERVER_H
#include <stdint.h>

typedef struct Minefield * Minefield_p;

Minefield_p create_minefield(int64_t cols, int64_t rows, int64_t mines);
void free_minefield(Minefield_p m);
int64_t uncover_sector(Minefield_p m, int64_t x, int64_t y, int64_t player, int8_t (*retbuf)[4]);
int64_t get_mine_buffer(Minefield_p m, int64_t ** mb);
#endif
