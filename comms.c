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
#define ADD_CONN_PCK 10
#define ACK_CONN_PCK 11
#define DATASIZE 1024

/*
** All the information required to
** process an incoming message is contained
** in this struct. The messages transmitted are
** of fixed size.
*/
struct message{
  int size;
  int type;
  char data[DATASIZE];
} ;

typedef struct message Message;

/*
** This struct contains the address of the
** fifo where the process should be contacted
** in order to establish a connection.
*/
struct address {
  const char * fifo;
} ;


struct listener {
     int l_fd;
};

struct connection {
  int w_fd;
  int r_fd;
};

static int write_msg(int w_fd,const char * m,int type,int size);
static Connection * newConnection(int w,int r);


Listener_p mm_listen(Address * addr){
     Listener * l = NULL;
     l = malloc(sizeof(Listener));
     if (addr == NULL || l == NULL || mkfifo(addr->fifo, S_IWUSR | S_IRUSR)) {
  	  free(l);
  	  return NULL;
     }
     l->l_fd=open(addr->fifo,O_RDWR);
     if (l->l_fd < 0) {
  	  free(l);
  	  /*rm ${addr->path} */
  	  l = NULL;
     }
     return l;
}

/* */
Connection * mm_connect(Listener_p l,Address * addr){
  if(addr == NULL){
    return NULL;
  }

  int w_fd=open(addr->fifo,O_WRONLY);
  Connection * c;
  int pid = getpid();
  char r_addr[20], w_addr[20];

  sprintf(r_addr,"/tmp/r%d",pid);
  sprintf(w_addr,"/tmp/w%d",pid);

  if(mkfifo(r_addr,S_IWUSR | S_IRUSR) || mkfifo(w_addr,S_IWUSR | S_IRUSR)){
    return NULL;
  }

  int r=open(r_addr,O_RDONLY | O_NONBLOCK);
  int size = strlen(r_addr)+strlen(w_addr)+2;

  char * msg = malloc(size);
  memcpy(msg,r_addr,strlen(r_addr)+1);
  memcpy(msg+strlen(r_addr)+1,w_addr,strlen(w_addr)+1);
  write_msg(w_fd,msg,ADD_CONN_PCK,size);
  int to=TIMEOUT;
  void * buf = malloc(sizeof(Message));
  while(to>0){
    if(read(r,(char *)buf,sizeof(Message))>0){
      Message * msg = (Message *) buf;
      if(msg->type==ACK_CONN_PCK){
	   int w=open(w_addr,O_WRONLY);
        c = newConnection(w,r);
        free(buf);
        return c;
      }
    }
    sleep(1);
    to-=1;
  }
  free(buf);
  close(w_fd);
  return NULL;

}

void mm_disconnect(Connection * c){
  int pid = getpid();
  char buf[20];
  snprintf(buf, 20, "%d", pid);
  write_msg(c->w_fd,buf,DELETE_CONN_PCK,strlen(buf)+1);
  close(c->w_fd);
  close(c->r_fd);
  free(c);
}

int mm_write(Connection * c,const char * m,int size){
  return write_msg(c->w_fd,m,NORMAL_PCK,size);
}


static int write_msg(int w_fd,const char * m,int type,int size){

  Message * msg = calloc(sizeof(Message),1);
  msg->size=size;
  msg->type=type;

  if(size>0){
    memcpy(msg->data,m,size);
  }
  write(w_fd,msg,sizeof(Message));
  free(msg);
  return size;
}

Connection * mm_accept(Listener_p l){
     int64_t len;
  void * msg = malloc(sizeof(Message));

  if ((len = read(l->l_fd,msg,sizeof(Message))) < 0) {
    return NULL;
  }
  Message * m = (Message *)msg;
  char * buf = malloc(m->size);
  memcpy(buf,m->data,m->size);
  int w_fd,i,r_fd;
  if(m->type == ADD_CONN_PCK){
       w_fd= open(buf,O_WRONLY);
      r_fd= open(buf+strlen(buf)+1,O_RDONLY|O_NONBLOCK);
      if (w_fd < 0 || r_fd < 0) {
	   return NULL;
      }
      Connection * c = newConnection(w_fd, r_fd);

      write_msg(c->w_fd,NULL,ACK_CONN_PCK,0);
      free(msg);
      free(buf);
      return c;
  }
  return NULL;
}

char * mm_read(Connection * c, int * size){
  void * msg = malloc(sizeof(Message));
  while(read(c->r_fd,msg,sizeof(Message))!=sizeof(Message));
  Message * m = (Message *)msg;
  char * buf = malloc(m->size);
  memcpy(buf,m->data,m->size);
  int i;
  if(m->type == DELETE_CONN_PCK){
      int o_pid=atoi(buf);
      int pid = getpid();
      char r_addr[20], w_addr[20];
      sprintf(r_addr,"/tmp/r%d",o_pid);
      sprintf(w_addr,"/tmp/w%d",o_pid);
      remove(r_addr);
      remove(w_addr);
      free(msg);
      free(buf);
      return  NULL;
  }
  else if ( m->type != NORMAL_PCK){
    return NULL;
  }
  *size=m->size;
  return buf;

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
