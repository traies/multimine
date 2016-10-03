#ifndef CLIENT_MARSH_H
#define CLIENT_MARSH_H

#include <msg_structs.h>
#include <stdint.h>

int64_t send_query(Connection * c, QueryStruct * us);
int64_t send_h_query(Connection * c);
int64_t send_highscore(Connection * c,Highscore * h);
int8_t receive_init(Connection * c,InitStruct ** buf, struct timeval * timeout, int64_t tries);
int8_t receive_update(Connection * c, char * data_struct, int64_t data_size, struct timeval * timeout);
#endif
