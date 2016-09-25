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
#define min(a,b)  ((a) < (b))?(a):(b) 
#define TIMEOUT 5
#define NORMAL_PCK 8
#define DELETE_CONN_PCK 9
#define ADD_CONN_PCK 10
#define ACK_CONN_PCK 11
#define DATASIZE 4000

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

typedef struct Listener {
     int l_fd;
} Listener;

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
Connection * mm_connect(Address * addr){
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

  
  
  int size = strlen(r_addr)+strlen(w_addr)+2;

  char * msg = malloc(size);
  memcpy(msg,r_addr,strlen(r_addr)+1);
  memcpy(msg+strlen(r_addr)+1,w_addr,strlen(w_addr)+1);
  write_msg(w_fd,msg,ADD_CONN_PCK,size);
  int r=open(r_addr,O_RDONLY);
  int to=TIMEOUT;
  void * buf = malloc(sizeof(Message));
  while(to>0){
       int w=open(w_addr,O_WRONLY);
    if(read(r,(char *)buf,sizeof(Message))>0){
      Message * msg = (Message *) buf;
      if(msg->type==ACK_CONN_PCK){
	   
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
  
  return NULL;
}

int mm_select(Connection * c, struct timeval * timeout)
{
     fd_set r_set;
     if (c == NULL) {
	  return -1;
     }
     FD_SET(c->r_fd, &r_set);
     return select(c->r_fd + 1, &r_set, NULL, NULL, timeout);
}

int mm_read(Connection * c, char buf[], int size)
{
     int len;
     Message m;
     
     if ((len = read(c->r_fd,(char *) &m,sizeof(Message))) < 0) {
	  return -1;
     }
     len = min(size, m.size);
     memcpy(buf,m.data,len);
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
     else if ( m.type != NORMAL_PCK){
	  return -1;
     }
     return len;
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
