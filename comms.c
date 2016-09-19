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

#define INCREMENT 10
#define NORMAL_PCK 8
#define DELETE_CONN_PCK 9
#define ADD_CONN_PCK 10

/*
** All the information required to
** process an incoming message is contained
** in this struct.
*/
struct message{
  int pid;
  int size;
  int type;
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
} ;
static int write_msg(Connection * c,const char * m,int type,int size);
static int findConn(int pid);
static void increaseConns();
static void deleteConn(int conn_num);
static Connection * newConnection(int pid,int w);

/*
** Static variables
*/
static Address * m_addr=NULL;
static int r_fd=0;
static Connection ** conns;
static int n_conns=0;
static int s_conns=0;


Address * subscribe(const char * addr){
  if(m_addr!=NULL){
    return NULL;
  }
  increaseConns();
  int pid = getpid(), err;
  char * tmp= malloc(sizeof(char)*50);
  if(addr==NULL){
    
    sprintf(tmp,"tmp/juancito");
    tmp = (char *)realloc(tmp,strlen(tmp)+1);
    addr=tmp;
  }
  if(err = mkfifo(addr,S_IWUSR | S_IRUSR)){
       printf("subscribe error %d\n", err);
    return NULL;
  }
  r_fd=open(addr,O_RDONLY|O_NONBLOCK);
  Address * a = newAddress(addr,pid);
  m_addr=a;
  return a;
}

Connection * connect(Address * addr){
  if(m_addr==NULL || findConn(addr->pid) != -1){
    return NULL;
  }
  if(n_conns==s_conns){
    increaseConns();
  }
  int w_fd=open(addr->fifo,O_WRONLY);
  if (w_fd < 0) {
       return NULL;
  }
  Connection * c=newConnection(addr->pid,w_fd);
  conns[n_conns++]=c;
  write_msg(c,m_addr->fifo,ADD_CONN_PCK,strlen(m_addr->fifo));
  printf("a\n");
  return c;

}

void disconnect(Connection * con){
  if(m_addr == NULL){
    return ;
  }

  int i;
  if((i=findConn(con->o_pid))==-1){
    return ;
  }

  write_msg(con,NULL,DELETE_CONN_PCK,0);
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
  return write_msg(c,m,NORMAL_PCK,size);
}


static int write_msg(Connection * c,const char * m,int type,int size){

  if(findConn(c->o_pid)==-1){
    return 0;
  }

  int t_size= size + 3*sizeof(int);
  char * msg = malloc(t_size);
  memcpy(msg,&(m_addr->pid),sizeof(int));
  memcpy(&msg[sizeof(int)],&size,sizeof(int));
  memcpy(&msg[2*sizeof(int)],&type,sizeof(int));
  if(size>0){
    memcpy(&msg[3*sizeof(int)],m,size);
  }
  write(c->w_fd,msg,t_size);
  free(msg);
  return size;
}


char * listen(Connection ** c,int * size){
  void * msg = malloc(sizeof(Message));
  Message * m = (Message *)msg;
  char * buf = malloc(m->size);
  read(r_fd,buf,m->size);
  Address * o_a; int w_fd;
  int i ;
  if(m->type == ADD_CONN_PCK){
      /*
      ** This means the connection established is new,
      ** therefore the content of the buff is the address
      ** of the source process.
      */
      if(n_conns==s_conns){
        increaseConns();
      }
      w_fd= open(buf,O_WRONLY);
      *c = emptyConnection();
      (*c)->o_pid=m->pid;
      (*c)->w_fd=w_fd;
      conns[n_conns++]= *c;
      printf("algo\n");
      return NULL;
  } else if(m->type == DELETE_CONN_PCK){
      i = findConn(m->pid);
      deleteConn(i);
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

Connection * emptyConnection(){
  return newConnection(0,0);
}

static Connection * newConnection(int pid,int w){
  Connection * ans = malloc(sizeof(Connection));
  ans->o_pid=pid;
  ans->w_fd=w;
  return ans;
}

Address * newAddress(const char * f,int p){
  Address * ans = malloc(sizeof(Address));
  ans->fifo=f;
  ans->pid=p;
  return ans;
}
