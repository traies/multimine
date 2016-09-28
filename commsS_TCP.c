#include "comms.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PORT 1100

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

Listener_p mm_listen(char * addr){
  struct sockaddr_in sa ;
  memset(&sa,0,sizeof(sa));
  int sfd = socket(AF_INET, SOCK_STREAM, 0);
  if(sfd==-1){
    return NULL;
  }

  int enable = 1;
  setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));

  sa.sin_family=AF_INET;
  sa.sin_port=htons(PORT);
  sa.sin_addr.s_addr= htonl(INADDR_ANY);

  /*
  ** print Address
  */
  printf("%d.%d.%d.%d\n",
  (int)(sa.sin_addr.s_addr&0xFF),
  (int)((sa.sin_addr.s_addr&0xFF00)>>8),
  (int)((sa.sin_addr.s_addr&0xFF0000)>>16),
  (int)((sa.sin_addr.s_addr&0xFF000000)>>24));

  if(bind(sfd,(struct sockaddr *)&sa,sizeof(sa))==-1){
    close(sfd);
    return NULL;
  }
  return newListener(sfd);
}



Connection * mm_connect(char * addr){
  struct sockaddr_in sa;
  int val,sfd;

  sfd=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
  if(sfd==-1){
    return NULL;
  }

  memset(&sa,0,sizeof(sa));
  sa.sin_family=AF_INET;
  sa.sin_port=htons(PORT);
  val = inet_pton(AF_INET,addr,&sa.sin_addr);
  if(connect(sfd,(struct sockaddr *)&sa,sizeof(sa))==-1){
    close(sfd);
    return NULL;
  }
  return newConnection(sfd);

}


void mm_disconnect(Connection * c){
  close(c->fd);
  free(c);
}


int mm_select(Connection * c, struct timeval * timeout){
  fd_set r_set;
  if (c == NULL){
    return -1;
  }
  FD_SET(c->fd, &r_set);
  return select(c->fd + 1, &r_set, NULL, NULL, timeout);
}


Connection * mm_accept(Listener_p l){
  int n_fd;
  n_fd=accept(l->l_fd,NULL,NULL);
  if(n_fd<0){
    return NULL;
  }
  return newConnection(n_fd);
}


int mm_read(Connection * c, char buf[], int size){
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
