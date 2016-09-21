#ifndef COMMS_H
#define COMMS_H

/*
** Declarations
*/
/*
** This struct contains the address of the
** fifo where the process should be contacted,
*/
typedef struct address {
  const char * fifo;
} Address;
typedef struct connection Connection ;
typedef struct Listener * Listener_p;

/*
** Subscribes the caller process to the channel,
** returning the callee's address. If the passed
** addr is not null, this process' read only FIFO will be
** generated at the aforementioned address. Otherwise,
** the address will be "/tmp/pid" where pid=getpid().
*/
Listener_p mm_listen(Address * addr);
/*
** Connects the caller process to the given address.
** The callee must be subscribed to the channel.
*/
Connection * mm_connect(Address * addr);

/*
** Disconnects the caller process to the given address.
**
*/
void mm_disconnect(Connection * conn);

/*
 *
*/
Connection * mm_accept(Listener_p l);

char * mm_read(Connection * c, int * size);

/*
** Writes size bytes of m to the given connection.
*/
int mm_write(Connection * c, const char * m,int size);

#endif
