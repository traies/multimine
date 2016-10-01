#ifndef MSG_STRUCTS_H
#define MSG_STRUCTS_H

#include <stdint.h>

enum MessageType{
     INITGAME,
     QUERYMINE,
     UPDATEGAME
};

typedef struct GenericStruct
{
     enum MessageType type;
     char data[];
} GenericStruct;

typedef struct InitStruct
{
     int64_t rows;
     int64_t cols;
     int64_t mines;
     int64_t player_id;
     int64_t players;
} InitStruct;

typedef struct EndGameStruct
{
     int64_t players;
     int64_t winner_id;
     int64_t player_scores[7];
} EndGameStruct;

typedef struct QueryStruct
{
     int64_t x;
     int64_t y;
} QueryStruct;

typedef struct UpdateStruct
{
     int64_t players;
     int64_t player_scores[8][2];
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
