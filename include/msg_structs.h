#ifndef MSG_STRUCTS_H
#define MSG_STRUCTS_H

#include <stdint.h>

extern const int8_t INITGAME;
extern const int8_t QUERYMINE;
extern const int8_t UPDATEGAME;
extern const int8_t ENDGAME;
extern const int8_t DISCONNECT;
extern const int8_t NOREAD;
extern const int8_t ERROR;
extern const int8_t HIGHSCORES;
extern const int8_t HIGHSCORE_ADD;



typedef struct Highscore
{
  char name[20];
  int64_t score;
}Highscore;


typedef struct InitStruct
{
     int64_t rows;
     int64_t cols;
     int64_t mines;
     int8_t player_id;
     int8_t players;
} InitStruct;

typedef struct EndGameStruct
{
     int8_t players;
     int8_t winner_id;
     int64_t player_scores[8];
} EndGameStruct;

typedef struct QueryStruct
{
     int8_t x;
     int8_t y;
} QueryStruct;

typedef struct UpdateStruct
{
     int8_t players;
     int64_t player_scores[8];
     int64_t len;
     struct TileUpdate
     {
	  int8_t x;
	  int8_t y;
	  int8_t nearby;
	  int8_t player;
     } tiles [];
} UpdateStruct;

#endif
