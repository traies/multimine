#include "comms.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>


struct message{
  int size;
} ;

typedef struct message Message;

struct connection{
  int fd;
} ;

struct listener {
     int l_fd;
};

static Listener_p newListener(int fd);
static Connection * newConnection(int fd);
static int write_msg(int w_fd,const char * m,int size);
/*
** This function creates a socket and binds
** it to the address providen. The file descriptor
** generated is returned in the Listener struct.
*/
Listener_p mm_listen(char * addr){
  struct sockaddr_un sa ;
  memset(&sa,0,sizeof(sa));
  int sfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if(sfd==-1){
    return NULL;
  }
  sa.sun_family=AF_UNIX;
  strncpy(sa.sun_path, addr, strlen(addr)+1);
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
Connection * mm_connect(char * addr){
  if(addr==NULL){
    return NULL;
  }
  int sfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if(sfd==-1){
    return NULL;
  }
  struct sockaddr_un sa;
  memset(&sa,0,sizeof(sa));
  sa.sun_family=AF_UNIX;
  strncpy(sa.sun_path, addr, strlen(addr)+1);

  if(connect(sfd,(struct sockaddr *)&sa,sizeof(sa))==-1){
    return NULL;
  }
  return newConnection(sfd);
}

void mm_disconnect(Connection * c){
  shutdown(c->fd,SHUT_RDWR);
  free(c);
}

void mm_disconnect_listener(Listener * l) {
     shutdown(l->l_fd, SHUT_RDWR);
     free(l);
}

/*
** Accepts incoming communications on the listening socket.
** This should block the caller until other process attempts
** to conect to this process' address. A Connection struct
** is returned upon connection.
*/
Connection * mm_accept(Listener_p l){
  listen(l->l_fd,1);
  struct sockaddr_un sa_cli;
  memset(&sa_cli,0,sizeof(sa_cli));
  int size_cli = sizeof(sa_cli);
  int n_fd=accept(l->l_fd,(struct sockaddr *)&sa_cli,(socklen_t *)&size_cli);
  if(n_fd<0){
    return NULL;
  }
  return newConnection(n_fd);
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
    char * tmp = malloc(m.size);
    read(c->fd,tmp,m.size);
    len = min(size, m.size);
    memcpy(buf,tmp,len);
    free(tmp);
    return len;
}

int mm_write(Connection * c, const char * m,int size){
  write_msg(c->fd,m,size);
  return size;
}

int mm_select(Connection * c, struct timeval * timeout){
  fd_set r_set;
  int ret;
  if (c == NULL){
    return -1;
  }
  FD_SET(c->fd, &r_set);
  ret = select(c->fd + 1, &r_set, NULL, NULL, timeout);
  if (ret == 0) {
       return 0;
  }
  if (ret > 0) {
       return FD_ISSET(c->fd, &r_set);
  }
  return -1;
}

static Connection * newConnection(int fd){
  Connection * c = malloc(sizeof(Connection));
  c->fd=fd;
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
  write(w_fd,msg,sizeof(Message));
  if(size>0){
    write(w_fd,m,size);
  }
  free(msg);
  return size;
}

int mm_commtype(){
  return COMM_SOCK_UNIX;
}
