#include "comms.h"
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
#define INCREMENT 10
#define NORMAL_PCK 8
#define DELETE_CONN_PCK 9
#define ADD_CONN_PCK 10
#define ACK_CONN_PCK 11
#define DATASIZE 1024
/*
** All the information required to
** process an incoming message is contained
** in this struct.
*/
struct message{
  int pid;
  int size;
  int type;
  char data[DATASIZE];
} ;

typedef struct message Message;



/*
** This struct contains the address of the
** fifo where the process should be contacted,
*/
struct address {
  const char * fifo;
  int pid;
} ;

struct connection {
  int o_pid;
  int w_fd;
  int r_fd;
} ;
static int write_msg(Connection * c,const char * m,int type,int size,int check);
static int findConn(int pid);
static void increaseConns();
static void deleteConn(int conn_num);
static Connection * newConnection(int pid,int w,int r);

/*
** Generates a blank connection. Use this in listen()
** when a entrant connection is expected.
*/
static Connection * emptyConnection();

/*
** Static variables
*/
static Address * m_addr=NULL;
static Connection ** conns;
static int listen_fd = 0;
static int n_conns=0;
static int s_conns=0;


Address * subscribe(char * addr){
  if(m_addr!=NULL){
    return NULL;
  }
  increaseConns();
  int pid = getpid();

  if(mkfifo(addr,S_IWUSR | S_IRUSR)){
    return NULL;
  }
  listen_fd=open(addr,O_RDONLY|O_NONBLOCK);

  Address * a = newAddress(addr,pid);
  m_addr=a;
  return a;
}

Connection * connect(Address * addr,char * read_addr,char * write_addr){
  if(findConn(addr->pid) != -1 || addr == NULL || read_addr == NULL || write_addr == NULL){
    return NULL;
  }
  if(n_conns==s_conns){
    increaseConns();
  }
  int w_fd=open(addr->fifo,O_WRONLY);
  Connection * c=newConnection(addr->pid,w_fd,-1);

  char * r_addr= malloc(sizeof(char)*50);
  char * w_addr= malloc(sizeof(char)*50);
  sprintf(r_addr,"/tmp/%s",read_addr);
  sprintf(w_addr,"/tmp/%s",write_addr);
  r_addr = (char *)realloc(r_addr,strlen(r_addr)+1);
  w_addr = (char *)realloc(w_addr,strlen(w_addr)+1);
  if(mkfifo(r_addr,S_IWUSR | S_IRUSR) || mkfifo(w_addr,S_IWUSR | S_IRUSR)){
    return NULL;
  }

  int r=open(r_addr,O_RDONLY|O_NONBLOCK);


  int size = strlen(r_addr)+strlen(w_addr)+2;

  char * msg = malloc(size);
  memcpy(msg,r_addr,strlen(r_addr)+1);
  memcpy(msg+strlen(r_addr)+1,w_addr,strlen(w_addr)+1);
  write_msg(c,msg,ADD_CONN_PCK,size,0);

  int to=TIMEOUT;
  void * buf = malloc(sizeof(Message));
  while(to>0){
    if(read(r,(char *)buf,sizeof(Message))==sizeof(Message)){
      Message * msg = (Message *) buf;
      if(msg->type==ACK_CONN_PCK){
        int w=open(w_addr,O_WRONLY);
        c = newConnection(addr->pid,w,r);
        conns[n_conns++]= c;
        free(buf);
        return c;
      }
    }
    sleep(1);
    to-=1;
  }
  free(c);
  free(buf);
  close(w_fd);
  return NULL;

}

void disconnect(Connection * con){
  if(m_addr == NULL){
    return ;
  }

  int i;
  if((i=findConn(con->o_pid))==-1){
    return ;
  }

  write_msg(con,NULL,DELETE_CONN_PCK,0,1);
  deleteConn(i);
}

static int findConn(int pid){
  char found=0;
  int i=0;
  for(i=0;!found && i<n_conns;i++){
    if(conns[i]->o_pid == pid){
	return i;
    }
  }
	return -1;
}

int writem(Connection * c,const char * m,int size){
  return write_msg(c,m,NORMAL_PCK,size,1);
}


static int write_msg(Connection * c,const char * m,int type,int size,int check){

  if(check && findConn(c->o_pid)==-1){
    return 0;
  }

  Message * msg = calloc(sizeof(Message),1);
  msg->size=size;
  msg->type=type;
  msg->pid=m_addr->pid;
  if(size>0){
    memcpy(msg->data,m,size);
  }
  write(c->w_fd,msg,sizeof(Message));
  free(msg);
  return size;
}


Connection *listen(){

  void * msg = malloc(sizeof(Message));
  if (read(listen_fd,msg,sizeof(Message)) < 0) {
    return NULL;
  }
  Message * m = (Message *)msg;
  char * buf = malloc(m->size);
  memcpy(buf,m->data,m->size);
  int w_fd,i,r_fd;
  if(m->type == ADD_CONN_PCK){
      /*
      ** This means the connection established is new,
      ** therefore the content of the buff is the address
      ** of the source process.
      */
      if(n_conns==s_conns){
        increaseConns();
      }
      w_fd= open(buf,O_WRONLY|O_NONBLOCK);
      r_fd= open(buf+strlen(buf)+1,O_RDONLY|O_NONBLOCK);
      Connection * c = emptyConnection();
      c->o_pid=m->pid;
      c->w_fd=w_fd;
      c->r_fd=r_fd;
      conns[n_conns++]=c;
      write_msg(c,NULL,ACK_CONN_PCK,0,1);
      free(msg);free(buf);
      return c;
  }
  return NULL;
}

char * readm(Connection * c, int * size){
  void * msg = malloc(sizeof(Message));
  while(read(c->r_fd,msg,sizeof(Message))!=sizeof(Message));
  Message * m = (Message *)msg;
  char * buf = malloc(m->size);
  memcpy(buf,m->data,m->size);
  int i;
  if(m->type == DELETE_CONN_PCK){
      i = findConn(m->pid);
      deleteConn(i);
      free(msg);free(buf);
      return  NULL;
  }
  else if ( m->type != NORMAL_PCK){
    return NULL;
  }
  *size=m->size;
  return buf;

}



static void increaseConns(){
  conns=(Connection **)realloc(conns,
  sizeof(Connection *)*(s_conns+INCREMENT));
  s_conns+=INCREMENT;
}

 static void deleteConn(int i){
  Connection * c = conns[i];
  close(c->w_fd);
  free(c);
  for(;i < n_conns-1;i++){
    conns[i]=conns[i+1];
  }
  conns[n_conns]=NULL;
  n_conns--;
}

static Connection * emptyConnection(){
  return newConnection(0,0,0);
}

static Connection * newConnection(int pid,int w,int r){
  Connection * ans = malloc(sizeof(Connection));
  ans->o_pid=pid;
  ans->w_fd=w;
  ans->r_fd=r;
  return ans;
}

Address * newAddress(const char * f,int p){
  Address * ans = malloc(sizeof(Address));
  ans->fifo=f;
  ans->pid=p;
  return ans;
}
