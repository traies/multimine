#include "comms.h"
#include <sys/socket.h>
#include <sys/types.h>
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

struct address {
  char * path;
} ;

struct listener {
     int l_fd;
};

/*
** This function creates a socket and binds
** it to the address providen. The file descriptor
** generated is returned in the Listener struct.
*/
Listener_p mm_listen(Address * addr){
  struct sockaddr_un sa ;
  memset(&sa,0,sizeof(sa));
  int sfd = socket(PF_LOCAL, SOCK_STREAM, IPPROTO_TCP);
  if(sfd==-1){
    return NULL;
  }
  sa.sun_family=AF_LOCAL;
  memcpy(sa.sun_path,addr->path,strlen(addr->path)+1);
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
Connection * mm_connect(Listener_p l,Address * addr){
  if(l==NULL || addr==NULL){
    return NULL;
  }
  struct sockaddr_un * sa=calloc(sizeof(*sa),1);
  sa->sun_family=AF_LOCAL;
  memcpy(sa->sun_path,addr->path,strlen(addr->path)+1);

  if(connect(l->l_fd,(struct sockaddr *)sa,sizeof(*sa))==-1){
    return NULL;
  }
  return newConnection(l->l_fd,sa);
}

void mm_disconnect(Connection * c){

}

/*
** Accepts incoming communications on the listening socket.
** This should block the caller until other process attempts
** to conect to this process' address. A Connection struct
** is returned upon connection.
*/
Connection * mm_accept(Listener_p l){
  struct sockaddr_un * sa_cli=calloc(sizeof(*sa_cli),1);
  int size_cli = sizeof(*sa_cli);
  int n_fd=accept(l->l_fd,(struct sockaddr *)sa_cli,&size_cli);
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
char * mm_read(Connection * c, int * s){
  void * b = calloc(sizeof(Message),1);
  read(c->fd,b,sizeof(Message));
  Message * msg = (Message *)b;
  char * buf = malloc(msg->size);
  memcpy(buf,msg->data,msg->size);
  *s=msg->size;
  free(b);
  return buf;
}

int mm_write(Connection * c, const char * m,int size){
  write_msg(c->fd,m,size);
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
