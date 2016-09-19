#ifndef COMMS_H
#define COMMS_H

/*
** Declarations
*/
typedef struct address Address ;
typedef struct connection Connection ;

/*
** Subscribes the caller process to the channel,
** returning the callee's address. If the passed
** addr is not null, this process' read only FIFO will be
** generated at the aforementioned address. Otherwise,
** the address will be "/tmp/pid" where pid=getpid().
*/
Address * subscribe(char *addr);
/*
** Connects the caller process to the given address.
** The callee must be subscribed to the channel.
*/
Connection * connect(Address * addr,char * read_addr,char * write_addr);

/*
** Disconnects the caller process to the given address.
**
*/
void disconnect(Connection * conn);

/*
** Listens to this process' read only fd. If the
** incoming message's source pid is new to this process,
** a new connection must be established. It is the
** programmer's responsibility to know when this will
** occur, and a blank connection must be passed using
** emptyConnection(). When a message is expected, a pointer
** to int should be providen. If the function returns NULL
** when expecting a message and not expecting a connection,
** it means that a disconnection has occurred.
*/
Connection * listen();

char * readm(Connection * c, int * size);

/*
** Writes size bytes of m to the given connection.
*/
int writem(Connection * c, const char * m,int size);

Address * newAddress(const char * f,int p);


#endif
