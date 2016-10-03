#include <server.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <queue.h>
#include <comms.h>
#include <unistd.h>
#include <signal.h>
#include <marsh.h>
#include <pthread.h>
#include <mqueue.h>
#include <string.h>
#include "configurator.h"
#include <msg_structs.h>
#include <game.h>
#include <server_marsh.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <common.h>
#define NORMAL_PR 0
#define WARNING_PR 1000
#define ERR_PR sysconf(_SC_MQ_PRIO_MAX) - 1
#define MEMORY_ERROR -1
#define TRUE 1
#define FALSE 0
#define COLS 50
#define ROWS 20
#define MINES 10
#define BUF_SIZE 50000
#define DEFAULT_PLAYERS 1

typedef struct ClientPthreads
{
     pthread_t p_attr;
     pthread_t p_info;
     int r_fd;
     int w_fd;
} ClientPthreads;

int64_t update_scores(Minefield * m, UpdateStruct * us )
{
     int64_t player_scores[8];
     int64_t players;

     if (m == NULL || us == NULL) {
	  return -1;
     }
     players = get_scores(m, player_scores);
     if (players < 0) {
	  return -1;
     }
     memcpy(&us->player_scores, player_scores, sizeof(int64_t[8]));
     us->players = players;
     return 0;
}


typedef struct attender
{
     Connection * con;
     int w_fd, buf_size;
} Attender;

/* attendants threads run here */
void * attend(void * a)
{
     Attender * attr = (Attender *) a ;
     Connection * con = attr->con;
     int buf_size = attr->buf_size, w_fd = attr->w_fd, len;
     int8_t * buf;
     int sel;
     struct timeval timeout;

     buf = malloc (buf_size);
     if (!buf) {
             return NULL;
     }
     timeout.tv_sec = 1;
     timeout.tv_usec = 0;

     free(a);
     /* expects blocking read */
     while (true) {
	  if ((len = receive(con, buf, buf_size, &timeout)) > 0) {
	       write(w_fd, buf, len);
	  }
	  else if (len == -1) {
	       printf("disconnect \n");
	       break;
	  }
     }
     /* close fd to main server */
     close(w_fd);
     free(buf);
     pthread_exit(0);
}

typedef struct Informer
{
     Connection * con;
     int r_fd, buf_size;
} Informer;

/* informer threads run here */
void * inform(void * a)
{
     Informer * info = (Informer *) a;
     Connection * con = info->con;
     int r_fd = info->r_fd;
     int8_t buf[BUF_SIZE];
     int64_t len, buf_size = BUF_SIZE;
     fd_set fds;
     UpdateStruct * us;
     struct timeval timeout;
     timeout.tv_sec = 1;
     timeout.tv_usec = 0;

     /* expects blocking read */
     while(true) {
	  select(r_fd + 1, &fds, NULL, NULL, &timeout);
	  timeout.tv_sec = 1;
	  timeout.tv_usec = 0;
	  if (FD_ISSET(r_fd, &fds) && (len = read(r_fd, buf, 1)) > 0) {
	       if (buf[0] == INITGAME) {
		    read(r_fd, &buf[1], sizeof(InitStruct));
		    send_init(con, (InitStruct *) (buf + 1));
	       }
	       else if (buf[0] == UPDATEGAME) {
		    read(r_fd, &buf[1], sizeof(UpdateStruct));
		    us = (UpdateStruct *) &buf[1];
		    read(r_fd, &buf[sizeof(UpdateStruct) + 1], 4 * us->len);
		    send_update(con, (UpdateStruct *) (buf + 1));
	       }
	       else if (buf[0] == ENDGAME) {
		    read(r_fd, &buf[1], sizeof(EndGameStruct));
		    send_endgame(con, (EndGameStruct *) (buf + 1));
	       }
         else if (buf[0] == HIGHSCORES) {
           read(r_fd, &buf[1], sizeof(int));
          read(r_fd, &buf[1+sizeof(int)], (int)buf[1]*sizeof(Highscore));
          send_highscore(con, (Highscore *) (buf + 1+sizeof(int)));
        }
	  }
	  else if (FD_ISSET(r_fd, &fds) && len == 0) {
	       break;
	  }
	  else {
	       FD_SET(r_fd, &fds);
	  }
     }
     printf("closing informer\n");
     mm_disconnect(con);
     pthread_exit(0);
}



int64_t add_client(ClientPthreads * cli_arr [], fd_set * r_set, fd_set * w_set, int64_t * cli_i, int * attr_nfds, int * info_nfds, Connection * con, int attr_buf_size, int info_buf_size)
{
     ClientPthreads * cli_p;
     Attender * attender;
     Informer * informer;
     int attr_fd[2], info_fd[2];
     pthread_t pt_attr, pt_info;

     cli_p = malloc(sizeof(ClientPthreads));
     attender = malloc(sizeof(Attender));
     informer = malloc(sizeof(Informer));
     if (!informer || !attender || !cli_p) {
	  free(attender);
	  free(cli_p);
	  return -1;
     }
     attender->con = con;
     informer->con = con;
     pipe(attr_fd);
     pipe(info_fd);
     attender->w_fd = attr_fd[1];
     informer->r_fd = info_fd[0];
     attender->buf_size = attr_buf_size;
     informer->buf_size = info_buf_size;
     pthread_create(&pt_attr, NULL, attend, attender);
     pthread_create(&pt_info, NULL, inform, informer);
     cli_p->p_attr = pt_attr;
     cli_p->p_info = pt_info;
     cli_p->r_fd = attr_fd[0];
     cli_p->w_fd = info_fd[1];
     FD_SET(attr_fd[0], r_set);
     FD_SET(info_fd[1], w_set);
     cli_arr[*cli_i] = cli_p;
     *cli_i = *cli_i + 1;
     if (*attr_nfds < attr_fd[0] + 1) {
	  *attr_nfds = attr_fd[0] + 1;
     }
     if (*info_nfds < info_fd[1] + 1) {
	  *info_nfds = info_fd[1] + 1;
     }
     return 0;
}

int64_t attend_requests(Minefield * minef, int64_t msize, ClientPthreads * pths[8], int64_t players,mqd_t mqd)
{
     fd_set r_set;
     int8_t q, uflag = false, msg_type, endflag = false,h_flag=false,h_add_flag = false;
     int64_t nfds = 0, sflag, rlen, data_size, pleft ;
     UpdateStruct * us;
     QueryStruct * qs;
     EndGameStruct es;
     struct timeval timeout;
     char * data_struct;
     char * highscore_struct;
     int player;
     Highscore * h;
     pleft = players;
     char msg[100]=" ";
     /* init data buffer */
     data_size = sizeof(UpdateStruct) + msize * 4 + 1;
     data_struct = malloc(data_size);
      us = malloc(data_size);
     highscore_struct = malloc(sizeof(Highscore)*10 + 1 +sizeof(int));
     if (!data_struct || !us || !highscore_struct) {
	  free_minefield(minef);
	  return MEMORY_ERROR;
     }
     us->len = 0;

     /* set select timeout */
     timeout.tv_sec = 5;
     timeout.tv_usec = 0;
     /* fill in read file descriptor set */
     for (int i = 0; i < players; i++) {
	  if (nfds < pths[i]->r_fd) {
	       nfds = pths[i]->r_fd;
	  }
	  FD_SET(pths[i]->r_fd, &r_set);
     }
     nfds++;



     open_database();


     while(!q && pleft > 0)  {
	  /* select on pthreads read */
	  sflag = select(nfds, &r_set, NULL, NULL, &timeout);
	  /* reset timeout */
	  timeout.tv_sec = 5;
	  timeout.tv_usec = 0;

	  if (sflag == 0) {
	       /* timeout expired */
	       for (int i = 0; i < players; i++) {
		    if (pths[i] == NULL) {
			 continue;
		    }
		    FD_SET(pths[i]->r_fd, &r_set);
	       }
	       printf("listening...\n");
	  }
	  else if (sflag < 0) {
	       /* an error ocurred */
	       return -1;
	  }
	  else {
	       /* a fd is ready */
	       for (int i = 0; i < players; i++) {
		    /* skip closed connections */
		    if (pths[i] == NULL) {
			 continue;
		    }
		    if (FD_ISSET(pths[i]->r_fd, &r_set)) {
			 /* read first byte */
			 rlen = read(pths[i]->r_fd, data_struct, 1);
			 if (rlen > 0) {
			      /* request arrived */
			      if (data_struct[0] == QUERYMINE) {
				   /* read remaining data */
				   read(pths[i]->r_fd, &data_struct[1], sizeof(QueryStruct));
				   qs = (QueryStruct *) &data_struct[1];
				   sprintf(msg,"PLAYER %d OPENED x: %d, y: %d ", i, qs->x, qs->y);
                                    mq_send(mqd,msg,strlen(msg)+1,NORMAL_PR);
				   if (update_minefield(minef, qs->x, qs->y, i, us)) {
					uflag = true;
				   }
			      }
            else if (data_struct[0] == HIGHSCORES){
              player= i;
              h_flag = true;
            }  else if (data_struct[0] == HIGHSCORE_ADD){
              read(pths[i]->r_fd, &data_struct[1], sizeof(Highscore));
                h_add_flag = true;
              }
			 }
			 else if (rlen == 0) {
			      /* client disconnected */
                              sprintf(msg,"PLAYER %d DISCONECTED", i);
                               mq_send(mqd,msg,strlen(msg)+1,WARNING_PR);
			      reset_score(minef, i);
			      close(pths[i]->w_fd);
			      FD_CLR(pths[i]->r_fd, &r_set);
			      free(pths[i]);
			      pths[i] = NULL;
            pleft--;
			 }

		    }
		    else {
			 /* add fd to set */
			 FD_SET(pths[i]->r_fd, &r_set);
		    }
	       }
               if(!endflag){
	                      endflag = check_win_state(minef, &es);
                }
	       if (uflag) {
		    /* send updates to players */

		    uflag = false;
		    update_scores(minef, us);
		    msg_type = UPDATEGAME;
		    data_struct[0] = msg_type;
		    data_size = sizeof(UpdateStruct) + 4 * us->len;
		    memcpy(&data_struct[1], us, data_size);
		    for (int i = 0; i < players; i++) {
			 if (pths[i] == NULL) {
			      continue;
			 }
			 write(pths[i]->w_fd, data_struct, data_size + 1);
		    }
		    us->len = 0;
      }
         else if (h_flag){
        int count,size;
       h = get_highscores(&count);
        msg_type = HIGHSCORES;
        highscore_struct[0] = msg_type;
        highscore_struct[1] = count;
	size = sizeof(Highscore) *count;
        memcpy(&highscore_struct[1+sizeof(int)], h, size);
	  write(pths[player]->w_fd, highscore_struct, size + 1);
        h_flag = false;
      }else if (h_add_flag){
     h = (Highscore *)&data_struct[1];
     if(h[0].score > 0)
        insert_highscore(h[0].name,h[0].score);
       q = true;
   }

	       if (endflag && !h_add_flag) {
		    /* end game conditions where met */
		    msg_type = ENDGAME;
		    data_struct[0] = msg_type;
		    memcpy(&data_struct[1], &es, sizeof(EndGameStruct));

		    for (int i = 0; i < players; i++) {
			 if (pths[i] == NULL) {
			      continue;
			 }
			 write(pths[i]->w_fd, data_struct, sizeof(EndGameStruct) + 1);

		    }
	       }
	  }
     }
     free_minefield(minef);
     return 0;

}

int64_t host_game(ClientPthreads * pths[8], int64_t players, int64_t rows, int64_t cols, int64_t mines,mqd_t mqd)
{
     Minefield * minef;
     char * data_struct;
     int64_t data_size, ret;
     InitStruct * is;

     /* create minefield */
     minef = create_minefield(cols, rows, mines, players);
     if (minef == NULL) {
	  return MEMORY_ERROR;
     }
     /* init data buffer with max size */
     data_struct = malloc(sizeof(InitStruct) + 1);
     if (!data_struct) {
	  free_minefield(minef);
	  return MEMORY_ERROR;
     }

     /* init INITGAME message struct  */
     data_struct[0] = INITGAME;
     is = (InitStruct *) &data_struct[1];
     is->cols = cols;
     is->rows = rows;
     is->mines = mines;
     is->players = players;

     /* send game info to clients */
     for (int i = 0; i < players; i++) {
	  is->player_id = i;
	  write(pths[i]->w_fd, data_struct, sizeof(InitStruct) + 1);
     }

     /* free init data buffer */
     free(data_struct);

     /* attend requests */
     printf("c\n");
     ret = attend_requests(minef, rows * cols, pths , players,mqd);
     for (int i = 0; i < players; i++) {
	  if (pths[i] == NULL) {
	       continue;
	  }
	  free(pths[i]);
	  pths[i] = NULL;
     }
     return ret;
}


int main(int argc, char * argv[])
{
     char * srv_addr;
     char * srv_addr_mq;
     Connection * c = NULL;
     int64_t rows = ROWS, cols = COLS, mines = MINES;
     Minefield * minef;
     char fifo[20];
     int sflag, q = 0, attr_nfds = 0, info_nfds = 0, max_size = 1000;
     int64_t  us_size, es_size;
     fd_set r_set, w_set;
     struct timeval timeout;
     int count = 0;
     QueryStruct * qs;
     UpdateStruct  * us;
     EndGameStruct es;
     InitStruct * is;
     int players;
     ClientPthreads * pths[8];
     int64_t cli_i = 0;
     Listener * lp = NULL;
     int8_t endflag = FALSE, msg_type;


     if (argc > 1) {
	      sscanf(argv[1],"%d",&players);
        if(players > 7 || 0 >= players ){
          printf("%d no es un numero valido. Inserte un numero de jugadores del 1 al 7",players);
          return 1;
        }
     } else {
	      players = DEFAULT_PLAYERS;
     }
     us_size = cols * rows * 4 + sizeof(UpdateStruct);
     us = malloc(us_size);

     if (!us) {
	  printf("memory error.\n");
	  return -1;
     }
     us->len = 0;

     int8_t u_flag = 0;
     int8_t h_flag = 0;
     int player;
      Highscore * h;

     timeout.tv_sec = 10;
     timeout.tv_usec = 0;

     char * addr = configuration("config",mm_commtype(),3);

      lp = mm_listen(addr);
      if (lp == NULL) {
    printf("fracaso\n");
    return 0;
      }


     system("gnome-terminal -e ./bin/mq.out");


     c = mm_accept(lp) ;
       char msg[100]="";

       while(strcmp(msg,"got_connected") != 0){
       mm_read(c,msg,strlen("got_connected")+1);
     }

     mm_disconnect(c);
       mqd_t mqd = mq_open("/mq",O_WRONLY);

       mm_disconnect_listener(lp);



     /* open connection */
     addr = configuration("config",mm_commtype(),1);
     count=0;
     lp=mm_listen(addr);
     if(lp==NULL){
		   printf("fracaso\n");
		  return 0;
	   }

	   /* wait for connections */
	   while ( count < players && (c = mm_accept(lp)) != NULL) {
		/* established conection on c, needs to create thread */
		printf("conexion establecida. Creando thread.\n");
		add_client(pths, &r_set, &w_set, &cli_i, &attr_nfds, &info_nfds, c, sizeof(Highscore) + 1, us_size);
		printf("thread creado..\n");
		 mq_send(mqd,"GOT A CONNECTION",strlen("GOT A CONNECTION")+1,NORMAL_PR);
		count++;
	   }

	   mm_disconnect_listener(lp);

	   lp = NULL;

	   if (count < players) {
		/* connections failed */
  	   return -1;
	   }

	   host_game(pths, players, rows, cols, mines,mqd);

     return 0;
}
/*


     printf("game ended.\n");
     getchar();

     srv_exit(pths, cli_i);
     //minef = create_minefield(cols, rows, mines);

     /* game loop */
     // while (player > 1)
     //    read();
     //    update();
     //    print_log();
     //    send();



     /* fifo clean up */
     /* final */
     // close_connections();
     // free_minefield();
     // return;
//}
