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


MsgH_p setup(char * fifo)
{
     MsgH_p msgh;
     msgh = calloc(1,sizeof(MsgHandle));
     if (!msgh){
	  return NULL;
     }
     msgh->addr = subscribe(fifo);
     if (!msgh->addr) {
	  free(msgh);
	  return NULL;
     }
     return msgh;
}

int64_t conn(MsgH_p msgh, char * path, int64_t t_sec, int64_t t_nsec, int64_t tries)
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
     addr = newAddress(path, 0);
     /* set up tries time out */
     t.tv_sec = t_sec;
     t.tv_nsec = t_nsec;

     /* try out connecion */
     c = connect(addr);
     while(!c && tries-- > 0) {
	  nanosleep(&t, NULL);
	  printf("try %d...\n",tr++);
	  c = connect(addr);
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

int64_t check_msg(MsgH_p msgh, void ** data_struct)
{
     Connection * c;
     int64_t size, opcode;
     void * msg_data = NULL;
     
     msg_data = listen(&c, &size);
     if (!msg_data) {
	  return -1;
     }
     //opcode = unmarsh(data_struct, msg_data);
     return opcode = 0;
}

/*
int64_t wait_conn(MsgH_p msgh, int64_t timeout) 
{
     return 0;
}
*/

int64_t wait_msg(MsgH_p msgh, enum Opcode opcode, void ** data_struct, int64_t t_sec, int64_t t_nsec, int64_t tries)
{
     int64_t auxop;
     void * aux_data;
     struct timespec t;

     t.tv_sec = t_sec;
     t.tv_nsec = t_nsec;
     
     auxop = check_msg(msgh, &aux_data);
     while (auxop != opcode && tries-->0) {
	  nanosleep(&t, NULL);
	  printf("waiting...\n");
	  auxop = check_msg(msgh, &aux_data);
     }
     if (auxop != opcode) {
	  return -1;
     }
     *data_struct = aux_data;
     return opcode;
}
