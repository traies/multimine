#ifndef SERVER_MARSH_H
#define SERVER_MARSH_H
#include <stdint.h>
#include <comms.h>
#include <msg_structs.h>
#include <time.h>

int64_t send_init(Connection * c, InitStruct * is);
int64_t send_update(Connection * c,  UpdateStruct  * us);
int64_t send_endgame(Connection * c, EndGameStruct * es);
int64_t send_highscore(Connection *c,Highscore * h);
int64_t query_unmarsh(UpdateStruct ** us, char buf[]);
int64_t highscore_unmarsh(UpdateStruct ** us, char buf[]);
int8_t receive(Connection * c, int8_t * buf, int8_t * recv_buf, int64_t size, struct timeval * timeout);
#endif
