#ifndef COMMS_H
#define COMMS_H

/*
** Declarations
*/

/*
** This struct contains the address of the
** fifo where the process should be contacted
** in order to establish a connection.
*/
typedef struct address {
  const char * fifo;
} Address;
typedef struct connection Connection ;
typedef struct Listener * Listener_p;

/*
** Subscribes the caller process to the channel,
** returning the read end of a named pipe/socket
** for use in the accept() function.
*/
Listener_p mm_listen(Address * addr);
/*
** Connects the caller process to the given address.
** The process establishing the connection sends the
** accepting process the new channels through which
** they should communicate in the future.
*/
Connection * mm_connect(Address * addr);

/*
** Disconnects the caller process from a connection.
*/
void mm_disconnect(Connection * c);

/*
** Waits for connections at the given listening port.
*/
Connection * mm_accept(Listener_p l);


/*
** Reads bytes from a connection. An int, where the
** amount of bites read will be completed, must be passed.
*/
char * mm_read(Connection * c, int * size);

/*
** Writes size bytes of m to the given connection.
*/
int mm_write(Connection * c, const char * m,int size);

#endif
