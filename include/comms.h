#ifndef COMMS_H
#define COMMS_H
#define min(a,b)  ((a) < (b))?(a):(b)

#if __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 600
#else
#define _XOPEN_SOURCE 500
#endif /* __STDC_VERSION__ */


#include <stdint.h>
#include <sys/time.h>

typedef struct InitStruct
{
     int64_t rows;
     int64_t cols;
     int64_t mines;
     int64_t player_id;
     int64_t player_count;
} InitStruct;

typedef struct QueryStruct
{
     int x;
     int y;
} QueryStruct;

typedef struct UpdateStruct
{
     int64_t len;
     struct TileUpdate
     {
	  int8_t x;
	  int8_t y;
	  int8_t nearby;
	  int8_t player;
     } tiles [];
} UpdateStruct;

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

int mm_select(Connection * c, struct timeval * timeout);
/*
** Waits for connections at the given listening port.
*/
Connection * mm_accept(Listener_p l);


/*
** Reads bytes from a connection. An int, where the
** amount of bites read will be completed, must be passed.
*/
int mm_read(Connection * c, char buf[], int size);

/*
** Writes size bytes of m to the given connection.
*/
int mm_write(Connection * c, const char * m,int size);

#endif
