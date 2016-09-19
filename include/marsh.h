#ifndef MARSH_H
#define MARSH_H
#define OPCODE_SIZE 12
#include <stdint.h>

enum Opcode {
     ACK = 0,
     INIT_PLAYER,
     INIT_GAME
};

typedef struct MsgHandle * MsgH_p;

MsgH_p setup(char * fifo);
int64_t conn(MsgH_p msgh, char * path, int64_t t_sec, int64_t t_nsec, int64_t tries);
int64_t send(MsgH_p msgh, int64_t connid, int64_t opcode, void * data);
int64_t wait_ack(MsgH_p msgh, int64_t mid, int64_t timeout);
int64_t wait_msg(MsgH_p msgh, enum Opcode opcode, void ** data_struct, int64_t t_sec, int64_t t_nsec, int64_t tries );
int64_t check_msg(MsgH_p msgh, void ** data_struct); 



typedef struct PlayerInit_r {
     int64_t playerid;
}PlayerInit_r;
#endif
