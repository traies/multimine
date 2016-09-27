#include "comms.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>

#define DATASIZE 1024

struct message{
  int size;
  char data[DATASIZE];
} ;

typedef struct message Message;

struct connection{
  int fd;
  struct sockaddr_un * o_a;
} ;

struct listener {
     int l_fd;
};

static Listener_p newListener(int fd);
static Connection * newConnection(int fd,struct sockaddr_un * o);
static int write_msg(int w_fd,const char * m,int size);
/*
** This function creates a socket and binds
** it to the address providen. The file descriptor
** generated is returned in the Listener struct.
*/
Listener_p mm_listen(Address * addr){
  struct sockaddr_un sa ;
  memset(&sa,0,sizeof(sa));
  int sfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if(sfd==-1){
    return NULL;
  }
  sa.sun_family=AF_UNIX;
  strncpy(sa.sun_path, (char *)addr, strlen((char*)addr)+1);
  if(bind(sfd,(struct sockaddr *)&sa,sizeof(sa))==-1){
    close(sfd);
    return NULL;
  }
  return newListener(sfd);
}

/*
** Connects the own listening socket to the address
** providen.
*/
Connection * mm_connect(Address * addr){
  if(addr==NULL){
    return NULL;
  }
  int sfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if(sfd==-1){
    return NULL;
  }
  struct sockaddr_un * sa=calloc(sizeof(*sa),1);
  sa->sun_family=AF_UNIX;
  strncpy(sa->sun_path, (char *)addr, strlen((char *)addr)+1);

  if(connect(sfd,(struct sockaddr *)sa,sizeof(*sa))==-1){
    return NULL;
  }
  return newConnection(sfd,sa);
}

void mm_disconnect(Connection * c){
  close(c->fd);
  free(c->o_a);
  free(c);
}

/*
** Accepts incoming communications on the listening socket.
** This should block the caller until other process attempts
** to conect to this process' address. A Connection struct
** is returned upon connection.
*/
Connection * mm_accept(Listener_p l){
  listen(l->l_fd,1);
  struct sockaddr_un * sa_cli=calloc(sizeof(*sa_cli),1);
  int size_cli = sizeof(*sa_cli);
  int n_fd=accept(l->l_fd,(struct sockaddr *)sa_cli,(socklen_t *)&size_cli);
  if(n_fd<0){
    free(sa_cli);
    return NULL;
  }
  return newConnection(n_fd,sa_cli);
}

/*
** The folowing functions allow read and write operations
** on an established connection.
*/
int mm_read(Connection * c, char buf[], int size)
{
    int len;
    Message m;
    if ((len = read(c->fd,(char *) &m,sizeof(Message))) < 0) {
	     return -1;
    }
    len = min(size, m.size);
    memcpy(buf,m.data,len);
    return len;
}

int mm_write(Connection * c, const char * m,int size){
  write_msg(c->fd,m,size);
  return size;
}

int mm_select(Connection * c, struct timeval * timeout){
  fd_set r_set;
  if (c == NULL){
    return -1;
  }
  FD_SET(c->fd, &r_set);
  return select(c->fd + 1, &r_set, NULL, NULL, timeout);
}

static Connection * newConnection(int fd,struct sockaddr_un * o){
  Connection * c = malloc(sizeof(Connection));
  c->fd=fd;
  c->o_a=o;
  return c;
}

static Listener_p newListener(int fd){
  Listener_p l = malloc(sizeof(*l));
  l->l_fd=fd;
  return l;
}

static int write_msg(int w_fd,const char * m,int size){

  Message * msg = calloc(sizeof(Message),1);
  msg->size=size;

  if(size>0){
    memcpy(msg->data,m,size);
  }
  write(w_fd,msg,sizeof(Message));
  free(msg);
  return size;
}
