#ifndef CLIENT_MARSH_H
#define CLIENT_MARSH_H

#include <msg_structs.h>
#include <stdint.h>

int64_t send_query(Connection * c, QueryStruct * us);
int8_t receive_init(Connection * c,InitStruct ** buf, struct timeval * timeout, int64_t tries);
int8_t receive_update(Connection * c, void ** buf, struct timeval * timeout);

#endif
