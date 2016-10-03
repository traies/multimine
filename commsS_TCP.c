#include <comms.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>

struct message{
     int size;
};

typedef struct message Message;

struct connection{
  int fd;
} ;

struct listener {
     int l_fd;
};

static Listener_p newListener(int fd);
static Connection * newConnection(int fd);
static int64_t write_msg(int w_fd,const char * m,int64_t size);

Listener_p mm_listen(char * addr){
  struct addrinfo hints, *res;
  int sfd, true = 1;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  getaddrinfo(NULL, addr, &hints, &res);

  if((sfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol))==-1){
    return NULL;
  }
  if ( setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &true, sizeof(int)) < 0) {
       return NULL;
  }
  if(bind(sfd, res->ai_addr, res->ai_addrlen)==-1){
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
  char tmp[64];
  strcpy(tmp,addr);
  char * dir = strtok(tmp,":");
  char * port = strtok(NULL,":");
  memset(&sa,0,sizeof(sa));
  sa.sin_family=AF_INET;
  sa.sin_port=htons(atoi(port));
  val = inet_pton(AF_INET,dir,&sa.sin_addr);
  if(connect(sfd,(struct sockaddr *)&sa,sizeof(sa))==-1){
    close(sfd);
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


Connection * mm_accept(Listener_p l){
  listen(l->l_fd,1);
  struct sockaddr_in sa_cli;
  memset(&sa_cli,0,sizeof(sa_cli));
  int size_cli = sizeof(sa_cli);
  int n_fd=accept(l->l_fd,(struct sockaddr *)&sa_cli,(socklen_t *)&size_cli);
  if(n_fd<0){
    return NULL;
  }
  return newConnection(n_fd);
}


int64_t mm_read(Connection * c,char buf[], int64_t size){
  int64_t len, r_len;
  int64_t s;

  if ((len = read(c->fd,(char *) &s,sizeof(int64_t))) <= 0) {
     return 0;
  }
  char * tmp = malloc(s);
  r_len = read(c->fd,tmp,s);
  len = min(size, r_len);
  memcpy(buf,tmp,len);
  free(tmp);
  return len;
}

int64_t mm_write(Connection * c, const char * m,int64_t size){
  return write_msg(c->fd,m,size);
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

static int64_t write_msg(int w_fd,const char * m,int64_t size){
     int64_t ret;
     char * buf = calloc(1,sizeof(int64_t) + size), * b;
     b = buf;
     memcpy(b, &size, sizeof(int64_t));
     b+=sizeof(int64_t);
     memcpy(b, m, size);
     ret = write(w_fd,buf,sizeof(int64_t)+size);
     free(buf);
     return ret;
}

int mm_commtype(){
  return COMM_SOCK_INET;
}
