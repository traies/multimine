#ifndef COMMS_H
#define COMMS_H
#define COMM_FIFO 6
#define COMM_SOCK_UNIX 66
#define COMM_SOCK_INET 666

#if __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 600
#else
#define _XOPEN_SOURCE 500
#endif /* __STDC_VERSION__ */


#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>

typedef struct connection Connection ;
typedef struct listener Listener;
typedef Listener * Listener_p;

/*
** Subscribes the caller process to the channel,
** returning the read end of a named pipe/socket
** for use in the accept() function.
*/
Listener_p mm_listen(char * addr);
/*
** Connects the caller process to the given address.
** The process establishing the connection sends the
** accepting process the new channels through which
** they should communicate in the future.
*/
Connection * mm_connect(char * addr);

/*
** Disconnects the caller process from a connection.
*/
void mm_disconnect(Connection * c);

void mm_disconnect_listener(Listener * lp);

int mm_select(Connection * c, struct timeval * timeout);
/*
** Waits for connections at the given listening port.
*/
Connection * mm_accept(Listener_p l);


/*
** Reads bytes from a connection. An int, where the
** amount of bites read will be completed, must be passed.
*/
int64_t mm_read(Connection * c, int8_t buf[], int64_t size);

/*
** Writes size bytes of m to the given connection.
*/
int64_t mm_write(Connection * c, const int8_t * m,int64_t size);

/*
** Returns the version of this header's implementation
** currently in use.
*/
int mm_commtype();

#endif
