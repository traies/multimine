#include <stdint.h>
#include <msg_structs.h>
#include <comms.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define MAX_BUF_SIZE 20000

static int64_t init_marsh(char buf[], const InitStruct * is)
{
     int64_t a;
     if (is == NULL) {
	  return -1;
     }
     *buf++ = INITGAME;
     memcpy(buf, &(is->rows), sizeof(int64_t));
     memcpy(&a,buf,sizeof(int64_t));
     buf += sizeof(int64_t);
     memcpy(buf, &(is->cols), sizeof(int64_t));
     buf += sizeof(int64_t);
     memcpy(buf, &(is->mines), sizeof(int64_t));
     buf += sizeof(int64_t);
     *buf++ = is->player_id;
     *buf++ = is->players;
     return sizeof(InitStruct) + 1;
}

static int64_t update_marsh(char buf[], const UpdateStruct * us)
{
     int64_t size = 1 + us->len * 4 + sizeof(UpdateStruct);
     char * base = buf;
     if (MAX_BUF_SIZE < size) {
	  return -1;
     }
     *buf++ = UPDATEGAME;
     memcpy(buf, (char *) &us->len, sizeof(int64_t));
     buf += sizeof(int64_t);
     *buf++ = us->players;
     memcpy(buf,us->player_scores, sizeof(int64_t[8]));
     buf += sizeof(int64_t[8]);
     for (int i = 0; i < us->len; i++) {
	  *buf++ = us->tiles[i].x;
	  *buf++ = us->tiles[i].y;
	  *buf++ = us->tiles[i].nearby;
	  *buf++ = us->tiles[i].player;
     }
     return size;
}

static int64_t endgame_marsh(char buf[], const EndGameStruct * es)
{
     *buf++ = ENDGAME;
     *buf++ = es->players;
     *buf++ = es->winner_id;
     memcpy(buf, es->player_scores, sizeof(int64_t[8]));
     return 1 + sizeof(EndGameStruct);
}

static int64_t highscore_marsh(char buf[], const Highscore * h)
{
     *buf++ = HIGHSCORES;
     int i = 0;
     while(strcmp(h[i].name,"")!=0){
       memcpy(buf, &h[i++], sizeof(Highscore));
       buf+= sizeof(Highscore);
     }
     return 1 + sizeof(Highscore)*i;
}

static int64_t send(Connection * c, void * data, int64_t (*marsh)(void*, const void*))
{
     int len;
     static char buf[MAX_BUF_SIZE];
     if (data == NULL || c == NULL) {
	  return -1;
     }
     len = marsh(buf, data);
     return mm_write(c, buf, len);
}

int64_t send_init(Connection * c,  InitStruct * is)
{
     return send(c, (void *) is, (int64_t (*) (void *, const void *))init_marsh);
}

int64_t send_update(Connection * c, UpdateStruct * us)
{
     return send(c,  (void *) us, (int64_t (*) (void *, const void *))update_marsh);
}

int64_t send_endgame(Connection * c, EndGameStruct * es)
{
     return send(c, (void *) es, (int64_t (*) (void *, const void *)) endgame_marsh);
}
int64_t send_highscore(Connection * c,Highscore * h)
{
     return send(c, (void *) h, (int64_t (*) (void *, const void *)) highscore_marsh);
}

int64_t query_unmarsh(char data_struct[], char buf[])
{
     QueryStruct * qs = (QueryStruct *) &data_struct[1];
     data_struct[0] = buf[0];
     memcpy(&qs->x, &buf[1], 1);
     memcpy(&qs->y, &buf[2], 1);
     return sizeof(QueryStruct) + 1;
}
int64_t highscore_unmarsh(char data_struct[], char buf[])
{
     Highscore * h = (Highscore*) &data_struct[1];
     data_struct[0] = buf[0];
     printf("%s\n",buf+1 );
     memcpy(&h->name, &buf[1], 20);
     memcpy(&h->score, &buf[21], sizeof(int));
     return sizeof(Highscore) + 1;
}

int8_t receive(Connection * c, char * data_struct, int64_t size, struct timeval * timeout)
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
     if ( (read = mm_select(c,timeout) ) > 0) {
	  ret = mm_read(c, buf, buf_size);
     }
     else {
	  return 0;
     }
     if (ret == 0) {
	  return -1;
     }
     if (buf[0] == QUERYMINE) {
	  ret = query_unmarsh(data_struct, buf);
  }else if(buf[0] == HIGHSCORES){
    data_struct[0]= HIGHSCORES;
    ret = 1;
  }else if(buf[0] == HIGHSCORE_ADD){
    ret = highscore_unmarsh(data_struct,buf);
  }
     else {
	  ret = 0;
     }
     return ret;
}
