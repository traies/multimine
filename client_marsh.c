#include <stdint.h>
#include <msg_structs.h>
#include <comms.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_BUF_SIZE   2048
static int64_t query_marsh(char buf[], const QueryStruct * qs)
{
     if (qs == NULL) {
	  return -1;
     }
     buf[0] = (char) QUERYMINE;
     memcpy(&buf[1], &(qs->x), 1);
     memcpy(&buf[2], &(qs->y), 1);
     return sizeof(QueryStruct) + 1;
}

static int64_t init_unmarsh(InitStruct ** is, char buf[])
{
     *is = calloc(1, sizeof(InitStruct));
     if(!*is) {
	  return -1;
     }
     buf++;
     memcpy(&(*is)->rows, buf,sizeof(int64_t));
     buf+=sizeof(int64_t);
     memcpy(&(*is)->cols, buf,sizeof(int64_t));
     buf+=sizeof(int64_t);
     memcpy(&(*is)->mines, buf,sizeof(int64_t));
     buf+=sizeof(int64_t);
     (*is)->player_id = *buf++;
     (*is)->players = *buf++;
     return sizeof(InitStruct);
}
static int64_t update_unmarsh(char data_struct[], char buf[])
{
     UpdateStruct * us;
     int64_t len;
     data_struct[0] = buf[0];
     us = (UpdateStruct *) &data_struct[1];
     memcpy(&len, &buf[1], sizeof(int64_t));
     memcpy(&us->len, &buf[1], sizeof(int64_t));
     buf += sizeof(int64_t) + 1;
     us->players = *buf++;
     memcpy(&us->player_scores, buf, sizeof(int64_t[8]));
     buf += sizeof(int64_t[8]);
     for (int i = 0; i < len; i++) {
	  us->tiles[i].x = *buf++;
	  us->tiles[i].y = *buf++;
	  us->tiles[i].nearby = *buf++;
	  us->tiles[i].player = *buf++;
     }
     return sizeof(UpdateStruct) + len * 4;
}

static int64_t endgame_unmarsh(char data_struct[], char buf[])
{
     EndGameStruct * es;
     data_struct[0] = buf[0];
     es = (EndGameStruct *)&data_struct[1];
     es->players = buf[1];
     es->winner_id = buf[2];
     memcpy(es->player_scores, &buf[3], sizeof(int64_t[8]));
     return sizeof(EndGameStruct) + 1;
}

static int64_t send(Connection * c,  void * data, int64_t (*marsh)(void*, const void*))
{
     int len;
     static char buf[MAX_BUF_SIZE];
     if (data == NULL || c == NULL) {
	  return -1;
     }
     len = marsh(buf, data);
     return mm_write(c, buf, len);
}

int64_t send_query(Connection * c, QueryStruct * qs)
{
     return send(c, (void *) qs, (int64_t (*) (void *, const void *))query_marsh);
}

int8_t receive_init(Connection * c,InitStruct ** data_struct, struct timeval * timeout, int64_t tries)
{
     static char buf[MAX_BUF_SIZE];
     struct timeval taux;
     int64_t read, ret, i = 0;
     if (timeout != NULL) {
	  taux.tv_sec = timeout->tv_sec;
	  taux.tv_usec = timeout->tv_usec;
     }
     while (i < tries && (read = mm_select(c,timeout) ) <= 0) {
	  if (read == 0) {
	       
	       i++;
	  }
	  timeout->tv_sec = taux.tv_sec;
	  timeout->tv_usec= taux.tv_usec;
     }
     if (i < tries) {
	  ret = mm_read(c, buf, MAX_BUF_SIZE);
     }
     else {
	  return NOREAD;
     }
     if (ret == 0) {
	  return DISCONNECT;
     }
     if (buf[0] == INITGAME) {
	  if (init_unmarsh(data_struct, buf) > 0){
	       ret = INITGAME;
	  }
	  else {
	       ret = ERROR;
	  }
     }
     else {
	  ret = ERROR;
     }
     return ret;
}

int8_t receive_update(Connection * c, char * data_struct, int64_t size, struct timeval * timeout)
{
     static char * buf = NULL;
     static int buf_size = 0;
     int64_t read, ret;
     
     if (buf_size < size) {
	  free(buf);
	  buf = malloc(size);
	  if (buf == NULL) {
	       return ERROR;
	  }
	  buf_size = size;
     }
     if ((read = mm_select(c,timeout) ) > 0) {
	  ret = mm_read(c, buf, MAX_BUF_SIZE);
     }
     else {
	  return NOREAD;
     }
     if (ret == 0) {
	  return DISCONNECT;
     }
     if (buf[0] == UPDATEGAME) {
	  
	  if (update_unmarsh(data_struct, buf) > 0){
	       ret = UPDATEGAME;
	  }
	  else {
	       ret = ERROR;
	  }
     }
     else if (buf[0] == ENDGAME) {
	  if (endgame_unmarsh(data_struct, buf) > 0){
	       ret = ENDGAME;
	  }
	  else {
	       ret = ERROR;
	  }
     }
     else {
	  ret = ERROR;
     }
     return ret;
}
