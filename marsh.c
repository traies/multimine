#include <stdint.h>
#include <stdlib.h>
#include <comms.h>
#include <marsh.h>
#include <time.h>
#include <stdio.h>
#define MAX_CONNS      15

typedef struct MsgHandle
{
     Address * addr;
     int64_t conn_size;
     Connection * conns[MAX_CONNS];
} MsgHandle;

typedef struct SrvHandle
{
     Listener * listener;
} SrvHandle;

void * init_player_marsh(void * data_struct, int64_t * size)
{
     return NULL;
}

void * init_game_marsh(void * data_struct, int64_t * size)
{
     return NULL;
}


void * ( *opmarsher [OPCODE_SIZE])(void *, int64_t *) = {
     init_player_marsh,
     init_game_marsh
};

void * unmarsh(void * data_msg, int64_t size);


SrvHandle_p setup_srv(char * fifo)
{
     SrvHandle_p srvh;
     Address addr;
     addr->
     srvh = calloc(1,sizeof(SrvHandle));
     if (!srvh){
	  return NULL;
     }
     srvh->listener = mm_listen(fifo);
     if (!msgh->addr) {
	  free(msgh);
	  return NULL;
     }
     return msgh;
}

int64_t conn(MsgH_p msgh, char * srv_path, int64_t t_sec, int64_t t_nsec, int64_t tries)
{
     Connection * c = NULL;
     Address * addr;
     struct timespec t;
     int64_t i = 0, tr = 0;

     /* check if conns are available */
     if (msgh->conn_size >= MAX_CONNS) {
	  return -1;
     }

     /* set up address */
     addr = newAddress(srv_path, 0);
     /* set up tries time out */
     t.tv_sec = t_sec;
     t.tv_nsec = t_nsec;

     /* try out connecion */
     c = connect(addr, r_path, w_path);
     while(!c && tries-- > 0) {
	  nanosleep(&t, NULL);
	  printf("try %d...\n",tr++);
	  c = connect(addr, r_path, w_path);
     }
     if (!c) {
	  return -1;
     }
     /* save conn and return connid */
     i = msgh->conn_size++;
     msgh->conns[i] = c;
     return i;
}

int64_t send(MsgH_p msgh, int64_t connid, int64_t opcode, void * data_struct)
{
     int64_t size = 0;
     void * msg_data;

     /* check opcode and connid */
     if (opcode < 0 || opcode >= OPCODE_SIZE || connid < 0 || connid >= msgh->conn_size) {
	  return -1;
     }
     /* marsh data struct */
     msg_data = opmarsher[opcode](data_struct, &size);
     if ( size < 0) {
	  return -1;
     }
     /* write msg */
     writem(msgh->conns[connid], msg_data, size);
     return 0;
}

// TODO
/*
int64_t wait_ack(MsgH_p msgh, int64_t mid, int64_t timeout)
{

     return 0;
}
*/

int64_t check_msg(MsgH_p msgh,int64_t cid ,void ** data_struct)
{
    if(cid < 0 || cid >= msgh->conn_size)
      return -1;
     Connection * c = msgh->conns[cid];
     int64_t size, opcode;
     void * msg_data = NULL;
     msg_data = readm(c, &size);
     if (!msg_data) {
	  return -1;
     }
     //opcode = unmarsh(data_struct, msg_data);
     return opcode = 0;
}


int64_t wait_conn(MsgH_p msgh, int64_t t_sec, int64_t t_nsec, int64_t tries)
{
  Connection * c = NULL;
  int64_t i;
  struct timespec t;
  t.tv_sec = t_sec;
  t.tv_nsec = t_nsec;

  c = listen();
  while (!c && tries-->0) {
    printf("listening...\n");
    nanosleep(&t, NULL);
    c = listen();
  }
  if (!c) {
    return -1;
  }
  i = msgh->conn_size++;
  msgh->conns[i] = c;
  return i;
}


int64_t wait_msg(MsgH_p msgh, int64_t cid, enum Opcode opcode, void ** data_struct, int64_t t_sec, int64_t t_nsec, int64_t tries)
{
     int64_t auxop;
     void * aux_data;
     struct timespec t;

     t.tv_sec = t_sec;
     t.tv_nsec = t_nsec;

     auxop = check_msg(msgh, cid, &aux_data);
     while (auxop != opcode && tries-->0) {
	  nanosleep(&t, NULL);
	  printf("waiting...\n");
	  auxop = check_msg(msgh, cid, &aux_data);
     }
     if (auxop != opcode) {
	  return -1;
     }
     *data_struct = aux_data;
     return opcode;
}
