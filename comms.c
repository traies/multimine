#if __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 600
#else
#define _XOPEN_SOURCE 500
#endif /* __STDC_VERSION__ */


#include <comms.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#define TIMEOUT 5
#define NORMAL_PCK 8
#define DELETE_CONN_PCK 9
#define ACK_CONN_PCK 11
#define CONN_DATASIZE 256

/*
** All the information required to
** process an incoming message is contained
** in this struct. The messages transmitted are
** of fixed size.
*/
struct message{
  int64_t size;
  int64_t type;
} ;

struct connectionmessage {
  int64_t size;
  char data[CONN_DATASIZE];
} ;

typedef struct connectionmessage ConnectionMessage;
typedef struct message Message;

typedef struct listener {
     int l_fd;
     char * addr;
} Listener;

struct connection {
  int w_fd;
  int r_fd;
};

static int64_t write_msg(int w_fd,const int8_t * m,int64_t type,int64_t size);
static int64_t write_conn_msg(int w_fd,const char * m,int64_t size);
static Connection * newConnection(int w,int r);
static Listener * newListener(int fd,char * p);


Listener_p mm_listen(char * addr){
    int l_fd;
    if(addr==NULL){
      return NULL;
    }
    if(mkfifo(addr, S_IWUSR | S_IRUSR)!=0){
      return NULL;
    }
    l_fd=open(addr,O_RDWR);
    if (l_fd < 0) {
      remove(addr);
  	  return NULL;
    }
    return newListener(l_fd,addr);
}

Connection * mm_connect(char * addr){
  if(addr == NULL){
    return NULL;
  }
  struct timeval to;
  fd_set fds;
  int w_fd=open(addr,O_WRONLY),ret;
  Connection * c;
  int pid = getpid();
  char r_addr[20], w_addr[20];

  sprintf(r_addr,"/tmp/r%d",pid);
  sprintf(w_addr,"/tmp/w%d",pid);

  if(mkfifo(r_addr,S_IWUSR | S_IRUSR) || mkfifo(w_addr,S_IWUSR | S_IRUSR)){
    return NULL;
  }



  int size = strlen(r_addr)+strlen(w_addr)+2;

  char * msg = malloc(size);
  memcpy(msg,r_addr,strlen(r_addr)+1);
  memcpy(msg+strlen(r_addr)+1,w_addr,strlen(w_addr)+1);
  write_conn_msg(w_fd,msg,size);

  int r=open(r_addr,O_RDONLY);
  void * buf = malloc(sizeof(Message));
  int w=open(w_addr,O_WRONLY);

  to.tv_sec=TIMEOUT;
  to.tv_usec=0;
  FD_SET(r, &fds);
  ret = select(r + 1, &fds, NULL, NULL, &to);
  if (ret > 0) {
	   read(r,(char *)buf,sizeof(Message));
	   Message * msg = (Message *) buf;
       if(msg->type==ACK_CONN_PCK){
			puts("fifo\n");
			c = newConnection(w,r);
			free(buf);
			return c;
       }
  }
  free(buf);
  close(w_fd);
  return NULL;
}

void mm_disconnect(Connection * c){
  int pid = getpid();
  char buf[20];
  snprintf(buf, 20, "%d", pid);
  //write_msg(c->w_fd,buf,DELETE_CONN_PCK,strlen(buf)+1);
  close(c->w_fd);
  close(c->r_fd);
  free(c);
}

void mm_disconnect_listener(Listener * l) {
     close(l->l_fd);
     remove(l->addr);
     free(l->addr);
}

int64_t mm_write(Connection * c,const int8_t * m,int64_t size){
  return write_msg(c->w_fd,m,NORMAL_PCK,size);
}


static int64_t write_msg(int w_fd,const int8_t * m,int64_t type,int64_t size){

  Message msg;
  memset(&msg,0,sizeof(Message));
  msg.size=size;
  msg.type=type;
  write(w_fd,&msg,sizeof(Message));
  if(size>0){
    write(w_fd,m,size);
  }
  return size;
}

static int64_t write_conn_msg(int w_fd,const char * m,int64_t size){
  ConnectionMessage  msg ;
  memset(&msg,0,sizeof(ConnectionMessage));
  msg.size=size;
  if(size>0){
    memcpy(msg.data,m,size);
  }
  write(w_fd,&msg,sizeof(ConnectionMessage));
  return size;
}

Connection * mm_accept(Listener_p l){
  int64_t len;
  void * msg = malloc(sizeof(ConnectionMessage));
  if ((len = read(l->l_fd,msg,sizeof(ConnectionMessage))) < 0) {
    return NULL;
  }
  ConnectionMessage * m = (ConnectionMessage *)msg;
  char * buf = malloc(m->size);
  memcpy(buf,m->data,m->size);
  int w_fd,r_fd;
  w_fd= open(buf,O_WRONLY);
  r_fd= open(buf+strlen(buf)+1,O_RDONLY);
  if (w_fd < 0 || r_fd < 0) {
    return NULL;
  }
  Connection * c = newConnection(w_fd, r_fd);
  write_msg(c->w_fd,NULL,ACK_CONN_PCK,0);
  free(msg);
  free(buf);
  return c;
}

static int sel(int fd, struct timeval * timeout)
{
	fd_set r_set;
    int ret;
    FD_ZERO(&r_set);
    FD_SET(fd, &r_set);
    ret = select(fd + 1, &r_set, NULL, NULL, timeout);
    if (ret == 0) {
         return 0;
    }
    if (ret > 0) {
         return FD_ISSET(fd, &r_set);
    }
    return -1;
}

int mm_select(Connection * c, struct timeval * timeout)
{
     return sel(c->r_fd, timeout);
}

int mm_select_accept(Listener * lp, struct timeval * timeout)
{
     return sel(lp->l_fd, timeout);
}

int64_t mm_read(Connection * c, int8_t buf[], int64_t size)
{
    int64_t len, total_len = 0, r_len;
    Message m;

    if ((len = read(c->r_fd,(char *) &m,sizeof(Message))) == 0) {
	    return 0;
    }
    if (len < 0 || size < m.size) {
      return -1;
    }
    while (total_len < m.size) {
      r_len = read(c->r_fd, &buf[total_len], m.size - total_len);
      if (r_len == 0) {
        return 0;
      }
      if (r_len < 0) {
        return -1;
      }
      total_len += r_len;
    }

    /*
    if(m.type == DELETE_CONN_PCK){
  	  int o_pid=atoi(buf);
  	  int pid = getpid();
  	  char r_addr[20], w_addr[20];
  	  sprintf(r_addr,"/tmp/r%d",o_pid);
  	  sprintf(w_addr,"/tmp/w%d",o_pid);
  	  remove(r_addr);
  	  remove(w_addr);
  	  return  -1;
     }
     else if (m.type != NORMAL_PCK){
	      return -1;
     }*/
     return m.size;
}

static Connection * newConnection(int w,int r){
  Connection * ans = malloc(sizeof(Connection));
  if (ans == NULL) {
       return NULL;
  }
  ans->w_fd=w;
  ans->r_fd=r;
  return ans;
}

static Listener * newListener(int fd,char * p){
  Listener * ans = malloc(sizeof(Listener));
  ans->l_fd=fd;
  ans->addr=p;
  return ans;
}

int mm_commtype(){
  return COMM_FIFO;
}
